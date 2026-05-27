#include "simulation_data_csv_writer.hpp"

#include "../shaders/shared_consts.hpp"

#include <cstdint>
#include <fstream>

namespace pop::systems {

auto SimulationDataCsvWriter::write_to_file(const vulkan::renderer::SimulationDataSnapshot& data, const std::filesystem::path& filename) -> void {
    assert(data.simulation_objects.size() == data.simulation_objects_flags.size() && "simulation data snapshot vectors must all be of the same size");

    std::ofstream file(filename);

    file << "ObjectID,Type,Dead,PosX,PosY,VelX,VelY,Size\n";

    for (size_t i = 0; i < data.simulation_objects.size(); i++) {
        auto& object = data.simulation_objects[i];
        auto& flags = data.simulation_objects_flags[i];

        if ((flags & shader_consts::SIM_OBJECT_FLAG_DEAD) == 0) {
            std::string object_type;
            switch (object.object_type) {
            case shader_consts::SIM_OBJECT_TYPE_FOOD:
                object_type = "Food";
                break;
            case shader_consts::SIM_OBJECT_TYPE_FISH:
                object_type = "Fish";
                break;
            }

            file << std::format("{},{},,{},{},{},{},{}\n", i, object_type, object.position.x, object.position.y, object.velocity.x, object.velocity.y, object.size);
        } else {
            file << std::format("{},,Dead,,,,,\n", i);
        }
    }

    file.close();
}

} // namespace pop::systems