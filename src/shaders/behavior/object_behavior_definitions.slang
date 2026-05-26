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
        object.velocity *= pow(0.6f, delta_time);
    }
    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {}
};

struct SampleBehavior : IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time) {}

    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {
        if (other.object_type != pop::shader_consts::SIM_OBJECT_TYPE_FOOD) return;

        float2 pos_delta = other.position - object.position;
        float dist_squared = dot(pos_delta, pos_delta);
        float dist = sqrt(dist_squared);

        object.velocity += clamp(70.0 * delta_time * pos_delta / (dist + 1e-2f), float2(-100.0f), float2(100.0f));

        if (dist_squared < 1.0f) {
            simulation_state.try_set_object_dead(other_id);
        }
    }
};

} // namespace simulation