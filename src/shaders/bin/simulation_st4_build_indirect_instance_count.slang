import "../shared_code_shim";

import "../vk_structs/draw_indirect";

struct PushConstants {
    vk_structs::DrawIndexedIndirectCommand* draw_commands;
    uint* drawlocal_instance_indices;
    pop::shaders::SimulationObject* objects;
    uint* object_flags;
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
    bool thread_has_object = false;
    if (is_working_thread) {
        uint object_flags = push_constants.object_flags[tid];

        bool is_alive = (object_flags & pop::shader_consts::SIM_OBJECT_FLAG_DEAD) == 0;

        bool is_visible = is_alive; // TODO: frustum cull
        thread_has_object = is_visible;

        if (is_visible) {
            mesh_id = push_constants.objects[tid].mesh_index;
            InterlockedAdd(gs_instance_counts[mesh_id], 1, mesh_object_instance_id);
        }

        uint new_object_flags = is_visible ?
            (object_flags | pop::shader_consts::SIM_OBJECT_FLAG_VISIBLE) :
            (object_flags & (~pop::shader_consts::SIM_OBJECT_FLAG_VISIBLE));

        push_constants.object_flags[tid] = new_object_flags;
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

    if (thread_has_object) {
        uint global_instance_index = gs_instance_prefixes[mesh_id] + mesh_object_instance_id;
        push_constants.drawlocal_instance_indices[tid] = global_instance_index;
    }
}
