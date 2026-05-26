import "../shared_code_shim";
import "../sim_state/sim_state";
import "object_behavior";

namespace simulation {

struct NoBehavior : IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time) {}
    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {}
};

struct FoodBehavior : IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time) {
        object.velocity *= pow(0.00025f, delta_time);
    }
    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {}
};

struct SampleBehavior : IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time) {}

    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {
        float2 pos_delta = other.position - object.position;
        float dist_squared = dot(pos_delta, pos_delta);
        float dist = sqrt(dist_squared);
        float eat_distance = (object.size / 10.0f) - 0.65f;

        switch (other.object_type) {
        case pop::shader_consts::SIM_OBJECT_TYPE_FOOD:
            object.velocity += clamp(70.0 * delta_time * pos_delta / (dist + 1e-2f), float2(-100.0f), float2(100.0f));

            if (dist < eat_distance) {
                if (simulation_state.try_set_object_dead(other_id)) {
                    object.size += 0.5f;
                }
            }
            break;
        case pop::shader_consts::SIM_OBJECT_TYPE_FISH:
            float size_diff = object.size - other.size;
            if (size_diff < 10.0f) return;

            object.velocity += clamp(105.0 * delta_time * pos_delta / (dist + 1e-2f), float2(-100.0f), float2(100.0f));

            if (dist < eat_distance) {
                if (simulation_state.try_set_object_dead(other_id)) {
                    object.size += 10.0f;
                }
            }
            break;
        }
    }
};

} // namespace simulation