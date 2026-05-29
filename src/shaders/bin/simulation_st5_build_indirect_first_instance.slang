import "../shared_code_shim";

import "../vk_structs/draw_indirect";

[vk::push_constant] pop::shaders::BuildIndirectFirstInstanceCSPushConstants push_constants;

groupshared uint gs_prefix_sum;

[shader("compute")]
[numthreads(pop::shader_consts::CS_BUILD_INDIRECT_FIRST_INSTANCE_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.draw_commands_count) return;

    if (tid == 0) gs_prefix_sum = 0;

    GroupMemoryBarrierWithGroupSync();

    let instance_count = push_constants.draw_commands[tid].instanceCount;

    uint base = 0;
    InterlockedAdd(gs_prefix_sum, instance_count, base);

    push_constants.draw_commands[tid].firstInstance = base;
}