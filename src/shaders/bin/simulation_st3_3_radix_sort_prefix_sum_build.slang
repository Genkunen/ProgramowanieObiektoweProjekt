import "../shared_code_shim";

struct PushConstants {
    uint* global_histogram;
    uint* group_local_histograms;

    uint group_count;
};

[vk::push_constant] PushConstants push_constants;

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

uint div_ceil(uint a, uint b) {
    return (a + b - 1) / b;
}

// Hardware has different wave sizes, but it is not possible to reliably dynamically size arrays in LDS to depend on the hardware wave size.
// Therefore we have to make a guess. Typically we see wave8/wave16 on Intel Arc, wave32 on NVIDIA (and in some cases AMD), and wave64 on AMD.
// I read somewhere that wave4 is a thing so I'm going to assume that's the minimum.
static const uint ASSUMED_MIN_WAVE_SIZE = 4;

static const uint MAX_WAVE_TOTAL_SUMS = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / ASSUMED_MIN_WAVE_SIZE;

groupshared uint gs_column_prefix_sums_last_value;
groupshared uint gs_intrawave_total_sums[MAX_WAVE_TOTAL_SUMS];

uint compute_interwave_prefix_sum(uint val, uint hardware_wave_lane_count, uint group_wave_count, uint wave_index, uint wave_lane) {
    bool is_first_wave = wave_index == 0;
    bool is_last_wave_lane = wave_lane == hardware_wave_lane_count - 1;

    // First perform an intra-wave prefix sum.
    // intrawave_prefix_sum becomes the sum of lane_hist_value of lanes [0, wave_lane - 1] in the wave.
    uint intrawave_prefix_sum = WavePrefixSum(val);

    // Have the last lane in a wave write the the total sum of val in that wave to the total sums table in LDS.
    if (is_last_wave_lane) {
        uint intrawave_total_sum = intrawave_prefix_sum + val;
        gs_intrawave_total_sums[wave_index] = intrawave_total_sum;
    }
    GroupMemoryBarrierWithGroupSync();

    // Have the first wave in the group transform the intrawave total sums into a prefix sum of it.
    if (is_first_wave) {
        // TODO: support GPUs with execution mode narrower than wave16
        bool is_participating_lane = wave_lane < group_wave_count;

        uint value = is_participating_lane ? gs_intrawave_total_sums[wave_lane] : 0;
        uint value_prefix_sum = WavePrefixSum(value);

        if (is_participating_lane) gs_intrawave_total_sums[wave_lane] = value_prefix_sum;
    }
    GroupMemoryBarrierWithGroupSync();

    // All waves read back the prefix sum at their respective wave index and add it to their prefix sums to obtain an interwave prefix sum across all
    // waves in the group.
    return intrawave_prefix_sum + gs_intrawave_total_sums[wave_index];
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    uint hardware_wave_lane_count = WaveGetLaneCount();
    uint wave_lane = WaveGetLaneIndex();

    uint wave_count = div_ceil(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, hardware_wave_lane_count);
    uint wave_index = group_tid / hardware_wave_lane_count;

    bool is_last_thread_in_group = group_tid == pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS - 1;

    if (is_last_thread_in_group) gs_column_prefix_sums_last_value = 0;
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Each group transforms one radix column in the local histograms table into prefix sums of it.

    // histogram_bin_base_index - base index for the bins array in the local histograms table
    uint histogram_bin_base_index = group_id * push_constants.group_count;

    // The loop processes histogram bins of 256 groups at a time.
    uint group_batch_count = div_ceil(push_constants.group_count, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS);
    for (uint group_batch_index = 0; group_batch_index < group_batch_count; group_batch_index++) {
        bool is_last_group_batch = group_batch_index == group_batch_count - 1;

        // bin_group_index - index of the group within a bin array in the local histograms table
        uint bin_group_index = group_batch_index * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS + group_tid;

        bool bin_group_index_is_valid = bin_group_index < push_constants.group_count;

        // local_histograms_index - canonical index of the bin of a group within the local histograms table
        uint local_histograms_index = histogram_bin_base_index + bin_group_index;

        uint bin_value = bin_group_index_is_valid ? push_constants.group_local_histograms[local_histograms_index] : 0;

        uint prefix_sum = compute_interwave_prefix_sum(bin_value, hardware_wave_lane_count, wave_count, wave_index, wave_lane) + gs_column_prefix_sums_last_value;

        // Write back the prefix sum with the previous total sum added to the local histograms.
        if (bin_group_index_is_valid) push_constants.group_local_histograms[local_histograms_index] = prefix_sum;

        // If there is a next iteration, it will need the total sum after this iteration.
        // And if there is a next iteration, it means that all threads in this loop were occupied, which means that the last thread can always write the total sum.
        if (!is_last_group_batch) {
            GroupMemoryBarrierWithGroupSync();
            if (is_last_thread_in_group) gs_column_prefix_sums_last_value = prefix_sum + bin_value;
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (group_id != 0) return;

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // After group 0 is done with computing the prefix sums within local histograms bin 0, it also computes the prefix sums of the global histogram.

    uint global_histogram_value = push_constants.global_histogram[group_tid];
    uint global_histogram_prefix_sum = compute_interwave_prefix_sum(global_histogram_value, hardware_wave_lane_count, wave_count, wave_index, wave_lane);

    push_constants.global_histogram[group_tid] = global_histogram_prefix_sum;
}