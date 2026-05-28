import "../shared_code_shim";

struct PushConstants {
    pop::shaders::SimulationObject* objects;
    uint* object_flags;
    uint object_count;
    uint event_randseed;
    float2 simulation_bounds;
};

[vk::push_constant] PushConstants push_constants;

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

    uint object_ev_hash = hash(hash(tid) ^ push_constants.event_randseed);

    if (push_constants.object_flags[tid] & pop::shader_consts::SIM_OBJECT_FLAG_DEAD) {
        bool resurrect = roll_chance(object_ev_hash, 0u, 0.005f);

        if (resurrect) {
            float2 new_position =
                float2(roll_norm_float(object_ev_hash, 1000u), roll_norm_float(object_ev_hash, 1001u))
                * push_constants.simulation_bounds;

            float2 new_velocity =
                float2(roll_norm_float(object_ev_hash, 1002u), roll_norm_float(object_ev_hash, 1003u))
                * 4.0f - 2.0f;

            uint new_object_type = pop::shader_consts::SIM_OBJECT_TYPE_FOOD;

            if (roll_chance(object_ev_hash, 1005u, 0.05f)) {
                new_object_type = pop::shader_consts::SIM_OBJECT_TYPE_FISH;
            }

            push_constants.objects[tid].position = new_position;
            push_constants.objects[tid].velocity = new_velocity;
            push_constants.objects[tid].size = 10.0f;
            push_constants.objects[tid].object_type = new_object_type;
            push_constants.object_flags[tid] &= ~pop::shader_consts::SIM_OBJECT_FLAG_DEAD;
        }
    }
}