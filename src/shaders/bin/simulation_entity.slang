#include "../shared_types.hpp"

struct PushConstants {
    pop::shaders::Vertex* vertices;
    pop::shaders::PreparedSimulationObject* object_data;
    pop::shaders::SimulationData* simulation_data;
};

[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    nointerpolation uint randseed;
};

float3 fish_displace_model_vertices(float3 pos, uint seed) {
    float tbase = seed / 4294967296.0f;
    float tweight = exp(-pos.x * 2.0);
    float t = sin(1.5 * pos.x + tbase + 4.0 * push_constants.simulation_data->simulation_time_since_start) * tweight * 0.1f;

    return float3(pos.x, pos.y + t, pos.z);
}

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID, uint instance_id : SV_VulkanInstanceID, uint draw_id : SV_DrawIndex) {
    let vertex = push_constants.vertices[vertex_id];
    let object_data = push_constants.object_data[instance_id];

    float3 position = fish_displace_model_vertices(vertex.position, push_constants.object_data[instance_id].randseed);

    VSOutput output;
    output.position = mul(object_data.transform, float4(position, 1.0));
    output.color = vertex.normal;
    output.randseed = push_constants.object_data[instance_id].randseed;
    return output;
}

float3 generate(uint3 x) {
    uint3 prec = x * 747796405u + 2891336453u;

    float3 precf = prec / 4294967296.0f;
    return precf;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {

    uint3 seed = uint3(input.randseed, input.randseed >> 4, input.randseed >> 9);
    float3 col = 0.25 * input.color + 0.75 * generate(seed);

    return float4(col, 1.0);
}