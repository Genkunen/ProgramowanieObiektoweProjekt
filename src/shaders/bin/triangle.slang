#include "../shared_types.hpp"

struct PushConstants {
    pop::shaders::Vertex* vertices;
    pop::shaders::SimulationObject* simulation_objects;
};

[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

float hash1(uint x)
{
    x ^= x * 0x27d4eb2d;
    x ^= x >> 15;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    return frac(x / 4294967296.0); // divide by 2^32
}

// 2D random in [-1,1]
float2 randomOffset(uint seed)
{
    float r1 = hash1(seed);
    float r2 = hash1(seed * 1664525u + 1013904223u); // LCG step for decorrelation

    return float2(r1, r2) * 2.0 - 1.0;
}

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID, uint instance_id : SV_VulkanInstanceID) {
    let vertex = push_constants.vertices[vertex_id];
    let object = push_constants.simulation_objects[instance_id];

    VSOutput output;
    output.position = 
        float4(vertex.position + float3(object.position, 0.0), 1.0);
    output.color = vertex.normal;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
