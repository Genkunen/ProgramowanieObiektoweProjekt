#pragma once
#include "device_ptr.hpp"
#include "shared_types.hpp"

#ifdef __cplusplus
#include <glm/glm.hpp>
#else
// for vk::DrawIndexedIndirectCommand
import "vk_structs/draw_indirect";
#endif

namespace pop::shaders {

#ifdef __cplusplus

# ifdef _MSC_VER
#  pragma pack(push, 1)
#  define PACKED
# else
#  define PACKED __attribute((packed))
# endif

using uint = uint32_t;
using float2 = glm::vec2;
using float3 = glm::vec3;

#else

# define PACKED

#endif

struct UploadMeshesCSPushConstants {
    DevicePtr<vk::DrawIndexedIndirectCommand> draw_commands;
    DevicePtr<pop::shaders::MeshAllocationData> mesh_allocations;
    uint mesh_count;
} PACKED;

struct RandomEventsCSPushConstants {
    DevicePtr<pop::shaders::SimulationObject> simulation_objects;
    DevicePtr<uint> simulation_object_flags;
    uint object_count;
    uint event_randseed;
    float2 simulation_bounds;
} PACKED;

struct ClearInstanceCountCSPushConstants {
    DevicePtr<vk::DrawIndexedIndirectCommand> draw_commands;
    uint mesh_count;
} PACKED;

struct SimulationStepCSPushConstants {
    DevicePtr<pop::shaders::SimulationData> simulation_data;
    DevicePtr<pop::shaders::SimulationObject> objects;
    DevicePtr<pop::shaders::SimulationObject> dst_updated_objects;
    DevicePtr<uint> object_flags;
    float2 simulation_bounds;
    uint object_count;
    float water_current_strength;
} PACKED;

struct SimulationAccelerationGridSortPrepareCSPushConstants {
    DevicePtr<pop::shaders::SimulationObject> objects;
    DevicePtr<uint> object_flags;
    DevicePtr<uint> sort_keys;
    DevicePtr<uint> sort_values;
    float grid_cell_size;
    uint grid_width;
    uint object_count;
} PACKED;

struct SimulationAccelerationGridRadixSortHistogramCSPushConstants {
    DevicePtr<uint> sort_keys;

    DevicePtr<uint> group_local_histograms;

    uint group_count;
    uint keys_count;
    uint radix_bit_shift;
} PACKED;

struct SimulationAccelerationGridRadixSortPrefixSumCSPushConstants {
    DevicePtr<uint> global_histogram;
    DevicePtr<uint> group_local_histograms;

    uint group_count;
} PACKED;

struct SimulationAccelerationGridRadixSortGlobalPrefixSumCSPushConstants {
    DevicePtr<uint> global_histogram;
} PACKED;

// TODO: reduce size (<= 13 DWORDs)
struct SimulationAccelerationGridRadixSortScatterCSPushConstants {
    DevicePtr<uint> sort_keys;
    DevicePtr<uint> sort_values;
    DevicePtr<uint> dst_sort_keys;
    DevicePtr<uint> dst_sort_values;

    DevicePtr<uint> global_histogram;
    DevicePtr<uint> group_local_histograms;

    uint group_count;
    uint keys_count;
    uint radix_bit_shift;
} PACKED;

struct SimulationAccelerationGridBoundScanCSPushConstants {
    DevicePtr<uint> sort_keys;
    DevicePtr<uint> tile_start_indices;
    DevicePtr<uint> tile_end_indices;
    uint keys_count;
} PACKED;

// TODO: reduce size (<= 13 DWORDs)
struct SimulationInfluenceStepCSPushConstants {
    DevicePtr<pop::shaders::SimulationData> simulation_data;
    DevicePtr<pop::shaders::SimulationObject> objects;
    DevicePtr<pop::shaders::SimulationObject> dst_objects;
    DevicePtr<uint> object_flags;
    DevicePtr<uint> acceleration_grid_values;
    DevicePtr<uint> acceleration_grid_tile_start_indices;
    DevicePtr<uint> acceleration_grid_tile_end_indices;
    float grid_cell_size;
    uint grid_width;
    uint grid_height;
    uint object_count;
} PACKED;

struct BuildIndirectInstanceCountCSPushConstants {
    DevicePtr<vk::DrawIndexedIndirectCommand> draw_commands;
    DevicePtr<uint> drawlocal_instance_indices;
    DevicePtr<pop::shaders::SimulationObject> simulation_objects;
    DevicePtr<uint> simulation_object_flags;
    uint object_count;
} PACKED;

struct BuildIndirectFirstInstanceCSPushConstants {
    DevicePtr<vk::DrawIndexedIndirectCommand> draw_commands;
    uint draw_commands_count;
} PACKED;

struct BuildInstanceBufferCSPushConstants {
    DevicePtr<vk::DrawIndexedIndirectCommand> draw_commands;
    DevicePtr<uint> drawlocal_instance_indices;
    DevicePtr<pop::shaders::SimulationData> simulation_data;
    DevicePtr<pop::shaders::SimulationObject> simulation_objects;
    DevicePtr<uint> simulation_object_flags;
    DevicePtr<pop::shaders::PreparedSimulationObject> instance_data;
    uint object_count;
} PACKED;

struct BackgroundVSFSPushConstants {
    DevicePtr<pop::shaders::SimulationData> simulation_data;
    float3 base_color;
    float scale;
    uint max_iterations;
    float caustic_intensity;
    float ray_intensity;
    float surface_y;
    float depth_range;
    float3 deep_color;
    float vignette_size;
} PACKED;

struct FishVSPushConstants {
    DevicePtr<pop::shaders::Vertex> vertices;
    DevicePtr<pop::shaders::PreparedSimulationObject> object_data;
    DevicePtr<pop::shaders::SimulationData> simulation_data;
} PACKED;

} // namespace pop::shaders

#ifdef _MSC_VER
# pragma pack(pop)
#endif