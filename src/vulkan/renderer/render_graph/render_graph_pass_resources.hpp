#pragma once
#include "vulkan/vk_buffer.hpp"
#include "vulkan/vk_image.hpp"

#include <unordered_map>

namespace pop::vulkan::renderer::render_graph {

enum class BufferResourceIdentifier {
    FrameLocalMeshInfoStagingBuffer,

    SimulationDrawIndirectCommands,
    DrawCommandsObjectInstanceOffsets,

    FrameLocalSimulationData,
    SimulationObjects,
    SimulationNextObjects,

    ObjectsInstanceBuffer,
};

enum class ImageResourceIdentifier {
    MainRenderTarget,
    DepthBuffer,
};

class PassResources {
public:
    PassResources() = default;

    auto inject_buffer(BufferResourceIdentifier identifier, VulkanBuffer&& buffer) -> void;
    auto inject_image(ImageResourceIdentifier identifier, VulkanImage&& image) -> void;

    auto get_buffer_by_identifier(BufferResourceIdentifier identifier) const -> const VulkanBuffer&;
    auto get_image_by_identifier(ImageResourceIdentifier identifier) const -> const VulkanImage&;

private:
    std::unordered_map<BufferResourceIdentifier, VulkanBuffer> m_buffers;
    std::unordered_map<ImageResourceIdentifier, VulkanImage> m_images;
};

}