#include "vk_pipeline_layout.hpp"

namespace pop::vulkan {

VulkanPipelineLayout::VulkanPipelineLayout(vk::raii::PipelineLayout&& pipeline_layout)
    : m_pipeline_layout(std::move(pipeline_layout)) {}

} // namespace pop::vulkan