#include "../shared_types.hpp"
#include "../shared_consts.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommand* draw_commands;
    uint* drawlocal_instance_indices;
    pop::shaders::SimulationObject* objects;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    let object_mesh_id = push_constants.objects[tid].mesh_index;

    uint instance_index = push_constants.draw_commands[object_mesh_id].instanceCount.increment(MemoryOrder.Relaxed);
    push_constants.drawlocal_instance_indices[tid] = instance_index;
}
