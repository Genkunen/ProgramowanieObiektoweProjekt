import "../../shared_code_shim";
import "../../math/div_ceil";
import "../../math/interwave_prefix_sum";

[vk::constant_id(0)] const uint SET_WAVE_SIZE = 4; // pessimistic assumption if not set

[vk::push_constant] pop::shaders::SimulationAccelerationGridRadixSortPrefixSumCSPushConstants push_constants;

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

uint div_ceil(uint a, uint b) {
    return (a + b - 1) / b;
}

static const uint MAX_WAVE_TOTAL_SUMS = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / SET_WAVE_SIZE;

groupshared uint gs_column_prefix_sums_last_value;
groupshared uint gs_intrawave_prefix_sum_scratch[MAX_WAVE_TOTAL_SUMS];

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

        uint prefix_sum = compute_interwave_prefix_sum(bin_value, hardware_wave_lane_count, wave_count, wave_index, wave_lane, gs_intrawave_prefix_sum_scratch) + gs_column_prefix_sums_last_value;

        // Write back the prefix sum with the previous total sum added to the local histograms.
        if (bin_group_index_is_valid) push_constants.group_local_histograms[local_histograms_index] = prefix_sum;

        // If there is a next iteration, it will need the total sum after this iteration.
        // And if there is a next iteration, it means that all threads in this loop were occupied, which means that the last thread can always write the total sum.
        if (!is_last_group_batch) {
            GroupMemoryBarrierWithGroupSync();
            if (is_last_thread_in_group) gs_column_prefix_sums_last_value = prefix_sum + bin_value;
            GroupMemoryBarrierWithGroupSync();
        } else {
            // Last active thread in the group writes the total sum to the respective global histogram index
            bool is_last_active_thread = bin_group_index == (push_constants.group_count - 1);

            if (is_last_active_thread) push_constants.global_histogram[group_id] = prefix_sum + bin_value;
        }
    }
}