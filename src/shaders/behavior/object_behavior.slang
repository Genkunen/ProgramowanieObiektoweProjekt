import "../shared_code_shim";

namespace simulation {

interface IObjectBehavior {
    void simulate_self(inout pop::shaders::SimulationObject object, float delta_time);
    void simulate_with_neighbor(inout pop::shaders::SimulationObject object, const in pop::shaders::SimulationObject other, float delta_time);
};

} // namespace simulation