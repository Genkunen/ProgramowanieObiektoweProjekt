#include "../shared_types.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    pop::shaders::SimulationObject* simulation_objects;
    pop::shaders::MeshAllocationData* mesh_allocations;
    vk_structs::DrawIndexedIndirectCommand* draw_indirect_commands;
    Atomic<uint>* draw_count;
    uint simulation_object_count;
};

[vk::push_constant] PushConstants push_constants;

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
}
