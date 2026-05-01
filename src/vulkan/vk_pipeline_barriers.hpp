#pragma once
#include "vulkan/vk_prelude.hpp"

namespace pop::vulkan {

class VulkanPipelineBarriers {
public:
    static auto builder() -> VulkanPipelineBarriers {
        return VulkanPipelineBarriers();
    }

    auto insert_image_memory_barrier(vk::Image image, vk::ImageLayout src_layout, vk::PipelineStageFlags2 src_stage, vk::AccessFlags2 src_access, vk::ImageLayout dst_layout, vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access, vk::ImageSubresourceRange subresource_range) -> VulkanPipelineBarriers& {
        auto barrier = vk::ImageMemoryBarrier2{}
            .setOldLayout(src_layout)
            .setSrcStageMask(src_stage)
            .setSrcAccessMask(src_access)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setNewLayout(dst_layout)
            .setDstStageMask(dst_stage)
            .setDstAccessMask(dst_access)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setImage(image)
            .setSubresourceRange(subresource_range);
        m_image_memory_barriers.push_back(barrier);
        return *this;
    }

    auto insert_buffer_memory_barrier(vk::Buffer buffer, uint64_t size, uint64_t offset, vk::PipelineStageFlags2 src_stage, vk::AccessFlags2 src_access, vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access) -> VulkanPipelineBarriers& {
        auto barrier = vk::BufferMemoryBarrier2{}
            .setSize(size)
            .setOffset(offset)
            .setSrcStageMask(src_stage)
            .setSrcAccessMask(src_access)
            .setDstStageMask(dst_stage)
            .setDstAccessMask(dst_access)
            .setBuffer(buffer);
        m_buffer_memory_barriers.push_back(barrier);
        return *this;
    }

    auto flush(const vk::CommandBuffer& cb) -> void {
        auto dependency_info = vk::DependencyInfo{}
            .setImageMemoryBarriers(m_image_memory_barriers)
            .setBufferMemoryBarriers(m_buffer_memory_barriers);
        cb.pipelineBarrier2(dependency_info);
    }

private:
    std::vector<vk::ImageMemoryBarrier2> m_image_memory_barriers;
    std::vector<vk::BufferMemoryBarrier2> m_buffer_memory_barriers;
};


}