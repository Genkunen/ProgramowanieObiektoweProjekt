#include "../shared_types.hpp"
#include "../shared_consts.hpp"

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::SimulationObject* dst_updated_objects;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    var object = push_constants.objects[tid];

    object.position += object.velocity * push_constants.simulation_data->delta_time;

    static const float2 SIMULATION_BOUNDS = float2(80.0f, 40.0f);

    if (object.position.x <   0.0f)              { object.position.x =  0.0f; object.velocity.x *= -1.0f; }
    if (object.position.y <   0.0f)              { object.position.y =  0.0f; object.velocity.y *= -1.0f; }

    if (object.position.x > SIMULATION_BOUNDS.x) { object.position.x = SIMULATION_BOUNDS.x; object.velocity.x *= -1.0f; }
    if (object.position.y > SIMULATION_BOUNDS.y) { object.position.y = SIMULATION_BOUNDS.y; object.velocity.y *= -1.0f; }

    push_constants.dst_updated_objects[tid] = object;
}