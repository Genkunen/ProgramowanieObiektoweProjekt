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