import "../shared_code_shim";
import "../sim_state/sim_state";

namespace simulation {

interface IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time);
    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, const uint other_id, inout SimulationState simulation_state, float delta_time);
};

} // namespace simulation