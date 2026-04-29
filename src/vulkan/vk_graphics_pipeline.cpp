#include "vk_graphics_pipeline.hpp"

namespace pop::vulkan {

VulkanGraphicsPipeline::VulkanGraphicsPipeline(vk::raii::Pipeline&& pipeline)
    : m_pipeline(std::move(pipeline)) {}

} // namespace pop::vulkan