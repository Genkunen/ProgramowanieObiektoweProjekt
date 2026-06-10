#pragma once
#include "vulkan/vk_buffer.hpp"
#include "vulkan/vk_image.hpp"

#include <unordered_map>

namespace pop::vulkan::renderer::render_graph {

/// @enum BufferResourceIdentifier
/// @brief Identifiers for buffers used in the render graph.
enum class BufferResourceIdentifier {
    FrameLocalMeshInfoStagingBuffer,

    SimulationDrawIndirectCommands,
    DrawCommandsObjectInstanceOffsets,

    FrameLocalSimulationData,
    SimulationObjects,
    SimulationObjectsScratch,

    SimulationObjectsFlags,

    AccelerationGridSortKeys,
    AccelerationGridSortValues,
    AccelerationGridSortKeysScratch,
    AccelerationGridSortValuesScratch,
    AccelerationGridSortGlobalHistogram,
    AccelerationGridSortGroupLocalHistograms,

    AccelerationGridCellsStartIndices,
    AccelerationGridCellsEndIndices,

    ObjectsInstanceBuffer,
};

/// @enum ImageResourceIdentifier
/// @brief Identifiers for images used in the render graph.
enum class ImageResourceIdentifier {
    MainRenderTarget,
    DepthBuffer,
};

/// @class PassResources
/// @brief Map of resource handles used by a render graph pass.
class PassResources {
public:
    PassResources() = default;

    auto inject_buffer(BufferResourceIdentifier identifier, VulkanBuffer& buffer) -> void;
    auto inject_image(ImageResourceIdentifier identifier, VulkanImage& image)     -> void;

    auto get_buffer_by_identifier(BufferResourceIdentifier identifier) const -> const VulkanBuffer&;
    auto get_image_by_identifier(ImageResourceIdentifier identifier)   const -> const VulkanImage&;

private:
    std::unordered_map<BufferResourceIdentifier, std::reference_wrapper<VulkanBuffer>> m_buffers;
    std::unordered_map<ImageResourceIdentifier, std::reference_wrapper<VulkanImage>> m_images;
};

} // namespace pop::vulkan::renderer::render_graph