import "../shared_code_shim";
import "object_behavior";

namespace simulation {

struct SampleBehavior : IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time) {
        object.velocity *= 0.98f;
    }

    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, float delta_time) {
        float2 pos_delta = other.position - object.position;
        float dist_squared = dot(pos_delta, pos_delta);

        object.velocity += clamp(10.0 * delta_time * pos_delta / (dist_squared + 1e-2f), float2(-100.0f), float2(100.0f));
    }
};

} // namespace simulation