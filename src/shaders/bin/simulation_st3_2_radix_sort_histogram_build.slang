#include "../shared_consts.hpp"

import "../radix_sort/histogram";

struct PushConstants {
    uint* sort_keys;
    RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS>* global_histogram;
    uint sort_keys_count;
    uint radix_bit_shift;
};

[vk::push_constant] PushConstants push_constants;

groupshared RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS> gs_histogram;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    bool is_working_thread = tid < push_constants.sort_keys_count;
    bool is_group_helper_thread = group_tid < pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS;

    if (is_group_helper_thread) {
        gs_histogram.buckets[group_tid] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (is_working_thread) {
        uint element = push_constants.sort_keys[tid];

        uint digit = (element >> push_constants.radix_bit_shift) & pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_RADIX_MASK;

        InterlockedAdd(gs_histogram.buckets[digit], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    if (is_group_helper_thread) {
        InterlockedAdd(push_constants.global_histogram[group_id].buckets[group_tid], gs_histogram.buckets[group_tid]);
    }
}