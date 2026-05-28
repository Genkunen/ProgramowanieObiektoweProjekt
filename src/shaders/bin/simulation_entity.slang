import "../shared_code_shim";

struct PushConstants {
    pop::shaders::Vertex* vertices;
    pop::shaders::PreparedSimulationObject* object_data;
    pop::shaders::SimulationData* simulation_data;
};

[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    float2 texcoord : TEXCOORD0;
    nointerpolation uint texindex : TEXINDEX;
};

[[vk::binding(0, 0)]] Texture2D global_textures[];
[[vk::binding(1, 0)]] SamplerState global_sampler;


float3 fish_displace_model_vertices(float3 pos, uint seed) {
    float tbase = seed / 4294967296.0f;
    float tweight = exp(-pos.x * 2.0);
    float t = sin(1.5 * pos.x + tbase + 4.0 * push_constants.simulation_data->simulation_time_since_start) * tweight * 0.02f;

    return float3(pos.x, pos.y + t, pos.z);
}

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID, uint instance_id : SV_VulkanInstanceID, uint draw_id : SV_DrawIndex) {
    let vertex = push_constants.vertices[vertex_id];
    let object_data = push_constants.object_data[instance_id];

    float3 position = fish_displace_model_vertices(vertex.position, push_constants.object_data[instance_id].randseed);

    float2 texcoord = vertex.texcoord;
    texcoord.x = (1.0f - texcoord.x);

    VSOutput output;
    output.position = mul(object_data.transform, float4(position, 1.0));
    output.texcoord = texcoord;
    output.texindex = draw_id;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    float4 color = global_textures[input.texindex].Sample(global_sampler, input.texcoord);
    return color;
}
