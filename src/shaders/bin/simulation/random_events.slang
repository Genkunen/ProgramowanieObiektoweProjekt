import "../../shared_code_shim";

[vk::push_constant] pop::shaders::RandomEventsCSPushConstants push_constants;

uint hash(uint h) {
    h = (h ^ 61u) ^ (h >> 16u);
    h *= 9u;
    h = h ^ (h >> 4u);
    h *= 0x27d4eb2du;
    h = h ^ (h >> 15u);

    h ^= (h >> 13u);
    h *= 0x85ebca6bu;
    h ^= (h >> 16u);

    return h;
}

float roll_norm_float(uint h, uint domain) {
    uint k = hash(h ^ (domain * 0x27d4eb2du));

    float r = (float)k / 4294967296.0f;

    return r;
}

bool roll_chance(uint h, uint domain, float chance) {
    if (chance <= 0.0f) return false;
    if (chance >= 1.0f) return true;

    float r = roll_norm_float(h, domain);
    return r < chance;
}

[shader("compute")]
[numthreads(pop::shader_consts::CS_RANDOM_EVENTS_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;
    if (tid >= push_constants.object_count) return;

    bool object_is_dead = (push_constants.simulation_object_flags[tid] & pop::shader_consts::SIM_OBJECT_FLAG_DEAD) != 0;
    if (object_is_dead) return;

    uint object_ev_hash = hash(hash(tid) ^ push_constants.event_randseed);

    bool resurrect = roll_chance(object_ev_hash, 0u, 0.03f);
    if (!resurrect) return;

    float2 new_position =
        float2(roll_norm_float(object_ev_hash, 1000u), roll_norm_float(object_ev_hash, 1001u))
        * push_constants.simulation_bounds;

    float2 new_velocity =
        float2(roll_norm_float(object_ev_hash, 1002u), roll_norm_float(object_ev_hash, 1003u))
        * 4.0f - 2.0f;

    uint new_object_type = pop::shader_consts::SIM_OBJECT_TYPE_FOOD;

    if (roll_chance(object_ev_hash, 1005u, 0.005f)) {
        new_object_type = pop::shader_consts::SIM_OBJECT_TYPE_FISH;
    }

    push_constants.simulation_objects[tid].position = new_position;
    push_constants.simulation_objects[tid].velocity = new_velocity;
    push_constants.simulation_objects[tid].size = 10.0f;
    push_constants.simulation_objects[tid].object_type = new_object_type;
    push_constants.simulation_object_flags[tid] &= ~pop::shader_consts::SIM_OBJECT_FLAG_DEAD;
}