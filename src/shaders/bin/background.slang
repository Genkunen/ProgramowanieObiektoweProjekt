import "../shared_code_shim";

#define TAU 6.28318530718f
#define MAX_ITER 20

struct SimulationData {
    float4x4 projview;
    float delta_time;
    float simulation_time_since_start;
};

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    float3 background_color;
};
[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

static float3 positions[6] = {
    { -5000.0f,    -5000.0f,    0 },
    { 15000.0f, -5000.0f,    0 },
    { 15000.0f, 10000.0f, 0 },
    { 15000.0f, 10000.0f, 0 },
    { -5000.0f,    10000.0f, 0 },
    { -5000.0f,    -5000.0f,    0 },
};

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID) {
    VSOutput output;
    output.position = float4(positions[vertex_id], 1.0f);
    output.position = mul(push_constants.simulation_data.projview, output.position);
    output.uv = positions[vertex_id].xy / 500.0f * 0.5f + 0.5f;
    return output;
}

float2 glsl_mod(float2 x, float2 y) {
    return x - y * floor(x / y);
}

float avg(float3 v) {
    return (v.x + v.y + v.z) / 3.f;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    float time = push_constants.simulation_data->simulation_time_since_start * 0.2f + 23.0f;
    float2 uv = input.uv;

    float3 bg_color = push_constants.background_color;

    float2 p = glsl_mod(uv * TAU, float2(TAU, TAU)) - 250.0f;

    float2 i = p;
    float c = 1.0f;
    float inten = 0.005f;

    for (int n = 0; n < MAX_ITER; n++) {
        float t = time * (1.0f - (3.5f / float(n + 1)));
        i = p + float2(cos(t - i.x) + sin(t + i.y), sin(t - i.y) + cos(t + i.x));
        c += 1.0f / length(float2(p.x / (sin(i.x + t) / inten), p.y / (cos(i.y + t) / inten)));
    }
    
    c /= float(MAX_ITER);
    
    c = 1.17f - pow(abs(c), 1.4f);
    
    float3 color = float3(pow(abs(c), 2.0f)) * avg(bg_color);
    // color = clamp(float3(0.0f, 0.03f, 0.08f) - color, 0.0f, 1.0f);
    color = clamp(bg_color - color, 0.0f, 1.0f);

    return float4(color, 1.0f);
}
