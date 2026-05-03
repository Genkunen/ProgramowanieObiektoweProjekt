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

groupshared Atomic<uint> gs_draw_instance_counts_histogram[pop::shader_consts::MAX_DRAW_COMMANDS];
groupshared uint gs_draw_instance_bases[pop::shader_consts::MAX_DRAW_COMMANDS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID) {
    uint grouptid = gtid.x;
    if (grouptid < pop::shader_consts::MAX_DRAW_COMMANDS) {
        gs_draw_instance_counts_histogram[grouptid].store(0, MemoryOrder.Relaxed);
    }

    GroupMemoryBarrierWithGroupSync();

    uint tid = dtid.x;
    uint histogram_local_instance_index = 0;
    uint object_mesh_id = 0;

    if (tid < push_constants.object_count) {
        object_mesh_id = push_constants.objects[tid].mesh_index;
        histogram_local_instance_index = gs_draw_instance_counts_histogram[object_mesh_id].increment(MemoryOrder.Relaxed);
    }

    GroupMemoryBarrierWithGroupSync();

    if (grouptid < pop::shader_consts::MAX_DRAW_COMMANDS) {
        gs_draw_instance_bases[grouptid] = push_constants.draw_commands[grouptid].instanceCount.add(
            gs_draw_instance_counts_histogram[grouptid].load(MemoryOrder.Relaxed), MemoryOrder.Relaxed
        );
    }

    GroupMemoryBarrierWithGroupSync();

    if (tid < push_constants.object_count) {
        uint instance_index = gs_draw_instance_bases[object_mesh_id] + histogram_local_instance_index;
        push_constants.drawlocal_instance_indices[tid] = instance_index;
    }
}