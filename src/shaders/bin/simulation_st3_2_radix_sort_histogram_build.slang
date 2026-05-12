#include "../shared_consts.hpp"

struct PushConstants {
    uint* sort_keys;

    uint* global_histogram;       // table in the format [radix_bucket]
    uint* group_local_histograms; // 2D table in the format [radix_bucket][group_id]

    uint group_count;
    uint sort_key_count;
    uint radix_bit_shift;
};

[vk::push_constant] PushConstants push_constants;

groupshared uint gs_histogram[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    // First the whole group cooperatively zeroes out the LDS histogram.
    gs_histogram[group_tid] = 0u;
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Load all the elements belonging to the thread.

    // base_index - index to the first key in the chunk of keys that this group is responsible for.
    uint base_index = group_id * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_GROUP;

    uint keys[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD];
    uint thread_keys_count = 0;

    for (uint i = 0; i < pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD; i++) {
        // This index formula gives a thread-interleaved access pattern, i.e. if we had 4 threads A,B,C,D working in the group, taking many elements
        // at a time, then the access pattern is to the chunk is ABCDABCDABCDABCD...
        uint index = base_index
            + group_tid
            + i * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS;

        if (index >= push_constants.sort_key_count) break;

        keys[i] = push_constants.sort_keys[index];
        thread_keys_count++;
    }

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Up next the group computes the LDS histogram over its chunk of keys.

    for (uint i = 0; i < thread_keys_count; i++) {
        uint val = keys[i];
        uint digit = (val >> push_constants.radix_bit_shift) & pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_RADIX_MASK;

        InterlockedAdd(gs_histogram[digit], 1u);
    }
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------

    uint digit_count = gs_histogram[group_tid];
    push_constants.group_local_histograms[push_constants.group_count * group_tid + group_id] = digit_count;

    InterlockedAdd(push_constants.global_histogram[group_tid], digit_count);
}