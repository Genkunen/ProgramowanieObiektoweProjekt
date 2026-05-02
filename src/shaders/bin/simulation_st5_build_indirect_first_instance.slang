#include "../shared_types.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommand* draw_commands;
    uint draw_commands_count;
};

[vk::push_constant] PushConstants push_constants;

groupshared Atomic<uint> gs_prefix_sum;

[shader("compute")]
[numthreads(pop::shaders::MAX_DRAW_COMMANDS, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.draw_commands_count) return;

    if (WaveIsFirstLane()) gs_prefix_sum.store(0, MemoryOrder.Relaxed);

    let instance_count = push_constants.draw_commands[tid].instanceCount.load(MemoryOrder.Relaxed);
    let base = gs_prefix_sum.add(instance_count, MemoryOrder.Relaxed);
    push_constants.draw_commands[tid].firstInstance = base;
}