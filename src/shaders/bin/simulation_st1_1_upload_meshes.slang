#include "../shared_types.hpp"
#include "../shared_consts.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommand* draw_commands;
    pop::shaders::MeshAllocationData* mesh_allocations;
    uint mesh_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_UPLOAD_MESHES_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.mesh_count) return;

    let mesh_allocation = push_constants.mesh_allocations[tid];

    push_constants.draw_commands[tid].firstIndex = mesh_allocation.first_index;
    push_constants.draw_commands[tid].indexCount = mesh_allocation.index_count;
    push_constants.draw_commands[tid].vertexOffset = mesh_allocation.vertex_offset;
}