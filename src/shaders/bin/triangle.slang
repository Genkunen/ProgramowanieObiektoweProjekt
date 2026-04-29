struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

static const float3 triangle_positions[3] = {
    float3(0.0, -0.5, 0.0),
    float3(-0.5, 0.5, 0.0),
    float3(0.5, 0.5, 0.0)
};

static const float3 triangle_colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID) {
    VSOutput output;
    output.position = float4(triangle_positions[vertex_id], 1.0);
    output.color = triangle_colors[vertex_id];
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}