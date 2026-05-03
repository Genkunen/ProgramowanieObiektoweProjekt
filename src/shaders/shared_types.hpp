#pragma once

namespace pop::shaders {

#ifdef __cplusplus
#include <glm/glm.hpp>

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;
using uint = uint32_t;

using float4x4 = glm::mat4;

#endif

struct Vertex {
    float3 position;
    float3 normal;
    float2 texcoord;
};

struct MeshAllocationData {
    uint vertex_count;
    uint index_count;
    uint vertex_offset;
    uint first_index;
};

struct SimulationObject {
    float2 position;
    float2 velocity;
    uint mesh_index;
    uint randseed;
};

struct PreparedSimulationObject {
    float4x4 transform;
    uint randseed;
};

struct SimulationData {
    float4x4 projview;
    float delta_time;
    float simulation_time_since_start;
};

}