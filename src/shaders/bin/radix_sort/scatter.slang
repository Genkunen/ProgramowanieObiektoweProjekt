import "../../shared_code_shim";

[vk::constant_id(0)] const uint GROUP_SIZE = 64;

[vk::push_constant] pop::shaders::SimulationAccelerationGridRadixSortScatterCSPushConstants push_constants;

uint div_ceil(uint a, uint b) {
    return (a + b - 1) / b;
}

uint countbits128(uint4 val) {
    return countbits(val.x) + countbits(val.y) + countbits(val.z) + countbits(val.w);
}

uint compute_element_intrawave_rank(uint value) {
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

groupshared uint gs_elem_write_offset_bases[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS];

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    uint hardware_wave_lane_count = WaveGetLaneCount();
    uint wave_lane = WaveGetLaneIndex();

    uint wave_count = div_ceil(GROUP_SIZE, hardware_wave_lane_count);
    uint wave_index = group_tid / hardware_wave_lane_count;

    uint wb_iterations = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / GROUP_SIZE;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // First load the local group prefix sums and add the global ones on top.

    for (uint i = 0; i < wb_iterations; i++) {
        uint expanded_gtid = group_tid + i * GROUP_SIZE;
        uint bin_write_offset = push_constants.group_local_histograms[expanded_gtid * push_constants.group_count + group_id];
        uint global_write_offset_base = push_constants.global_histogram[expanded_gtid];
        gs_elem_write_offset_bases[expanded_gtid] = global_write_offset_base + bin_write_offset;
    }
    GroupMemoryBarrierWithWaveSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Load all the elements belonging to the thread.

    // base_index - index to the first key in the chunk of keys that this group is responsible for.
    uint base_index = group_id * GROUP_SIZE * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD;

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
            + i * GROUP_SIZE;

        if (index >= push_constants.keys_count) break;

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
        uint rank = compute_element_intrawave_rank(digit);

        uint dst_local_write_offset = gs_elem_write_offset_bases[digit] + rank;
        kv_write_indices[i] = dst_local_write_offset;

        GroupMemoryBarrierWithWaveSync();
        InterlockedAdd(gs_elem_write_offset_bases[digit], 1u);
        GroupMemoryBarrierWithWaveSync();
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