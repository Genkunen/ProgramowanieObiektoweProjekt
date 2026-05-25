import "../shared_code_shim";

import "../sim_state/sim_state";

import "object_behavior";
import "object_behavior_definitions";

namespace simulation {

constexpr IObjectBehavior make_behavior(uint type) {
    switch (type) {
        default: return SampleBehavior();
    }
}

void dispatch_self_simulation_behavior(inout pop::shaders::SimulationObject object, float delta_time) {
    IObjectBehavior behavior = make_behavior(object.mesh_index);

    behavior.simulate_self(object, delta_time);
}

void dispatch_neighbor_simulation_behavior(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time) {
    IObjectBehavior behavior = make_behavior(object.mesh_index);

    behavior.simulate_with_neighbor(object, other, other_id, simulation_state, delta_time);
}

} // namespace simulation