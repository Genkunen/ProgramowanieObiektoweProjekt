#pragma once
#include "vk_context.hpp"
#include "vk_prelude.hpp"

namespace pop::vulkan {

class VulkanBufferBuilder;

/// @class VulkanBuffer
/// @brief Wrapper around a Vulkan @c vk::raii::Buffer and VMA @c vma::raii::Allocation objects.
class VulkanBuffer {
public:
    VulkanBuffer(vk::raii::Buffer&& buffer, vma::raii::Allocation&& allocation, uint8_t* memory_host_ptr, vk::DeviceAddress memory_device_ptr, uint64_t size);

    [[nodiscard]] constexpr static auto builder() -> VulkanBufferBuilder;

    /// @brief Returns the underlying Vulkan buffer object.
    [[nodiscard]] auto vk_buffer()         const noexcept -> const vk::raii::Buffer& { return m_buffer; }
    /// @brief Returns the underlying VMA allocation object.
    [[nodiscard]] auto vma_allocation()    const noexcept -> const vma::raii::Allocation& { return m_allocation; }
    /// @brief Returns the host pointer to the memory allocation for this buffer.
    /// @note The returned pointer is valid only if the buffer was created with @c map_for_sequential_write() or @c map_for_random_access()
    [[nodiscard]] auto memory_host_ptr()   const noexcept -> uint8_t* { return m_memory_host_ptr; }
    /// @brief Returns the device address pointing to the memory allocation for this buffer.
    /// @note The returned address is valid only if the buffer was created with @c vk::BufferUsageFlagBits::eShaderDeviceAddress passed to @c set_usage()
    [[nodiscard]] auto memory_device_ptr() const noexcept -> vk::DeviceAddress { return m_memory_device_ptr; }
    /// @brief Returns the size of the buffer in bytes.
    [[nodiscard]] auto size()              const noexcept -> uint64_t { return m_size; }

private:
    vk::raii::Buffer      m_buffer;
    vma::raii::Allocation m_allocation;

    uint8_t*          m_memory_host_ptr;
    vk::DeviceAddress m_memory_device_ptr;

    uint64_t m_size;
};

/// @class VulkanBufferBuilder
/// @brief A builder for a @c VulkanBuffer object.
class VulkanBufferBuilder {
public:
    constexpr VulkanBufferBuilder() = default;

    [[nodiscard]] constexpr auto set_size(uint64_t size)                         noexcept -> VulkanBufferBuilder& { m_size = size; return *this; }
    [[nodiscard]] constexpr auto set_usage(vk::BufferUsageFlags usage)           noexcept -> VulkanBufferBuilder& { m_usage = usage; return *this; }
    [[nodiscard]] constexpr auto set_memory_usage(vma::MemoryUsage memory_usage) noexcept -> VulkanBufferBuilder& { m_memory_usage = memory_usage; return *this; }
    /// @brief Maps the buffer for sequential write access.
    /// @note Either this function or @c map_for_random_access() must be used before creating the buffer if you wish to access the buffer memory from the host.
    /// @note This maps the buffer memory to host such that it's the best fit for sequential writes. Random writes or any reads are legal, but there might be
    ///     a performance penalty for accessing the buffer memory in that manner.
    /// @note Do not use this function together with @c map_for_random_access().
    [[nodiscard]] constexpr auto map_for_sequential_write() noexcept -> VulkanBufferBuilder& { m_allocation_flags |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite; return *this; }

    /// @brief Maps the buffer for random access.
    /// @note Either this function or @c map_for_sequential_write() must be used before creating the buffer if you wish to access the buffer memory from the host.
    /// @note Do not use this function together with @c map_for_sequential_write().
    [[nodiscard]] constexpr auto map_for_random_access() noexcept -> VulkanBufferBuilder& { m_allocation_flags |= vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom; return *this; }

    /// @brief Builds the buffer.
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

        auto [allocation, buffer] = VulkanContext::get().vma_allocator()
            .createBuffer(buffer_create_info, allocation_create_info)
            .split();

        uint8_t* memory_host_ptr{};
        if (m_allocation_flags & vma::AllocationCreateFlagBits::eMapped) {
            memory_host_ptr = static_cast<uint8_t*>(allocation.getInfo().pMappedData);
        }

        vk::DeviceAddress memory_device_ptr = vk::DeviceAddress(0);
        if (m_usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            auto bda_info = vk::BufferDeviceAddressInfo()
                .setBuffer(buffer);
            memory_device_ptr = VulkanContext::get().vk_device().getBufferAddress(bda_info);
        }

        return VulkanBuffer(std::move(buffer), std::move(allocation), memory_host_ptr, memory_device_ptr, m_size);
    }

private:
    uint64_t m_size;
    vk::BufferUsageFlags m_usage;
    vma::AllocationCreateFlags m_allocation_flags;
    vma::MemoryUsage m_memory_usage;
};

constexpr auto VulkanBuffer::builder() -> VulkanBufferBuilder { return VulkanBufferBuilder(); }

} // namespace pop::vulkan
