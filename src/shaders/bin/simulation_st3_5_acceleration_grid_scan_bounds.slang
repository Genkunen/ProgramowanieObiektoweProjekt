import "../shared_code_shim";

struct PushConstants {
    uint* sort_keys;
    uint* tile_start_indices;
    uint* tile_end_indices;
    uint keys_count;
};

[vk::push_constant] PushConstants push_constants;

bool acceleration_grid_tile_starts_here(uint tid, uint this_tid_grid_tile_index) {
    // Always starts if index is 0
    if (tid == 0) return true;

    // Otherwise check if the previous thread's value differs from this thread's one
    return this_tid_grid_tile_index != push_constants.sort_keys[tid - 1];
}

bool acceleration_grid_tile_ends_here(uint tid, uint this_tid_grid_tile_index) {
    // Always ends if index is the last one
    if (tid == push_constants.keys_count - 1) return true;

    // Otherwise check if the next thread's value differs from this thread's one
    return this_tid_grid_tile_index != push_constants.sort_keys[tid + 1];
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_SCAN_BOUNDS_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.keys_count) return;

    uint acceleration_grid_tile_index = push_constants.sort_keys[tid];

    if (acceleration_grid_tile_starts_here(tid, acceleration_grid_tile_index))
        push_constants.tile_start_indices[acceleration_grid_tile_index] = tid;

    if (acceleration_grid_tile_ends_here(tid, acceleration_grid_tile_index))
        push_constants.tile_end_indices[acceleration_grid_tile_index] = tid;
}