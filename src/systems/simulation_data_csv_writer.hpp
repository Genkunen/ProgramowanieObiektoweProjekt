#pragma once
#include "vulkan/renderer/vk_renderer.hpp"

namespace pop::systems {

/// @class SimulationDataCsvWriter
/// @brief Writes a snapshot of simulation data to a CSV file.
class SimulationDataCsvWriter {
public:
    SimulationDataCsvWriter();

    /// @brief Writes a snapshot of simulation data to a CSV file.
    /// @param data The snapshot of simulation data to write.
    /// @param filepath The path of the CSV file to write to.
    static auto write_to_file(const vulkan::renderer::SimulationDataSnapshot& data, const std::filesystem::path& filepath) -> void;
};

} // namespace pop::systems