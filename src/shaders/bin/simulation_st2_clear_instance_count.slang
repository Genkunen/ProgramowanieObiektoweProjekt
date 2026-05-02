import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommand* draw_commands;
    uint mesh_count;
};

[vk::push_constant] PushConstants pushConstants;

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= pushConstants.mesh_count) return;

    pushConstants.draw_commands[tid].instanceCount.store(0, MemoryOrder.Relaxed);
}