import "../shared_code_shim";

import "../behavior/object_behavior_dispatch";

[vk::push_constant] pop::shaders::SimulationStepCSPushConstants push_constants;

float2 hash_pos(float2 p) {
    float2 hp = float2(dot(p, float2(127.1, 311.7)),
                       dot(p, float2(269.5, 183.3)));
    return frac(sin(hp) * 43758.5453) * 2.0 - 1.0;
}

float vnoise(float2 p) {
    float2 i  = floor(p);
    float2 f  = frac(p);
    float2 u  = f * f * (3.0 - 2.0 * f);

    float a = dot(hash_pos(i),               f);
    float b = dot(hash_pos(i + float2(1,0)), f - float2(1,0));
    float c = dot(hash_pos(i + float2(0,1)), f - float2(0,1));
    float d = dot(hash_pos(i + float2(1,1)), f - float2(1,1));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float2 curl_noise(float2 p, float t, float scale) {
    const float EPSILON = 0.001;
    float2 sp = p * scale;
    float  tp = t * 0.3;

    float dFdy = (vnoise(float2(sp.x,           sp.y + EPSILON) + tp)
                - vnoise(float2(sp.x,           sp.y - EPSILON) + tp)) / (2.0 * EPSILON);

    float dFdx = (vnoise(float2(sp.x + EPSILON, sp.y)           + tp)
                - vnoise(float2(sp.x - EPSILON, sp.y)           + tp)) / (2.0 * EPSILON);

    return float2(dFdy, -dFdx);
}

float2 water_flow_vector_field(float2 pos, float time, float scale) {
    float2 c1 = curl_noise(pos, time, scale);

    float2 warped_pos = pos + 0.3 * float2(
        vnoise(pos * scale * 0.7 + float2(5.2, 1.3) + time * 0.15),
        vnoise(pos * scale * 0.7 + float2(3.7, 8.1) + time * 0.15)
    );

    float2 c2 = curl_noise(warped_pos, time * 0.7, scale * 1.3);

    return c1 * 0.6 + c2 * 0.4;
}

static const float SIMULATION_BOUNDS_PUSHBACK_ZONE_FACTOR = 0.01f;

static const float OBJECT_VELOCITY_LIMIT = 100.0f;

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    if (tid >= push_constants.object_count) return;

    bool is_alive = (push_constants.object_flags[tid] & pop::shader_consts::SIM_OBJECT_FLAG_DEAD) == 0;
    if (!is_alive) return;

    float dt = push_constants.simulation_data->delta_time;

    var object = push_constants.objects[tid];

    if (length(object.velocity) > OBJECT_VELOCITY_LIMIT) {
        object.velocity = normalize(object.velocity) * OBJECT_VELOCITY_LIMIT;
    }

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

    object.velocity += water_flow_vector_field(object.position, push_constants.simulation_data->simulation_time_since_start, .1f) * push_constants.water_current_strength * dt;
    object.velocity *= pow(0.25f, dt);
    simulation::dispatch_self_simulation_behavior(object, dt);

    push_constants.dst_updated_objects[tid] = object;
}