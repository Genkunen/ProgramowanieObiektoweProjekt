import "../../shared_code_shim";

#define TAU 6.28318530718f

struct SimulationData {
    float4x4 projview;
    float delta_time;
    float simulation_time_since_start;
};

[vk::push_constant] pop::shaders::BackgroundVSFSPushConstants push_constants;

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

float hash12(float2 p) {
    float3 p3  = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float compute_caustics(float2 uv, float time) {
    static const int STEPS = push_constants.max_iterations;
    float2 p = glsl_mod(uv * TAU * 2.5f, float2(TAU, TAU)) - 250.0f;
    float2 i = p;
    float c = 1.0f;
    float inten = 0.01f;
    float t0 = time * 0.5f;

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

float3 compute_chromatic_caustics(float2 uv, float time) {
    float offset = 0.002f;
    float r = compute_caustics(uv + float2(offset, 0.0f), time);
    float g = compute_caustics(uv, time);
    float b = compute_caustics(uv - float2(offset, 0.0f), time);
    return float3(r, g, b);
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

float compute_particulates(float2 world_pos, float time, float layer_depth) {
    float2 p = world_pos * 0.05f * layer_depth; 
    p.y += time * (-1.5f - layer_depth * 0.5f); 
    p.x += sin(time * 0.5f + p.y * 0.1f) * (2.0f + layer_depth * 0.5f);
    
    float2 id = floor(p);
    float2 local_uv = frac(p) - 0.5f;
    
    float n = hash12(id + layer_depth * 10.f);
    
    float2 offset = float2(cos(n * TAU), sin(n * TAU)) * 0.3f;
    float dist = length(local_uv - offset);
    
    float base_size = 0.02f + (layer_depth * 0.015f);
    float size = base_size * (1 + n);
    float particle = smoothstep(size, size * 0.2f, dist);
    
    float flicker = sin(time * 3.0f + n * 10.0f) * 0.5f + 0.5f;
    float layer_fade = saturate(layer_depth * 0.6f);
    
    return particle * flicker * (n * 0.5f + 0.5f);
}

float compute_edge_vignette(float2 world_pos) {
    float left_dist   = world_pos.x - POLY_MIN.x;
    float right_dist  = POLY_MAX.x - world_pos.x;
    float bottom_dist = world_pos.y - POLY_MIN.y;
    
    float nearest = min(min(left_dist, right_dist), bottom_dist);
    
    return smoothstep(0.0f, push_constants.vignette_size, nearest);
}

float interleaved_gradient_noise(float2 frag_coord) {
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(frag_coord, magic.xy)));
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    float time = push_constants.simulation_data->simulation_time_since_start * 0.2f + 23.0f;
    float2 uv = input.uv;

    float uv_distortion = 0.02f;
    float2 flow_uv = uv + float2(sin(uv.y * 15.0f + time * 0.4f) * uv_distortion, cos(uv.x * 15.0f - time * 0.3f) * uv_distortion);

    float2 world_pos = (uv - 0.5f) * 1000.0f / max(push_constants.scale, 0.0001f);
    float depth_below = push_constants.surface_y - world_pos.y;
    float depth_t = saturate(depth_below / max(push_constants.depth_range, 1.0f));

    float3 bg_color = push_constants.base_color;

    float ambient_noise = sin(flow_uv.x * 3.0f + time * 0.2f) * cos(flow_uv.y * 3.0f - time * 0.15f);
    float3 base_color = bg_color * (0.9f + 0.1f * ambient_noise);

    float3 caust_val = float3(0.0f, 0.0f, 0.0f);
    if (depth_below > 0.0f) {
        caust_val = compute_chromatic_caustics(flow_uv, time);
    }
    
    float caust_fade = exp(-depth_t * 3.5f);
    float3 caust_tint = bg_color * 20.8f + float3(0.1f, 0.15f, 0.05f);
    base_color += caust_val * caust_fade * push_constants.caustic_intensity * caust_tint;

    float ray_val = compute_god_rays(world_pos, time);
    float3 ray_tint = lerp(bg_color, float3(0.85f, 0.95f, 1.0f), 0.4f);
    base_color += ray_val * push_constants.ray_intensity * ray_tint;

    if (depth_below > 0.0f) {
        float part_val = 0;
        part_val += compute_particulates(world_pos, time, 0.5f);
        part_val += compute_particulates(world_pos, time, 2.f);
        part_val += compute_particulates(world_pos, time, 4.f);
        float part_fade = smoothstep(0.0f, 150.0f, depth_below) * exp(-depth_t * 2.0f);
        base_color += part_val * part_fade * float3(0.6f, 0.8f, 0.9f) * 0.03;
    }

    float depth_curve = depth_t * depth_t;
    base_color = lerp(base_color, push_constants.deep_color, depth_curve);
    base_color *= compute_edge_vignette(world_pos);

    float noise = interleaved_gradient_noise(input.position.xy);
    float dither = (noise - 0.5f) / 255.0f;
    base_color += dither;

    return float4(clamp(base_color, 0.0f, 1.0f), 1.0f);
}
