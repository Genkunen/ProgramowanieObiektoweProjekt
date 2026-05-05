#include "../shared_types.hpp"
#include "../shared_consts.hpp"

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommandNonatomic* draw_commands;
    uint* drawlocal_instance_indices;
    pop::shaders::SimulationObject* objects;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

groupshared uint gs_instance_counts[pop::shader_consts::MAX_DRAW_COMMANDS];
groupshared uint gs_instance_prefixes[pop::shader_consts::MAX_DRAW_COMMANDS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;

    bool is_working_thread = tid < push_constants.object_count;
    bool is_group_helper_thread = group_tid < pop::shader_consts::MAX_DRAW_COMMANDS;
    bool is_group_writer_thread = group_tid == 0;

    if (is_group_helper_thread) {
        gs_instance_counts[group_tid] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint mesh_id = 0;
    uint mesh_object_instance_id = 0;
    if (is_working_thread) {
        mesh_id = push_constants.objects[tid].mesh_index;
        InterlockedAdd(gs_instance_counts[mesh_id], 1, mesh_object_instance_id);
    }
    GroupMemoryBarrierWithGroupSync();

    if (is_group_helper_thread) {
        uint instance_count = gs_instance_counts[group_tid];
        if (instance_count > 0) {
            uint base;
            InterlockedAdd(push_constants.draw_commands[group_tid].instanceCount, instance_count, base);
            gs_instance_prefixes[group_tid] = base;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (is_working_thread) {
        uint global_instance_index = gs_instance_prefixes[mesh_id] + mesh_object_instance_id;
        push_constants.drawlocal_instance_indices[tid] = global_instance_index;
    }
}
