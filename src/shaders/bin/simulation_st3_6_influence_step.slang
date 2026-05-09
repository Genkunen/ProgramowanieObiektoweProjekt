#include "../shared_types.hpp"
#include "../shared_consts.hpp"

struct PushConstants {
    pop::shaders::SimulationObject* objects;

    uint* acceleration_grid_tile_object_ids;
    uint* acceleration_grid_tile_start_indices;
    uint* acceleration_grid_tile_end_indices;

    float grid_cell_size;
    uint grid_width;
    uint grid_height;
    uint32_t object_count;
};

[vk::push_constant] PushConstants push_constants;

uint compute_cell_key(uint2 cell_pos) {
    return cell_pos.x + cell_pos.y * push_constants.grid_width;
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_INFLUENCE_STEP_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;

    if (tid >= push_constants.object_count) return;

    float2 object_pos = push_constants.objects[tid].position;
    int2 object_grid_tile_pos = int2(object_pos / float2(push_constants.grid_cell_size));

    uint tile_start_indices[3][3];
    uint tile_end_indices[3][3];

    [ForceUnroll]
    for (int dy = -1; dy <= 1; dy++) {
        if (object_grid_tile_pos.y + dy < 0 || object_grid_tile_pos.y + dy >= push_constants.grid_height) continue;

        [ForceUnroll]
        for (int dx = -1; dx <= 1; dx++) {
            if (object_grid_tile_pos.x + dx < 0 || object_grid_tile_pos.x + dx >= push_constants.grid_width) continue;

            uint2 cell_pos = object_grid_tile_pos + uint2(dx, dy);
            uint cell_key = compute_cell_key(cell_pos);

            uint tile_start_index = push_constants.acceleration_grid_tile_start_indices[cell_key];
            uint tile_end_index = push_constants.acceleration_grid_tile_end_indices[cell_key];

            tile_start_indices[dy + 1][dx + 1] = tile_start_index;
            tile_end_indices[dy + 1][dx + 1] = tile_end_index;
        }
    }

    [ForceUnroll]
    for (int dx = -1; dx <= 1; dx++) {
        if (object_grid_tile_pos.x + dx < 0 || object_grid_tile_pos.x + dx >= push_constants.grid_width) continue;

        [ForceUnroll]
        for (int dy = -1; dy <= 1; dy++) {
            if (object_grid_tile_pos.y + dy < 0 || object_grid_tile_pos.y + dy >= push_constants.grid_height) continue;

            uint tile_start_index = tile_start_indices[dy + 1][dx + 1];
            uint tile_end_index = tile_end_indices[dy + 1][dx + 1];

            if (tile_start_index == 0xffffffff) continue;

            for (uint i = tile_start_index; i <= tile_end_index && i < tile_end_index + 256; i++) {
                uint foreign_object_id = push_constants.acceleration_grid_tile_object_ids[i];
                if (foreign_object_id == tid) continue;

                float2 direction_to_foreign_object = normalize(push_constants.objects[foreign_object_id].position - object_pos);
                float dist_squared = max(dot(direction_to_foreign_object, direction_to_foreign_object), 1e-4f)

                float2 vel = push_constants.objects[tid].velocity;
                vel += 0.025f * direction_to_foreign_object * (1.0f / dist_squared);

                vel = clamp(vel, float2(-100.0f), float2(100.0f));
                push_constants.objects[tid].velocity = vel;
            }
        }
    }
}