#ifndef ASTRAL_DDGI_LAYOUT_GLSL
#define ASTRAL_DDGI_LAYOUT_GLSL
#include <astral/scene.glsl>

// --- Octahedral mapping --------------------------------------------------------------------
vec2 sign_not_zero(vec2 v) { return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }
vec2 oct_encode(vec3 n) {
    n /= max(abs(n.x) + abs(n.y) + abs(n.z), 1e-8);
    return n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign_not_zero(n.xy);
}
vec3 oct_decode(vec2 e) {
    vec3 v = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);
    return normalize(v);
}

// --- DDGI ----------------------------------------------------------------------------------
const int DDGI_IRR = 8;   // interior texels per probe (irradiance)
const int DDGI_DIST = 16; // interior texels per probe (distance moments)

uint ddgi_probe_count(Frame f) { return f.ddgi_counts.x * f.ddgi_counts.y * f.ddgi_counts.z; }
ivec3 ddgi_probe_coord(Frame f, uint index) {
    uint nx = f.ddgi_counts.x, ny = f.ddgi_counts.y;
    return ivec3(index % nx, (index / nx) % ny, index / (nx * ny));
}
uint ddgi_probe_index(Frame f, ivec3 c) {
    return uint(c.x) + uint(c.y) * f.ddgi_counts.x + uint(c.z) * f.ddgi_counts.x * f.ddgi_counts.y;
}
vec3 ddgi_probe_position(Frame f, ivec3 c) { return f.ddgi_origin.xyz + vec3(c) * f.ddgi_spacing.xyz; }
float ddgi_max_dist(Frame f) { return max(1.5 * length(f.ddgi_spacing.xyz), 1e-3); }
// Atlas: probes in rows of nx*ny tiles, nz rows; each tile has a 1-texel border.
ivec2 ddgi_tile_origin(Frame f, uint index, int interior) {
    uint cols = f.ddgi_counts.x * f.ddgi_counts.y;
    return ivec2(int(index % cols), int(index / cols)) * (interior + 2);
}
ivec2 ddgi_atlas_size(Frame f, int interior) {
    return ivec2(int(f.ddgi_counts.x * f.ddgi_counts.y), int(f.ddgi_counts.z)) * (interior + 2);
}
vec2 ddgi_uv(Frame f, uint index, vec3 dir, int interior) {
    vec2 texel = vec2(ddgi_tile_origin(f, index, interior)) + 1.0 + (oct_encode(dir) * 0.5 + 0.5) * float(interior);
    return texel / vec2(ddgi_atlas_size(f, interior));
}

float ddgi_visibility(vec2 moments, float distance_norm) {
    if (distance_norm <= moments.x) return 1.0;
    float variance = max(moments.y - moments.x * moments.x, 0.0);
    float diff = distance_norm - moments.x;
    float cheb = variance / max(variance + diff * diff, 1e-8);
    return cheb * cheb * cheb;
}

// Irradiance (cosine-weighted average radiance, E/PI) at `p` with normal `n`; `v` points to the viewer.
// Distance moments are stored normalized in [0, 1] by ddgi_max_dist(f) so RG16Float never overflows.
vec3 ddgi_irradiance(Frame f, uint irr_tex, uint dist_tex, vec3 p, vec3 n, vec3 v) {
    if (f.ddgi_counts.w == 0u) return vec3(0);
    vec3 spacing = f.ddgi_spacing.xyz;
    float min_spacing = min(spacing.x, min(spacing.y, spacing.z));
    float max_d = ddgi_max_dist(f);
    vec3 biased = p + (n * 0.2 + v * 0.8) * (0.3 * min_spacing);
    ivec3 maxc = ivec3(f.ddgi_counts.xyz) - 1;
    vec3 grid = (biased - f.ddgi_origin.xyz) / spacing;
    ivec3 base = clamp(ivec3(floor(grid)), ivec3(0), max(maxc - 1, ivec3(0)));
    vec3 alpha = clamp(grid - vec3(base), 0.0, 1.0);
    vec3 sum = vec3(0);
    float weight_sum = 0.0;
    for (int i = 0; i < 8; ++i) {
        ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        ivec3 c = clamp(base + offset, ivec3(0), maxc);
        uint index = ddgi_probe_index(f, c);
        vec3 probe = ddgi_probe_position(f, c);
        vec3 to_probe = probe - biased;
        float r = max(length(to_probe), 1e-4);
        vec3 dir = to_probe / r;
        vec3 tri = mix(1.0 - alpha, alpha, vec3(offset));
        float trilinear = tri.x * tri.y * tri.z;
        // Smooth backface test: probes behind the surface barely count.
        vec3 p_to_probe = probe - p;
        float p_len = length(p_to_probe);
        vec3 p_dir = p_len > 1e-5 ? p_to_probe / p_len : n;
        float w = (dot(p_dir, n) + 1.0) * 0.5;
        w = w * w + 0.2;
        // Chebyshev visibility from the probe's normalized distance moments.
        vec2 moments = astral_sample(dist_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, ddgi_uv(f, index, -dir, DDGI_DIST)).rg;
        w *= ddgi_visibility(moments, r / max_d);
        w = max(w, 1e-6);
        if (w < 0.2) w *= w * w / 0.04; // crush tiny weights (light leaks)
        w *= trilinear;
        vec3 irr = astral_sample(irr_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, ddgi_uv(f, index, n, DDGI_IRR)).rgb;
        sum += sqrt(max(irr, vec3(0))) * w; // blend perceptually
        weight_sum += w;
    }
    vec3 e = sum / max(weight_sum, 1e-6);
    return e * e;
}

#endif
