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

#endif
