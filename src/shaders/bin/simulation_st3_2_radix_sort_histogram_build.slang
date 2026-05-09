#include "../shared_consts.hpp"

import "../radix_sort/histogram";

struct PushConstants {
    uint* sort_keys;
    RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS>* chunk_histograms;
    RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS>* global_histogram;
    uint sort_keys_count;
    uint radix_bit_shift;
};

[vk::push_constant] PushConstants push_constants;

groupshared RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS> gs_chunk_histogram;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    bool is_working_thread = tid < push_constants.sort_keys_count;
    bool is_group_helper_thread = group_tid < pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS;

    // Zero out this group's histogram.
    if (is_group_helper_thread) {
        gs_chunk_histogram.buckets[group_tid] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Build a histogram of radixes for this chunk.
    if (is_working_thread) {
        uint element = push_constants.sort_keys[tid];

        uint digit = (element >> push_constants.radix_bit_shift) & pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_RADIX_MASK;

        InterlockedAdd(gs_chunk_histogram.buckets[digit], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    // Write out this group's histogram to global memory, and add to the global histogram.
    // The per-chunk histograms are used by the scatter pass, the global histogram is used by the prefix sum pass.
    if (is_group_helper_thread) {
        push_constants.chunk_histograms[group_id].buckets[group_tid] = gs_chunk_histogram.buckets[group_tid];
        InterlockedAdd(push_constants.global_histogram->buckets[group_tid], gs_chunk_histogram.buckets[group_tid]);
    }
}