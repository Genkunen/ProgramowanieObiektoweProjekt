#include "../shared_types.hpp"
#include "../shared_consts.hpp"

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::SimulationObject* dst_updated_objects;
    float2 simulation_bounds;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

static const float SIMULATION_BOUNDS_PUSHBACK_ZONE_FACTOR = 0.01f;

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    var object = push_constants.objects[tid];

    object.position += object.velocity * push_constants.simulation_data->delta_time;

    float2 bounds = push_constants.simulation_bounds;

    float pushback_zone_width  = SIMULATION_BOUNDS_PUSHBACK_ZONE_FACTOR * push_constants.simulation_bounds.x;
    float pushback_zone_height = SIMULATION_BOUNDS_PUSHBACK_ZONE_FACTOR * push_constants.simulation_bounds.y;


    if (object.position.x <=     0.0f) { object.position.x =     400.0f; object.velocity.x =  1.0f; object.velocity.x += 0.1f; }
    if (object.position.y <=     0.0f) { object.position.y =     400.0f; object.velocity.y =  1.0f; object.velocity.y += 0.1f; }

    if (object.position.x >= bounds.x) { object.position.x = bounds.x; object.velocity.x = -1.0f; object.velocity.x -= 0.1f; }
    if (object.position.y >= bounds.y) { object.position.y = bounds.y; object.velocity.y = -1.0f; object.velocity.y -= 0.1f; }

    // persuade entities right near the edge of the screen to move away from the edge
    if (object.position.x < pushback_zone_width) {
        float dist = pushback_zone_width - object.position.x;
        object.velocity.x += pow(dist, 4) * 0.01f;
    }
    if (object.position.y < pushback_zone_height) {
        float dist = pushback_zone_height - object.position.y;
        object.velocity.y += pow(dist, 4) * 0.01f;
    }
    if (object.position.x > bounds.x - pushback_zone_width) {
        float dist = object.position.x - (bounds.x - pushback_zone_width);
        object.velocity.x -= pow(dist, 4) * 0.01f;
    }
    if (object.position.y > bounds.y - pushback_zone_height) {
        float dist = object.position.y - (bounds.y - pushback_zone_height);
        object.velocity.y -= pow(dist, 4) * 0.01f;
    }

    object.velocity *= 0.995f;

    push_constants.dst_updated_objects[tid] = object;
}