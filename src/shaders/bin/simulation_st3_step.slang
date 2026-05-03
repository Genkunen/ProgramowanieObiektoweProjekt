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

    if (object.position.x < -40.0f) { object.position.x = -40.0f; object.velocity.x *= -1.0f; }
    if (object.position.x >  40.0f) { object.position.x =  40.0f; object.velocity.x *= -1.0f; }
    if (object.position.y < -20.0f) { object.position.y = -20.0f; object.velocity.y *= -1.0f; }
    if (object.position.y >  20.0f) { object.position.y =  20.0f; object.velocity.y *= -1.0f; }

    push_constants.dst_updated_objects[tid] = object;
}