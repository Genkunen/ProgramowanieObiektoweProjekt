#pragma once
#include "../vk_prelude.hpp"

namespace pop::vulkan::renderer {

struct UploadMeshesCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress mesh_allocations;
    uint32_t mesh_count;
} __attribute((packed));

struct ClearInstanceCountCSPushConstants {
    vk::DeviceAddress draw_commands;
    uint32_t mesh_count;
} __attribute((packed));

struct SimulationStepCSPushConstants {
    vk::DeviceAddress simulation_data;
    vk::DeviceAddress objects;
    vk::DeviceAddress dst_updated_objects;
    uint32_t object_count;
} __attribute((packed));

struct SimulationAccelerationGridSortPrepareCSPushConstants {
    vk::DeviceAddress objects;
    vk::DeviceAddress sort_keys;
    vk::DeviceAddress sort_values;
    float grid_cell_size;
    uint32_t grid_width;
    uint32_t object_count;
} __attribute((packed));

constexpr auto round_to_next_power_of_two(uint32_t x) -> uint32_t {
    if (x == 1) return 1;
    return 1 << (32 - __builtin_clz(x - 1));
}

struct SimulationAccelerationGridBitonicSortCSPushConstants {
    vk::DeviceAddress sort_keys;
    vk::DeviceAddress sort_values;
    uint32_t bitonic_sort_stage;
    uint32_t bitonic_sort_step;
    uint32_t keys_count;
};

struct BuildIndirectInstanceCountCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress drawlocal_instance_indices;
    vk::DeviceAddress simulation_objects;
    uint32_t object_count;
} __attribute((packed));

struct BuildIndirectFirstInstanceCSPushConstants {
    vk::DeviceAddress draw_commands;
    uint32_t draw_commands_count;
} __attribute((packed));

struct BuildInstanceBufferCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress drawlocal_instance_indices;
    vk::DeviceAddress simulation_data;
    vk::DeviceAddress simulation_objects;
    vk::DeviceAddress instance_data;
    uint32_t object_count;
} __attribute((packed));

}