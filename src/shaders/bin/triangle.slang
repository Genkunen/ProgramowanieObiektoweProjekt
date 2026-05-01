#include "../shared_types.hpp"

struct PushConstants {
    pop::shaders::Vertex* vertices;
};

[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID) {
    let vertex = push_constants.vertices[vertex_id];

    VSOutput output;
    output.position = float4(vertex.position, 1.0);
    output.color = vertex.normal;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}