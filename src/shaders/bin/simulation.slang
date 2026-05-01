#include "../shared_types.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* simulation_objects;
    pop::shaders::MeshAllocationData* mesh_allocations;
    pop::shaders::PreparedSimulationObject* prepared_simulation_objects;
    vk_structs::DrawIndexedIndirectCommand* draw_indirect_commands;
    Atomic<uint>* draw_count;
    uint simulation_object_count;
};

[vk::push_constant] PushConstants push_constants;

float hash1(uint x) {
    x ^= x * 0x27d4eb2d;
    x ^= x >> 15;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    return frac(x / 4294967296.0); // divide by 2^32
}

// 2D random in [-1,1]
float2 random_offset(uint seed) {
    float r1 = hash1(seed);
    float r2 = hash1(seed * 1664525u + 1013904223u); // LCG step for decorrelation

    return float2(r1, r2) * 2.0 - 1.0;
}


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
[numthreads(32, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint index = dtid.x;

    // Get rid of threads that do not have work to do
    if (index >= push_constants.simulation_object_count) return;

    let object = push_constants.simulation_objects[index];
    let mesh_allocation = push_constants.mesh_allocations[object.mesh_index];
    // TODO: simulation step

    vk_structs::DrawIndexedIndirectCommand draw_command;
    draw_command.indexCount = mesh_allocation.index_count;
    draw_command.instanceCount = 1;
    draw_command.firstIndex = mesh_allocation.first_index;
    draw_command.firstInstance = index;
    draw_command.vertexOffset = mesh_allocation.vertex_offset;

    uint draw_id = push_constants.draw_count->add(1, MemoryOrder.Relaxed);
    push_constants.draw_indirect_commands[draw_id] = draw_command;

    float2 position = random_offset(index) * float2(4.0, 2.0);

    float4x4 transform = mul(push_constants.simulation_data->projview, build_model_matrix(position, 0.0));

    push_constants.prepared_simulation_objects[index].transform = transform;

}
