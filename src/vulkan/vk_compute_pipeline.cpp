#include "vk_compute_pipeline.hpp"

namespace pop::vulkan {

VulkanComputePipeline::VulkanComputePipeline(vk::raii::Pipeline&& pipeline)
    : m_pipeline(std::move(pipeline)) {}

} // namespace pop::vulkan