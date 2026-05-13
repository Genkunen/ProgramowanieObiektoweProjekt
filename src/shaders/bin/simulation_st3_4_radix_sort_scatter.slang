import "../shared_code_shim";

struct PushConstants {
    uint* sort_keys;
    uint* sort_values;
    uint* dst_sort_keys;
    uint* dst_sort_values;

    uint* global_histogram;       // table in the format [radix_bucket]
    uint* group_local_histograms; // 2D table in the format [radix_bucket][group_id]

    uint group_count;
    uint sort_key_count;
    uint radix_bit_shift;
};

[vk::push_constant] PushConstants push_constants;


uint div_ceil(uint a, uint b) {
    return (a + b - 1) / b;
}

groupshared uint gs_rank_elem_values[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS];

uint compute_element_rank256(uint value, uint group_tid) {
    gs_rank_elem_values[group_tid] = value;
    GroupMemoryBarrierWithGroupSync();

    uint rank = 0;

    for (uint i = 0; i < group_tid; i++) {
        if (gs_rank_elem_values[i] == value) rank++;
    }
    return rank;
}

uint countbits128(uint4 val) {
    return countbits(val.x) + countbits(val.y) + countbits(val.z) + countbits(val.w);
}

uint wave_lt_lanecount_with_same_value(uint value) {
    uint4 mask_before = WaveGetLaneLtMask();

    while (true) {
        bool match_condition = WaveReadLaneFirst(value) == value;
        uint4 matching = WaveActiveBallot(match_condition);

        if (match_condition) {
            return countbits128(mask_before & matching);
        }
    }
    return 0;
}

uint wave_count_pattern_matches(uint pattern, uint value) {
    uint match_count = 0;
    uint hardware_wave_lane_count = WaveGetLaneCount();

    // TODO: maybe there is a smart ballot that can do this without a per-lane loop
    for (uint i = 0; i < hardware_wave_lane_count; i++) {
        uint other_value = WaveReadLaneAt(value, i);
        if (other_value == pattern) match_count++;
    }
    return match_count;
}

uint compute_element_rank256v2(uint value, uint group_tid, uint group_id, uint hardware_wave_lane_count, uint wave_index, uint wave_lane) {
    gs_rank_elem_values[group_tid] = value;

    uint intrawave_matches = wave_lt_lanecount_with_same_value(value);
    GroupMemoryBarrierWithGroupSync();

    uint iters = wave_index * hardware_wave_lane_count;
    uint foreign_wave_matches = 0;
    for (uint i = 0; i < wave_index; i++) {
        uint other_value = gs_rank_elem_values[i * hardware_wave_lane_count + wave_lane];
        foreign_wave_matches += wave_count_pattern_matches(value, other_value);
    }

    return intrawave_matches + foreign_wave_matches;
}

groupshared uint gs_elem_write_offset_bases[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    uint hardware_wave_lane_count = WaveGetLaneCount();
    uint wave_lane = WaveGetLaneIndex();

    uint wave_count = div_ceil(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, hardware_wave_lane_count);
    uint wave_index = group_tid / hardware_wave_lane_count;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // First load the local group prefix sums and add the global ones on top.

    uint bin_write_offset = push_constants.group_local_histograms[group_tid * push_constants.group_count + group_id];
    uint global_write_offset_base = push_constants.global_histogram[group_tid];
    gs_elem_write_offset_bases[group_tid] = global_write_offset_base + bin_write_offset;

    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Load all the elements belonging to the thread.

    // base_index - index to the first key in the chunk of keys that this group is responsible for.
    uint base_index = group_id * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_GROUP;

    uint keys[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD];
    uint values[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD];

    uint thread_kv_count = 0;

    for (uint i = 0; i < pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD; i++) {
        // This index formula gives a thread-interleaved access pattern, i.e. if we had 4 threads A,B,C,D working in the group, taking many elements
        // at a time, then the access pattern is to the chunk is ABCDABCDABCDABCD...
        //
        // Here it's especially important, because it makes it much easier to compute the element ranks if the order of those in hardware registers
        // is the same as [wave_index][wave_lane].
        uint index = base_index
            + group_tid
            + i * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS;

        if (index >= push_constants.sort_key_count) break;

        keys[i] = push_constants.sort_keys[index];
        values[i] = push_constants.sort_values[index];
        thread_kv_count++;
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Rank elements across the group and compute the element write offsets.

    uint kv_write_indices[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD];

    for (uint i = 0; i < thread_kv_count; i++) {
        uint key = keys[i];

        uint digit = (key >> push_constants.radix_bit_shift) & pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_RADIX_MASK;
        uint rank = compute_element_rank256v2(digit, group_tid, group_id, hardware_wave_lane_count, wave_index, wave_lane);

        uint dst_local_write_offset = gs_elem_write_offset_bases[digit] + rank;
        kv_write_indices[i] = dst_local_write_offset;

        GroupMemoryBarrierWithGroupSync();
        InterlockedAdd(gs_elem_write_offset_bases[digit], 1u);
        // synchronized by barrier in compute_element_rank256v2
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Using a similar execution model to that of the histogram pass, now we can scatter the elements to new positions.

    for (uint i = 0; i < thread_kv_count; i++) {
        uint key = keys[i];
        uint val = values[i];
        uint dst_local_write_offset = kv_write_indices[i];

        push_constants.dst_sort_keys[dst_local_write_offset] = key;
        push_constants.dst_sort_values[dst_local_write_offset] = val;
    }
}