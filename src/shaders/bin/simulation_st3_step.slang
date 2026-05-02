#include "../shared_types.hpp"

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::SimulationObject* dst_updated_objects;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    var object = push_constants.objects[tid];

    object.position += object.velocity * push_constants.simulation_data->delta_time;

    if (object.position.x < -4.0f) { object.position.x = -4.0f; object.velocity.x *= -1.0f; }
    if (object.position.x >  4.0f) { object.position.x =  4.0f; object.velocity.x *= -1.0f; }
    if (object.position.y < -2.0f) { object.position.y = -2.0f; object.velocity.y *= -1.0f; }
    if (object.position.y >  2.0f) { object.position.y =  2.0f; object.velocity.y *= -1.0f; }

    push_constants.dst_updated_objects[tid] = object;
}