import "../shared_code_shim";

[vk::constant_id(0)] const uint GROUP_SIZE = 64;

[vk::push_constant] pop::shaders::SimulationAccelerationGridRadixSortHistogramCSPushConstants push_constants;

groupshared uint gs_histogram[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS];

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    uint wb_iterations = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / GROUP_SIZE;

    // First the whole group cooperatively zeroes out the LDS histogram.
    [unroll]
    for (uint i = 0; i < wb_iterations; i++) {
        uint base = i * GROUP_SIZE;
        gs_histogram[base + group_tid] = 0u;
    }
    GroupMemoryBarrierWithWaveSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Load all the elements belonging to the thread.

    // base_index - index to the first key in the chunk of keys that this group is responsible for.
    uint base_index = group_id * GROUP_SIZE * pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD;

    uint keys[pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD];
    uint thread_keys_count = 0;

    for (uint i = 0; i < pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD; i++) {
        // This index formula gives a thread-interleaved access pattern, i.e. if we had 4 threads A,B,C,D working in the group, taking many elements
        // at a time, then the access pattern is to the chunk is ABCDABCDABCDABCD...
        uint index = base_index
            + group_tid
            + i * GROUP_SIZE;

        if (index >= push_constants.keys_count) break;

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
    GroupMemoryBarrierWithWaveSync();

    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Write back the histogram to the global memory and build the global histogram.

    [unroll]
    for (uint i = 0; i < wb_iterations; i++) {
        uint base = i * GROUP_SIZE;

        uint digit_id = base + group_tid;
        uint digit_count = gs_histogram[digit_id];
        push_constants.group_local_histograms[push_constants.group_count * digit_id + group_id] = digit_count;

        InterlockedAdd(push_constants.global_histogram[digit_id], digit_count);
    }
}