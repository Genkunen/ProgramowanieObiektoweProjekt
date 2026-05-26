import "../shared_code_shim";

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
};
[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

static float3 positions[6] = {
    { -1.0f, -1.0f, 0 },
    { -1.0f,  1.0f, 0 },
    {  1.0f, 1.0f,  0 },
    { 1.0f, 1.0f, 0 },
    { 1.0f, -1.0f, 0 },
    {  -1.0f, -1.0f,  0 },
};

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID) {
    VSOutput output;
    output.position = float4(positions[vertex_id], 1);
    output.color = float3(1, 0, 0);

    return output;
}

float3 generate(uint3 x) {
    uint3 prec = x * 747796405u + 2891336453u;

    float3 precf = prec / 4294967296.0f;
    return precf;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    return float4(1, 0, 0, 1.0);
}

