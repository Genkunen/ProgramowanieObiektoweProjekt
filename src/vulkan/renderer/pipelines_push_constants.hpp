#pragma once
#include "../vk_prelude.hpp"
#include <glm/glm.hpp>

#ifdef _MSC_VER
# pragma pack(push, 1)
# define PACKED
#else
# define PACKED __attribute((packed))
#endif

namespace pop::vulkan::renderer {

struct UploadMeshesCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress mesh_allocations;
    uint32_t mesh_count;
} PACKED;

struct RandomEventsCSPushConstants {
    vk::DeviceAddress simulation_objects;
    vk::DeviceAddress simulation_object_flags;
    uint32_t object_count;
    uint32_t event_randseed;
    glm::vec2 simulation_bounds;
} PACKED;

struct ClearInstanceCountCSPushConstants {
    vk::DeviceAddress draw_commands;
    uint32_t mesh_count;
} PACKED;

struct SimulationStepCSPushConstants {
    vk::DeviceAddress simulation_data;
    vk::DeviceAddress objects;
    vk::DeviceAddress dst_updated_objects;
    vk::DeviceAddress object_flags;
    glm::vec2 simulation_bounds;
    uint32_t object_count;
    float water_current_strength;
} PACKED;

struct SimulationAccelerationGridSortPrepareCSPushConstants {
    vk::DeviceAddress objects;
    vk::DeviceAddress object_flags;
    vk::DeviceAddress sort_keys;
    vk::DeviceAddress sort_values;
    float grid_cell_size;
    uint32_t grid_width;
    uint32_t object_count;
} PACKED;

struct SimulationAccelerationGridRadixSortHistogramCSPushConstants {
    vk::DeviceAddress sort_keys;

    vk::DeviceAddress global_histogram;
    vk::DeviceAddress group_local_histograms;

    uint32_t group_count;
    uint32_t keys_count;
    uint32_t radix_bit_shift;
} PACKED;

struct SimulationAccelerationGridRadixSortPrefixSumCSPushConstants {
    vk::DeviceAddress global_histogram;
    vk::DeviceAddress group_local_histograms;

    uint32_t group_count;
} PACKED;

// TODO: reduce size (<= 13 DWORDs)
struct SimulationAccelerationGridRadixSortScatterCSPushConstants {
    vk::DeviceAddress sort_keys;
    vk::DeviceAddress sort_values;
    vk::DeviceAddress dst_sort_keys;
    vk::DeviceAddress dst_sort_values;

    vk::DeviceAddress global_histogram;
    vk::DeviceAddress group_local_histograms;

    uint32_t group_count;
    uint32_t keys_count;
    uint32_t radix_bit_shift;
} PACKED;

struct SimulationAccelerationGridBoundScanCSPushConstants {
    vk::DeviceAddress sort_keys;
    vk::DeviceAddress tile_start_indices;
    vk::DeviceAddress tile_end_indices;
    uint32_t keys_count;
} PACKED;

// TODO: reduce size (<= 13 DWORDs)
struct SimulationInfluenceStepCSPushConstants {
    vk::DeviceAddress simulation_data;
    vk::DeviceAddress objects;
    vk::DeviceAddress dst_objects;
    vk::DeviceAddress object_flags;
    vk::DeviceAddress acceleration_grid_values;
    vk::DeviceAddress acceleration_grid_tile_start_indices;
    vk::DeviceAddress acceleration_grid_tile_end_indices;
    float grid_cell_size;
    uint32_t grid_width;
    uint32_t grid_height;
    uint32_t object_count;
} PACKED;

struct BuildIndirectInstanceCountCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress drawlocal_instance_indices;
    vk::DeviceAddress simulation_objects;
    vk::DeviceAddress simulation_object_flags;
    uint32_t object_count;
} PACKED;

struct BuildIndirectFirstInstanceCSPushConstants {
    vk::DeviceAddress draw_commands;
    uint32_t draw_commands_count;
} PACKED;

struct BuildInstanceBufferCSPushConstants {
    vk::DeviceAddress draw_commands;
    vk::DeviceAddress drawlocal_instance_indices;
    vk::DeviceAddress simulation_data;
    vk::DeviceAddress simulation_objects;
    vk::DeviceAddress simulation_object_flags;
    vk::DeviceAddress instance_data;
    uint32_t object_count;
} PACKED;

struct BackgroundVSFSPushConstants {
    vk::DeviceAddress simulation_data;
    float base_color[3];
    float scale;
    uint32_t max_iterations;
    float caustic_intensity;
    float ray_intensity;
    float surface_y;
    float depth_range;
    float deep_color[3];
    float vignette_size;
} PACKED;

}

#ifdef _MSC_VER
# pragma pack(pop)
#endif