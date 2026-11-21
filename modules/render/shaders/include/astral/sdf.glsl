// Signed distance field scene: a list of primitives combined in order with CSG operations.
#ifndef ASTRAL_SDF_GLSL
#define ASTRAL_SDF_GLSL
#include <astral/scene.glsl>

const uint OP_UNION = 0u, OP_SUBTRACT = 1u, OP_INTERSECT = 2u, OP_SMOOTH_UNION = 3u, OP_SMOOTH_SUBTRACT = 4u;
const uint PRIM_SPHERE = 0u, PRIM_BOX = 1u, PRIM_TORUS = 2u, PRIM_CAPSULE = 3u, PRIM_CYLINDER = 4u;
const float SDF_FAR = 1e9;

float sd_box(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - (b - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}
float sd_torus(vec3 p, float ring, float tube) { return length(vec2(length(p.xz) - ring, p.y)) - tube; }
float sd_capsule(vec3 p, float r, float half_length) {
    p.y -= clamp(p.y, -half_length, half_length);
    return length(p) - r;
}
float sd_cylinder(vec3 p, float r, float half_height, float rounding) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r - rounding, half_height - rounding);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - rounding;
}

float sd_primitive(SdfShape s, vec3 world) {
    // Into the shape's local frame: inverse rotation = conjugate quaternion.
    vec4 inv = vec4(-s.rotation.xyz, s.rotation.w);
    vec3 p = quat_rotate(inv, world - s.position_blend.xyz);
    uint prim = uint(s.params.z);
    if (prim == PRIM_SPHERE) return length(p) - s.size.x;
    if (prim == PRIM_BOX) return sd_box(p, s.size.xyz, s.size.w);
    if (prim == PRIM_TORUS) return sd_torus(p, s.size.x, s.size.y);
    if (prim == PRIM_CAPSULE) return sd_capsule(p, s.size.x, s.size.y);
    return sd_cylinder(p, s.size.x, s.size.y, s.size.w);
}

struct SdfHit {
    float dist;
    vec3 albedo;
    float roughness;
    float metallic;
};

// Evaluates every shape in order. Smooth operations also blend the material.
SdfHit sdf_scene(SdfShapes shapes, uint count, vec3 p) {
    SdfHit h = SdfHit(SDF_FAR, vec3(1.0), 0.5, 0.0);
    for (uint i = 0u; i < count; ++i) {
        SdfShape s = shapes.data[i];
        float d = sd_primitive(s, p);
        uint op = uint(s.params.y);
        float k = max(s.position_blend.w, 1e-4);
        vec3 a = s.albedo_roughness.rgb;
        float r = s.albedo_roughness.a, m = s.params.x;
        if (op == OP_UNION || i == 0u && op != OP_SUBTRACT && op != OP_SMOOTH_SUBTRACT) {
            if (d < h.dist) h = SdfHit(d, a, r, m);
        } else if (op == OP_SUBTRACT) {
            h.dist = max(h.dist, -d);
        } else if (op == OP_INTERSECT) {
            h.dist = max(h.dist, d);
        } else if (op == OP_SMOOTH_UNION) {
            float t = clamp(0.5 + 0.5 * (d - h.dist) / k, 0.0, 1.0);
            h.dist = mix(d, h.dist, t) - k * t * (1.0 - t);
            h.albedo = mix(a, h.albedo, t);
            h.roughness = mix(r, h.roughness, t);
            h.metallic = mix(m, h.metallic, t);
        } else { // OP_SMOOTH_SUBTRACT
            float t = clamp(0.5 - 0.5 * (h.dist + d) / k, 0.0, 1.0);
            h.dist = mix(h.dist, -d, t) + k * t * (1.0 - t);
        }
    }
    return h;
}

float sdf_distance(SdfShapes shapes, uint count, vec3 p) { return sdf_scene(shapes, count, p).dist; }

vec3 sdf_normal(SdfShapes shapes, uint count, vec3 p, float eps) {
    const vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * sdf_distance(shapes, count, p + k.xyy * eps) +
                     k.yyx * sdf_distance(shapes, count, p + k.yyx * eps) +
                     k.yxy * sdf_distance(shapes, count, p + k.yxy * eps) +
                     k.xxx * sdf_distance(shapes, count, p + k.xxx * eps));
}

// Soft shadow toward the light (Inigo Quilez's technique). 1 = lit, 0 = fully shadowed.
float sdf_soft_shadow(SdfShapes shapes, uint count, vec3 origin, vec3 to_light, float max_t) {
    float shade = 1.0;
    float t = 0.02;
    for (int i = 0; i < 64 && t < max_t; ++i) {
        float d = sdf_distance(shapes, count, origin + to_light * t);
        if (d < 0.0005) return 0.0;
        shade = min(shade, 12.0 * d / t);
        t += clamp(d, 0.01, 0.5);
    }
    return clamp(shade, 0.0, 1.0);
}

#endif
