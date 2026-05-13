import "../shared_code_shim";

import "../behavior/object_behavior_dispatch";

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    pop::shaders::SimulationObject* objects;
    pop::shaders::SimulationObject* dst_updated_objects;
    float2 simulation_bounds;
    uint object_count;
};

[vk::push_constant] PushConstants push_constants;

static const float SIMULATION_BOUNDS_PUSHBACK_ZONE_FACTOR = 0.01f;

float2 hash22(float2 p) {
    p = float2(dot(p, float2(127.1, 311.7)),
               dot(p, float2(269.5, 183.3)));
    return frac(sin(p) * 43758.5453) * 2.0 - 1.0;
}

float smooth_noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);

    float2 u = f * f * (3.0 - 2.0 * f);

    float a = hash22(i + float2(0,0)).x;
    float b = hash22(i + float2(1,0)).x;
    float c = hash22(i + float2(0,1)).x;
    float d = hash22(i + float2(1,1)).x;

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float2 vector_field(float2 p, float t) {
    float2 sp = p / 4.0;

    float nx = smooth_noise(sp + float2(t * 0.11, 17.3));
    float ny = smooth_noise(sp + float2(-23.1, t * 0.13));

    return float2(nx, ny) * 50.0;
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    float dt = push_constants.simulation_data->delta_time;

    var object = push_constants.objects[tid];

    object.position += object.velocity * dt;

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

    object.velocity += vector_field(object.position, push_constants.simulation_data->simulation_time_since_start) * 0.1f;
    simulation::dispatch_self_simulation_behavior(object, dt);

    push_constants.dst_updated_objects[tid] = object;
}