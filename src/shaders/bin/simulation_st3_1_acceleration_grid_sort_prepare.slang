import "../shared_code_shim";

struct PushConstants {
    pop::shaders::SimulationObject* objects;
    uint* sort_keys;
    uint* sort_object_indices;
    float grid_cell_size;
    uint grid_width;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_SORT_PREPARE_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    float2 object_position = push_constants.objects[tid].position;
    uint2 cell_index = uint2(object_position / float2(push_constants.grid_cell_size));
    uint cell_key = cell_index.x + cell_index.y * push_constants.grid_width;

    push_constants.sort_keys[tid] = cell_key;
    push_constants.sort_object_indices[tid] = tid;
}