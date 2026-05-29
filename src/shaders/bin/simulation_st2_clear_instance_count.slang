import "../shared_code_shim";

import "../vk_structs/draw_indirect";

[vk::push_constant] pop::shaders::ClearInstanceCountCSPushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_CLEAR_INSTANCE_COUNT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.mesh_count) return;

    push_constants.draw_commands[tid].instanceCount = 0;
}