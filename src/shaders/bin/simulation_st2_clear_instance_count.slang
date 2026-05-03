#include "../shared_consts.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommandNonatomic* draw_commands;
    uint mesh_count;
};

[vk::push_constant] PushConstants pushConstants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_CLEAR_INSTANCE_COUNT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= pushConstants.mesh_count) return;

    pushConstants.draw_commands[tid].instanceCount = 0;
}