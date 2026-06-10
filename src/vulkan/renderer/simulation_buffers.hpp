#pragma once
#include "vulkan/vk_buffer.hpp"

namespace pop::vulkan::renderer {

/// @struct DynamicallySizedSimulationBuffers
/// @brief Manages buffer handles that are dynamically sized based on the number of simulation objects.
struct DynamicallySizedSimulationBuffers {
    VulkanBuffer simulation_objects;
    VulkanBuffer simulation_objects_scratch;
    VulkanBuffer simulation_objects_flags;
    VulkanBuffer draw_local_instance_ids;
    VulkanBuffer instance_data;
    VulkanBuffer acceleration_grid_sort_keys;
    VulkanBuffer acceleration_grid_sort_keys_scratch;
    VulkanBuffer acceleration_grid_sort_values;
    VulkanBuffer acceleration_grid_sort_values_scratch;
    VulkanBuffer acceleration_grid_sort_group_local_histograms;
};

/// @struct StaticallySizedSimulationBuffers
/// @brief Manages buffer handles that have a constant size and are never resized.
struct StaticallySizedSimulationBuffers {
    VulkanBuffer indirect_draw_commands;
    uint64_t     indirect_draw_commands_generation = 0;
    VulkanBuffer acceleration_grid_sort_global_histogram;
    VulkanBuffer acceleration_grid_tile_start_indices;
    VulkanBuffer acceleration_grid_tile_end_indices;
};

/// @class SimulationBuffersManager
/// @brief Manages buffers used by the simulation renderer.
class SimulationBuffersManager {
public:
    SimulationBuffersManager(StaticallySizedSimulationBuffers&& statically_sized_buffers, DynamicallySizedSimulationBuffers&& dynamically_sized_buffers);

    /// @brief Creates a new SimulationBuffersManager instance.
    /// @param max_draw_commands The maximum number of draw commands that can be stored in the indirect draw commands buffer.
    /// @param radix_sort_histogram_buckets The number of buckets in the radix sort histogram.
    /// @param acceleration_grid_tile_indices_count The number of indices in the acceleration grid tile indices buffer.
    /// @param initial_max_object_count The initial maximum number of simulation objects.
    /// @returns A new SimulationBuffersManager instance.
    static auto create(uint32_t max_draw_commands, uint32_t radix_sort_histogram_buckets, uint32_t acceleration_grid_tile_indices_count, uint32_t initial_max_object_count) -> SimulationBuffersManager;

    /// @brief Resizes the dynamically sized buffers to match the new maximum number of simulation objects.
    /// @param max_object_count The new maximum number of simulation objects.
    /// @note This function should be called whenever the maximum number of simulation objects changes.
    auto refit(uint32_t max_object_count) -> void;

    [[nodiscard]] constexpr auto simulation_objects()                            noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.simulation_objects; }
    [[nodiscard]] constexpr auto simulation_objects_scratch()                    noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.simulation_objects_scratch; }
    [[nodiscard]] constexpr auto simulation_objects_flags()                      noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.simulation_objects_flags; }
    [[nodiscard]] constexpr auto draw_local_instance_ids()                       noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.draw_local_instance_ids; }
    [[nodiscard]] constexpr auto instance_data()                                 noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.instance_data; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_keys()                   noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.acceleration_grid_sort_keys; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_keys_scratch()           noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.acceleration_grid_sort_keys_scratch; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_values()                 noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.acceleration_grid_sort_values; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_values_scratch()         noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.acceleration_grid_sort_values_scratch; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_group_local_histograms() noexcept -> VulkanBuffer& { return m_dynamically_sized_buffers.acceleration_grid_sort_group_local_histograms; }

    [[nodiscard]] constexpr auto indirect_draw_commands()                                         noexcept -> VulkanBuffer& { return m_statically_sized_buffers.indirect_draw_commands; }
    [[nodiscard]] constexpr auto indirect_draw_commands_generation()                        const noexcept -> uint64_t { return m_statically_sized_buffers.indirect_draw_commands_generation; }
                  constexpr auto set_indirect_draw_commands_generation(uint64_t generation)       noexcept -> void { m_statically_sized_buffers.indirect_draw_commands_generation = generation; }
    [[nodiscard]] constexpr auto acceleration_grid_sort_global_histogram()                        noexcept -> VulkanBuffer& { return m_statically_sized_buffers.acceleration_grid_sort_global_histogram; }
    [[nodiscard]] constexpr auto acceleration_grid_tile_start_indices()                           noexcept -> VulkanBuffer& { return m_statically_sized_buffers.acceleration_grid_tile_start_indices; }
    [[nodiscard]] constexpr auto acceleration_grid_tile_end_indices()                             noexcept -> VulkanBuffer& { return m_statically_sized_buffers.acceleration_grid_tile_end_indices; }

private:
    static auto create_statically_sized_buffers(uint32_t max_draw_commands, uint32_t radix_sort_histogram_buckets, uint32_t acceleration_grid_tile_indices_count) -> StaticallySizedSimulationBuffers;
    static auto create_dynamically_sized_buffers(uint32_t max_object_count) -> DynamicallySizedSimulationBuffers;

    StaticallySizedSimulationBuffers m_statically_sized_buffers;
    DynamicallySizedSimulationBuffers m_dynamically_sized_buffers;
};

} // namespace pop::vulkan::renderer
