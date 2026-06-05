#pragma once
#include "vulkan/renderer/vk_renderer.hpp"

namespace pop::systems {

class SimulationDataCsvWriter {
public:
    SimulationDataCsvWriter();

    static auto write_to_file(const vulkan::renderer::SimulationDataSnapshot& data, const std::filesystem::path& filename) -> void;
};

} // namespace pop::systems