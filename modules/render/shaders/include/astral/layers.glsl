// Material layers and mask blending for surface() (ADR-0055 D7). Pure functions: no built-ins are read, so the
// same code runs in the raster G-buffer and in the ray-traced hit shaders.
//
//   AstralLayer rock = astral_layer_default(); rock.albedo = ...;
//   AstralLayer moss = astral_layer_default(); moss.albedo = ...; moss.height = ...;
//   float m = astral_mask_slope(NORMAL, 0.6, 0.9) * texture(mask_tex, UV).r;
//   AstralLayer l = astral_layer_blend_height(rock, moss, m, 0.2);
//   ALBEDO = l.albedo; ROUGHNESS = l.roughness; METALLIC = l.metallic; EMISSION = l.emission; AO = l.ao;
//   NORMAL_MAP = l.normal_map;
#ifndef ASTRAL_LAYERS_GLSL
#define ASTRAL_LAYERS_GLSL

struct AstralLayer {
    vec3 albedo;
    float roughness;
    float metallic;
    vec3 emission;
    float ao;
    vec3 normal_map; // encoded like NORMAL_MAP: (0.5, 0.5, 1.0) is flat
    float height;    // [0, 1]; only height blending reads it
};

AstralLayer astral_layer_default() {
    AstralLayer l;
    l.albedo = vec3(1.0);
    l.roughness = 0.6;
    l.metallic = 0.0;
    l.emission = vec3(0.0);
    l.ao = 1.0;
    l.normal_map = vec3(0.5, 0.5, 1.0);
    l.height = 0.5;
    return l;
}

/// Linear blend of every field: t = 0 gives `a`, t = 1 gives `b`. The encoded normals are mixed like colours,
/// which is the usual cheap blend and is exact at both ends.
AstralLayer astral_layer_mix(AstralLayer a, AstralLayer b, float t) {
    t = clamp(t, 0.0, 1.0);
    AstralLayer l;
    l.albedo = mix(a.albedo, b.albedo, t);
    l.roughness = mix(a.roughness, b.roughness, t);
    l.metallic = mix(a.metallic, b.metallic, t);
    l.emission = mix(a.emission, b.emission, t);
    l.ao = mix(a.ao, b.ao, t);
    l.normal_map = mix(a.normal_map, b.normal_map, t);
    l.height = mix(a.height, b.height, t);
    return l;
}

/// Sharpens a mask around 0.5: contrast 0 leaves it, towards 1 it becomes a hard edge.
float astral_mask_contrast(float m, float contrast) {
    contrast = clamp(contrast, 0.0, 0.999);
    float k = 1.0 / (1.0 - contrast);
    return clamp((m - 0.5) * k + 0.5, 0.0, 1.0);
}

/// Height blending: where `b` is taller than `a` it wins earlier, so moss collects in crevices and sand fills the
/// gaps between stones. `mask` is the artist's coverage of `b` (0..1); `depth` is the transition width in height
/// units (about 0.05 to 0.3). mask 0 returns `a`, mask 1 returns `b`.
float astral_height_weight(float height_a, float height_b, float mask, float depth) {
    mask = clamp(mask, 0.0, 1.0);
    depth = max(depth, 1e-4);
    float top = max(height_a * (1.0 - mask), height_b * mask) - depth;
    float wa = max(height_a * (1.0 - mask) - top, 0.0);
    float wb = max(height_b * mask - top, 0.0);
    float sum = wa + wb;
    return sum > 1e-6 ? wb / sum : mask;
}

AstralLayer astral_layer_blend_height(AstralLayer a, AstralLayer b, float mask, float depth) {
    return astral_layer_mix(a, b, astral_height_weight(a.height, b.height, mask, depth));
}

/// Coverage that grows with how much the surface faces up: 0 on walls, 1 on flat ground. `world_normal` is the
/// built-in NORMAL; `from_up` and `to_up` are n.y values (0.6..0.9 is roughly 53..26 degrees from vertical ground).
float astral_mask_slope(vec3 world_normal, float from_up, float to_up) {
    return smoothstep(from_up, to_up, normalize(world_normal).y);
}

/// Coverage by world height in metres: 0 below `low`, 1 above `high`.
float astral_mask_height(vec3 world_position, float low, float high) {
    return smoothstep(low, high, world_position.y);
}

float astral_hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

/// Smooth value noise in [0, 1], one feature per unit of `p` (scale the position yourself).
float astral_mask_noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = astral_hash13(i);
    float n100 = astral_hash13(i + vec3(1, 0, 0));
    float n010 = astral_hash13(i + vec3(0, 1, 0));
    float n110 = astral_hash13(i + vec3(1, 1, 0));
    float n001 = astral_hash13(i + vec3(0, 0, 1));
    float n101 = astral_hash13(i + vec3(1, 0, 1));
    float n011 = astral_hash13(i + vec3(0, 1, 1));
    float n111 = astral_hash13(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

#endif
