import "../shared_code_shim";

import "../behavior/object_behavior_dispatch";

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::SimulationObject* next_objects;

    uint* acceleration_grid_tile_object_ids;
    uint* acceleration_grid_tile_start_indices;
    uint* acceleration_grid_tile_end_indices;

    float grid_cell_size;
    uint grid_width;
    uint grid_height;
    uint32_t object_count;
};

[vk::push_constant] PushConstants push_constants;

uint morton_code_stride2_decode(uint code) {
    uint x = code & 0x55555555u;
    x = (x | (x >> 1)) & 0x33333333u;
    x = (x | (x >> 2)) & 0x0f0f0f0fu;
    x = (x | (x >> 4)) & 0x00ff00ffu;
    x = (x | (x >> 8)) & 0x0000ffffu;
    return x;
}

uint2 group_id_morton_decode(uint group_id) {
    uint x = morton_code_stride2_decode(group_id);
    uint y = morton_code_stride2_decode(group_id >> 1);
    return uint2(x, y);
}

bool is_tile_in_bounds(int2 tile_position) {
    return tile_position.x >= 0 && tile_position.x < push_constants.grid_width && tile_position.y >= 0 && tile_position.y < push_constants.grid_height;
}

uint compute_tile_key(int2 tile_position) {
    return tile_position.x + tile_position.y * push_constants.grid_width;
}

static const uint MAX_OBJECTS_PER_TILE = 64;

groupshared uint neighborhood_tile_start_indices[3][3];
groupshared uint neighborhood_tile_end_indices[3][3];

groupshared pop::shaders::SimulationObject tile_objects_scratch[MAX_OBJECTS_PER_TILE];

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    float dt = push_constants.simulation_data->delta_time;

    bool is_group_indices_helper_thread = group_tid <  9; // to cooperatively fill tile start/end indices
    bool is_group_tile_helper_thread    = group_tid < 64; // to cooperatively load object data into tiles

    int2 master_tile_position = group_id_morton_decode(group_id);
    if (!is_tile_in_bounds(master_tile_position)) return;

    // Load dispatch's neighborhood tile start/end indices
    if (is_group_indices_helper_thread) {
        int dx = (group_tid % 3) - 1;
        int dy = (group_tid / 3) - 1;

        int2 delta = int2(dx, dy);
        int2 tile_position = master_tile_position + delta;

        if (is_tile_in_bounds(tile_position)) {
            uint tile_key = compute_tile_key(tile_position);
            uint tile_start_index = push_constants.acceleration_grid_tile_start_indices[tile_key];
            uint tile_end_index = push_constants.acceleration_grid_tile_end_indices[tile_key];

            neighborhood_tile_start_indices[dx + 1][dy + 1] = tile_start_index;
            neighborhood_tile_end_indices[dx + 1][dy + 1] = tile_end_index;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (neighborhood_tile_start_indices[1][1] == 0xffffffff) {
        return;
    }

    // Parameters about this this thread's object in the master tile
    bool is_in_bounds = neighborhood_tile_start_indices[1][1] + group_tid < neighborhood_tile_end_indices[1][1] + 1;
    pop::shaders::SimulationObject thread_object;
    uint thread_object_id = is_in_bounds ? push_constants.acceleration_grid_tile_object_ids[neighborhood_tile_start_indices[1][1] + group_tid] : 0xffffffff;

    // Load dispatch's tile objects
    uint master_tile_object_count = neighborhood_tile_end_indices[1][1] - neighborhood_tile_start_indices[1][1] + 1;
    bool thread_has_object = (group_tid < master_tile_object_count) && is_in_bounds;

    if (thread_has_object) {
        thread_object = push_constants.objects[thread_object_id];
        tile_objects_scratch[group_tid] = thread_object;
    }
    GroupMemoryBarrierWithGroupSync();

    if (thread_has_object) {
        // Compute influence from the same tile
        for (uint i = 0; i < master_tile_object_count && i < MAX_OBJECTS_PER_TILE; i++) {
            if (i == group_tid) continue;

            pop::shaders::SimulationObject neighbor_object = tile_objects_scratch[i];
            simulation::dispatch_neighbor_simulation_behavior(thread_object, neighbor_object, dt);
        }
    }

    // Loop over neighboring tiles
    for (int dy = -1; dy <= 1; dy++) {
        if (!is_tile_in_bounds(master_tile_position + int2(0, dy))) continue;

        for (int dx = -1; dx <= 1; dx++) {
            if (!is_tile_in_bounds(master_tile_position + int2(dx, dy))) continue;
            if (dx == 0 && dy == 0) continue;                                            // Skip the master tile
            if (neighborhood_tile_start_indices[dx + 1][dy + 1] == 0xffffffff) continue; // Skip tiles that don't have any objects

            // Load neighboring tile objects
            uint neighbor_tile_start_index = neighborhood_tile_start_indices[dx + 1][dy + 1];
            uint neighbor_tile_object_count = neighborhood_tile_end_indices[dx + 1][dy + 1] - neighbor_tile_start_index + 1;

            if (is_group_tile_helper_thread && group_tid < neighbor_tile_object_count) {
                uint object_id = push_constants.acceleration_grid_tile_object_ids[neighbor_tile_start_index + group_tid];
                tile_objects_scratch[group_tid] = push_constants.objects[object_id];
            }
            GroupMemoryBarrierWithGroupSync();

            // Compute influence from neighboring tiles
            // GTID - object index in master tile, i - object index in the neighboring tile
            if (thread_has_object) {
                for (uint i = 0; i < neighbor_tile_object_count && i < MAX_OBJECTS_PER_TILE; i++) {
                    pop::shaders::SimulationObject neighbor_object = tile_objects_scratch[i];

                    simulation::dispatch_neighbor_simulation_behavior(thread_object, neighbor_object, dt);
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (thread_has_object) {
        push_constants.next_objects[thread_object_id] = thread_object;
    }

    // If there are more objects in this group's tile than threads in it then copy the remaining objects
    if (master_tile_object_count > MAX_OBJECTS_PER_TILE) {
        if (is_group_tile_helper_thread) {
            for (uint i = MAX_OBJECTS_PER_TILE + group_tid; i < master_tile_object_count; i += MAX_OBJECTS_PER_TILE) {
                uint object_id = push_constants.acceleration_grid_tile_object_ids[neighborhood_tile_start_indices[1][1] + i];
                push_constants.next_objects[object_id] = push_constants.objects[object_id];
            }
        }
    }
}