#pragma once
#include "shaders/shared_consts.hpp"
#include "simulation_render_graph_passes.hpp"
#include "vulkan/vk_context.hpp"

#include <cstdint>
#include <print>

namespace pop::vulkan::renderer {

/// @brief Returns the group size that will be used for radix sort histogram and scatter shaders.
constexpr std::uint32_t get_radix_sort_group_size() {
    std::uint32_t clamp_min = VulkanContext::get().physical_device_vulkan13_properties().minSubgroupSize;
    std::uint32_t clamp_max = VulkanContext::get().physical_device_vulkan13_properties().maxSubgroupSize;
    return std::clamp(32u, clamp_min, clamp_max);
}

/// @brief Returns the number of keys that will be processed by each radix sort histogram build shader group.
constexpr std::uint32_t get_radix_sort_keys_count_per_group() {
    return get_radix_sort_group_size() * shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_BUILD_KEYS_PER_THREAD;
}

/// @brief Returns the number of bytes of memory needed for the radix sort group histograms.
constexpr std::uint64_t compute_memory_needed_for_radix_sort_group_histograms(std::uint32_t object_count) {
    std::uint32_t group_count = div_ceil(object_count, get_radix_sort_keys_count_per_group());

    std::uint64_t total_dwords = static_cast<std::uint64_t>(group_count)
        * static_cast<std::uint64_t>(shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS);

    std::uint64_t total_bytes = total_dwords * sizeof(uint32_t);
    return total_bytes;
}

} // namespace pop::vulkan::renderer