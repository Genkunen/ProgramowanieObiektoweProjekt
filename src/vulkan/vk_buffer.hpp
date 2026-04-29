#pragma once
#include "vk_context.hpp"
#include "vk_prelude.hpp"

namespace pop::vulkan {

class VulkanBufferBuilder;
class VulkanBuffer {
public:
    VulkanBuffer(vk::raii::Buffer&& buffer, vma::raii::Allocation&& allocation, uint8_t* memory_host_ptr, vk::DeviceAddress memory_device_ptr);

    [[nodiscard]] constexpr static auto builder() -> VulkanBufferBuilder;

    [[nodiscard]] auto vk_buffer() const -> const vk::raii::Buffer& { return m_buffer; }
    [[nodiscard]] auto vma_allocation() const -> const vma::raii::Allocation& { return m_allocation; }
    [[nodiscard]] auto memory_host_ptr() const -> uint8_t* { return m_memory_host_ptr; }
    [[nodiscard]] auto memory_device_ptr() const -> vk::DeviceAddress { return m_memory_device_ptr; }
    [[nodiscard]] auto size() const -> uint64_t { return m_size; }

private:
    vk::raii::Buffer m_buffer;
    vma::raii::Allocation m_allocation;

    uint8_t*          m_memory_host_ptr;
    vk::DeviceAddress m_memory_device_ptr;

    uint64_t m_size;
};

class VulkanBufferBuilder {
public:
    constexpr VulkanBufferBuilder() = default;

    [[nodiscard]] constexpr auto set_size(uint64_t size) noexcept -> VulkanBufferBuilder& { m_size = size; return *this; }
    [[nodiscard]] constexpr auto set_usage(vk::BufferUsageFlags usage) noexcept -> VulkanBufferBuilder& { m_usage = usage; return *this; }
    [[nodiscard]] constexpr auto set_memory_usage(vma::MemoryUsage memory_usage) noexcept -> VulkanBufferBuilder& { m_memory_usage = memory_usage; return *this; }
    [[nodiscard]] constexpr auto map_for_sequential_write() noexcept -> VulkanBufferBuilder& { m_allocation_flags |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite; return *this; }
    [[nodiscard]] constexpr auto map_for_random_access() noexcept -> VulkanBufferBuilder& { m_allocation_flags |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom; return *this; }
    [[nodiscard]] constexpr auto build() -> VulkanBuffer {
        auto graphics_queue_family_index = VulkanContext::get().vk_graphics_queue_family();

        auto buffer_create_info = vk::BufferCreateInfo()
            .setSize(m_size)
            .setUsage(m_usage)
            .setQueueFamilyIndices(graphics_queue_family_index)
            .setSharingMode(vk::SharingMode::eExclusive);

        auto allocation_create_info = vma::AllocationCreateInfo()
            .setUsage(m_memory_usage)
            .setFlags(m_allocation_flags);

        auto [allocation, buffer] = VulkanContext::get().vma_allocator().createBuffer(buffer_create_info, allocation_create_info).split();

        uint8_t* memory_host_ptr = nullptr;
        if (m_allocation_flags & vma::AllocationCreateFlagBits::eMapped) {
            memory_host_ptr = static_cast<uint8_t*>(allocation.getInfo().pMappedData);
        }

        vk::DeviceAddress memory_device_ptr = vk::DeviceAddress(nullptr);
        if (m_usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            auto bda_info = vk::BufferDeviceAddressInfo()
                .setBuffer(buffer);
            memory_device_ptr = VulkanContext::get().vk_device().getBufferAddress(bda_info);
        }

        return VulkanBuffer(std::move(buffer), std::move(allocation), memory_host_ptr, memory_device_ptr);
    }

private:
    uint64_t m_size;
    vk::BufferUsageFlags m_usage;
    vma::AllocationCreateFlags m_allocation_flags;
    vma::MemoryUsage m_memory_usage;
};

constexpr auto VulkanBuffer::builder() -> VulkanBufferBuilder { return VulkanBufferBuilder(); }

}