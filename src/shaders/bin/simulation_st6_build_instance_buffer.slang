#include "../shared_types.hpp"
#include "../shared_consts.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommandNonatomic* draw_commands;
    uint* draw_instance_indices;
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::PreparedSimulationObject* prepared_objects;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

float4x4 build_model_matrix(float2 pos, float rotation) {
    float sz = sin(rotation);
    float cz = cos(rotation);

    return float4x4(
        cz,    -sz,   0.0, pos.x,
        sz,    cz,    0.0, pos.y,
        0.0,   0.0,   1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_BUILD_INSTANCE_BUFFER_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    let mesh_id = push_constants.objects[tid].mesh_index;
    let mesh_instance_base = push_constants.draw_commands[mesh_id].firstInstance;
    let mesh_draw_local_instance = push_constants.draw_instance_indices[tid];
    let mesh_instance_index = mesh_instance_base + mesh_draw_local_instance;

    float4x4 model_matrix = mul(push_constants.simulation_data->projview, build_model_matrix(push_constants.objects[tid].position, 0.0));

    push_constants.prepared_objects[mesh_instance_index].transform = model_matrix;
}