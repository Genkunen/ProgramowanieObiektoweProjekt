#include "../shared_types.hpp"
#include "../shared_consts.hpp"

struct PushConstants {
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

int2 compute_tile_position(uint tile_id) {
    return int2(tile_id % push_constants.grid_width, tile_id / push_constants.grid_width);
}

bool is_tile_in_bounds(int2 current, int dx, int dy) {
    int2 neighbor = current + int2(dx, dy);
    return neighbor.x >= 0 && neighbor.x < push_constants.grid_width && neighbor.y >= 0 && neighbor.y < push_constants.grid_height;
}

uint compute_neighbor_tile_key(uint current, int dx, int dy) {
    return current + dx + dy * push_constants.grid_width;
}

static const uint MAX_OBJECTS_PER_TILE = 64;

groupshared uint neighborhood_tile_start_indices[3][3];
groupshared uint neighborhood_tile_end_indices[3][3];

groupshared pop::shaders::SimulationObject master_tile_objects[MAX_OBJECTS_PER_TILE];
groupshared pop::shaders::SimulationObject neighbor_tile_objects[MAX_OBJECTS_PER_TILE];

void interact(inout pop::shaders::SimulationObject object, const inout pop::shaders::SimulationObject other) {
    float2 pos_delta = other.position - object.position;
    float dist_squared = dot(pos_delta, pos_delta);

    object.velocity += clamp(pos_delta / (dist_squared + 1e-2f), float2(-100.0f), float2(100.0f));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    bool is_group_indices_helper_thread = group_tid <  9; // to cooperatively fill tile start/end indices
    bool is_group_tile_helper_thread    = group_tid < 64; // to cooperatively load object data into tiles

    int2 master_tile_position = compute_tile_position(group_id);

    // Load dispatch's neighborhood tile start/end indices
    if (is_group_indices_helper_thread) {
        int dx = (group_tid % 3) - 1;
        int dy = (group_tid / 3) - 1;

        if (is_tile_in_bounds(master_tile_position, dx, dy)) {
            uint neighbor_tile_key = compute_neighbor_tile_key(group_id, dx, dy);
            uint neighbor_tile_start_index = push_constants.acceleration_grid_tile_start_indices[neighbor_tile_key];
            uint neighbor_tile_end_index = push_constants.acceleration_grid_tile_end_indices[neighbor_tile_key];

            neighborhood_tile_start_indices[dx + 1][dy + 1] = neighbor_tile_start_index;
            neighborhood_tile_end_indices[dx + 1][dy + 1] = neighbor_tile_end_index;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (neighborhood_tile_start_indices[1][1] == 0xffffffff) {
        return;
    }

    // Parameters about this this thread's object in the master tile
    pop::shaders::SimulationObject thread_object;
    uint thread_object_id = push_constants.acceleration_grid_tile_object_ids[neighborhood_tile_start_indices[1][1] + group_tid];

    // Load dispatch's tile objects
    uint master_tile_object_count = neighborhood_tile_end_indices[1][1] - neighborhood_tile_start_indices[1][1] + 1;
    bool thread_has_object = group_tid < master_tile_object_count;

    if (thread_has_object) {
        thread_object = push_constants.objects[thread_object_id];
        master_tile_objects[group_tid] = thread_object;
    }
    GroupMemoryBarrierWithGroupSync();

    if (thread_has_object) {
        // Compute influence from the same tile
        for (uint i = 0; i < master_tile_object_count; i++) {
            if (i == group_tid) continue;

            pop::shaders::SimulationObject neighbor_object = master_tile_objects[i];
            interact(thread_object, neighbor_object);
        }
    }

    // Loop over neighboring tiles
    for (int dy = -1; dy <= 1; dy++) {
        if (!is_tile_in_bounds(master_tile_position, 0, dy)) continue;

        for (int dx = -1; dx <= 1; dx++) {
            if (!is_tile_in_bounds(master_tile_position, dx, dy)) continue;
            if (dx == 0 && dy == 0) continue;                                            // Skip the master tile
            if (neighborhood_tile_start_indices[dx + 1][dy + 1] == 0xffffffff) continue; // Skip tiles that don't have any objects

            // Load neighboring tile objects
            uint neighbor_tile_start_index = neighborhood_tile_start_indices[dx + 1][dy + 1];
            uint neighbor_tile_object_count = neighborhood_tile_end_indices[dx + 1][dy + 1] - neighbor_tile_start_index + 1;

            if (is_group_tile_helper_thread && group_tid < neighbor_tile_object_count) {
                uint object_id = push_constants.acceleration_grid_tile_object_ids[neighbor_tile_start_index + group_tid];
                neighbor_tile_objects[group_tid] = push_constants.objects[object_id];
            }
            GroupMemoryBarrierWithGroupSync();

            // Compute influence from neighboring tiles
            // GTID - object index in master tile, i - object index in the neighboring tile
            if (thread_has_object) {
                for (uint i = 0; i < neighbor_tile_object_count && i < MAX_OBJECTS_PER_TILE; i++) {
                    pop::shaders::SimulationObject neighbor_object = neighbor_tile_objects[i];

                    interact(thread_object, neighbor_object);
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (thread_has_object) {
        push_constants.next_objects[thread_object_id] = thread_object;
    }
    // All objects must be part of the grid so no objects are missed in next_objects update
}