#include "simulation_buffers.hpp"

#include "radix_sort.hpp"
#include "shaders/shared_consts.hpp"
#include "shaders/shared_types.hpp"

namespace pop::vulkan::renderer {

SimulationBuffersManager::SimulationBuffersManager(StaticallySizedSimulationBuffers&& statically_sized_buffers, DynamicallySizedSimulationBuffers&& dynamically_sized_buffers)
    : m_statically_sized_buffers(std::move(statically_sized_buffers)), m_dynamically_sized_buffers(std::move(dynamically_sized_buffers)) {}

auto SimulationBuffersManager::create(uint32_t max_draw_commands, uint32_t radix_sort_histogram_buckets, uint32_t acceleration_grid_tile_indices_count,
    uint32_t initial_max_object_count) -> SimulationBuffersManager {
    auto statically_sized_buffers = create_statically_sized_buffers(max_draw_commands, radix_sort_histogram_buckets, acceleration_grid_tile_indices_count);
    auto dynamically_sized_buffers = create_dynamically_sized_buffers(initial_max_object_count);
    return SimulationBuffersManager(std::move(statically_sized_buffers), std::move(dynamically_sized_buffers));
}

auto SimulationBuffersManager::refit(uint32_t max_object_count) -> void {
    m_dynamically_sized_buffers = create_dynamically_sized_buffers(max_object_count);
}

auto SimulationBuffersManager::create_statically_sized_buffers(uint32_t max_draw_commands, uint32_t radix_sort_histogram_buckets, uint32_t acceleration_grid_tile_indices_count) -> StaticallySizedSimulationBuffers {
    auto indirect_draw_commands = VulkanBuffer::builder()
        .set_size(max_draw_commands * sizeof(vk::DrawIndexedIndirectCommand))
        .set_usage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_global_histogram = VulkanBuffer::builder()
        .set_size(radix_sort_histogram_buckets * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_tile_start_indices = VulkanBuffer::builder()
        .set_size(acceleration_grid_tile_indices_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_tile_end_indices = VulkanBuffer::builder()
        .set_size(acceleration_grid_tile_indices_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    return StaticallySizedSimulationBuffers {
        .indirect_draw_commands                  = std::move(indirect_draw_commands),
        .indirect_draw_commands_generation       = 0,
        .acceleration_grid_sort_global_histogram = std::move(acceleration_grid_sort_global_histogram),
        .acceleration_grid_tile_start_indices    = std::move(acceleration_grid_tile_start_indices),
        .acceleration_grid_tile_end_indices      = std::move(acceleration_grid_tile_end_indices)
    };
}

auto SimulationBuffersManager::create_dynamically_sized_buffers(uint32_t max_object_count) -> DynamicallySizedSimulationBuffers {
    auto simulation_objects = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    auto simulation_objects_scratch = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto draw_local_instance_ids = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto instance_data = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(shaders::PreparedSimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_keys = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_keys_scratch = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_values = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_values_scratch = VulkanBuffer::builder()
        .set_size(max_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto acceleration_grid_sort_group_local_histograms = VulkanBuffer::builder()
        .set_size(compute_memory_needed_for_radix_sort_group_histograms(max_object_count))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    return DynamicallySizedSimulationBuffers {
        .simulation_objects                            = std::move(simulation_objects),
        .simulation_objects_scratch                    = std::move(simulation_objects_scratch),
        .draw_local_instance_ids                       = std::move(draw_local_instance_ids),
        .instance_data                                 = std::move(instance_data),
        .acceleration_grid_sort_keys                   = std::move(acceleration_grid_sort_keys),
        .acceleration_grid_sort_keys_scratch           = std::move(acceleration_grid_sort_keys_scratch),
        .acceleration_grid_sort_values                 = std::move(acceleration_grid_sort_values),
        .acceleration_grid_sort_values_scratch         = std::move(acceleration_grid_sort_values_scratch),
        .acceleration_grid_sort_group_local_histograms = std::move(acceleration_grid_sort_group_local_histograms)
    };
}

} // namespace pop::vulkan::renderer