#include "vk_buffer.hpp"

namespace pop::vulkan {

VulkanBuffer::VulkanBuffer(vk::raii::Buffer&& buffer, vma::raii::Allocation&& allocation, uint8_t* memory_host_ptr, vk::DeviceAddress memory_device_ptr, uint64_t size)
    : m_buffer(std::move(buffer)), m_allocation(std::move(allocation)), m_memory_host_ptr(memory_host_ptr), m_memory_device_ptr(memory_device_ptr), m_size(size) {}

} // namespace pop::vulkan