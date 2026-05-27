import "../shared_code_shim";

#define TAU 6.28318530718f

struct SimulationData {
    float4x4 projview;
    float delta_time;
    float simulation_time_since_start;
};

struct PushConstants {
    pop::shaders::SimulationData* simulation_data;
    float3 background_color;
    float scale;
    uint max_iterations;
    float caustic_intensity;
    float ray_intensity;
    float surface_y;
    float depth_range;
    float3 deep_color;
    float vignette_size;
};
[vk::push_constant] PushConstants push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD;
};

static float3 positions[6] = {
    { -5000.0f, -5000.0f, 0 },
    { 15000.0f, -5000.0f, 0 },
    { 15000.0f, 10000.0f, 0 },
    { 15000.0f, 10000.0f, 0 },
    { -5000.0f, 10000.0f, 0 },
    { -5000.0f, -5000.0f, 0 },
};

static const float2 POLY_MIN = float2(-5000.0f, -5000.0f);
static const float2 POLY_MAX = float2(15000.0f,  10000.0f);

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VulkanVertexID) {
    VSOutput output;
    output.position = float4(positions[vertex_id], 1.0f);
    output.position = mul(push_constants.simulation_data.projview, output.position);
    output.uv = positions[vertex_id].xy * push_constants.scale / 500.0f * 0.5f + 0.5f;
    return output;
}

float2 glsl_mod(float2 x, float2 y) {
    return x - y * floor(x / y);
}

float compute_caustics(float2 uv, float time) {
    static const int STEPS = push_constants.max_iterations;
    float2 p     = glsl_mod(uv * TAU * 2.5f, float2(TAU, TAU)) - 250.0f;
    float2 i     = p;
    float  c     = 1.0f;
    float  inten = 0.01f;
    float  t0    = time * 1.8f;

    for (int n = 0; n < STEPS; n++) {
        float t = t0 * (1.0f - (3.0f / float(n + 1)));
        i = p + float2(cos(t - i.x) + sin(t + i.y),
                       sin(t - i.y) + cos(t + i.x));
        c += 1.0f / length(float2(p.x / (sin(i.x + t) / inten),
                                  p.y / (cos(i.y + t) / inten)));
    }
    c /= float(STEPS);
    return pow(saturate(c * 0.45f), 4.0f);
}

float compute_god_rays(float2 world_pos, float time) {
    float depth_below = push_constants.surface_y - world_pos.y;
    if (depth_below <= 0.0f) return 0.0f;

    float spread = depth_below * 0.08f;
    float sample_x = world_pos.x + spread * sin(world_pos.x * 0.002f + time * 0.2f);

    float rays = 0.0f;
    float amplitude = 0.5f;
    float freq = 0.0015f;

    for (int i = 0; i < 4; i++) {
        float wave = sin(sample_x * freq + time * (0.8f + float(i) * 0.25f));
        rays += wave * amplitude;
        freq *= 2.1f;
        amplitude *= 0.5f;
    }

    rays = pow(saturate(rays * 0.5f + 0.5f), 4.0f);

    float flicker = 0.75f + 0.25f * sin(time * 2.5f + world_pos.x * 0.01f);
    float depth_att = exp(-depth_below / max(push_constants.depth_range, 1.0f));
    float surface_fade = smoothstep(0.0f, 100.0f, depth_below);

    return rays * flicker * depth_att * surface_fade;
}

float compute_edge_vignette(float2 world_pos) {
    float2 dist_min  = world_pos - POLY_MIN;
    float2 dist_max  = POLY_MAX  - world_pos;
    float2 edge_dist = min(dist_min, dist_max);
    float  nearest   = min(edge_dist.x, edge_dist.y);
    return smoothstep(0.0f, push_constants.vignette_size, nearest);
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    float time = push_constants.simulation_data->simulation_time_since_start * 0.3f + 23.0f;
    float2 uv = input.uv;

    float2 world_pos = (uv - 0.5f) * 1000.0f / max(push_constants.scale, 0.0001f);
    float depth_below = push_constants.surface_y - world_pos.y;
    float depth_t = saturate(depth_below / max(push_constants.depth_range, 1.0f));

    float3 bg_color = push_constants.background_color;

    float ambient_noise = sin(uv.x * 3.0f + time * 0.2f) * cos(uv.y * 3.0f - time * 0.15f);
    float3 base_color = bg_color * (0.9f + 0.1f * ambient_noise);

    float caust_val = 0.0f;
    if (depth_below > 0.0f) {
        caust_val = compute_caustics(uv, time);
    }
    
    float caust_fade = exp(-depth_t * 3.5f);
    float3 caust_tint = bg_color * 1.8f + float3(0.1f, 0.15f, 0.05f);
    base_color += caust_val * caust_fade * push_constants.caustic_intensity * caust_tint;

    float ray_val = compute_god_rays(world_pos, time);
    float3 ray_tint = lerp(bg_color, float3(0.85f, 0.95f, 1.0f), 0.4f);
    base_color += ray_val * push_constants.ray_intensity * ray_tint;

    float depth_curve = depth_t * depth_t;
    base_color = lerp(base_color, push_constants.deep_color, depth_curve);

    base_color *= compute_edge_vignette(world_pos);

    return float4(clamp(base_color, 0.0f, 1.0f), 1.0f);
}
