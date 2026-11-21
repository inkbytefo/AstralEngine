#ifndef ASTRAL_SDF_GLSL
#define ASTRAL_SDF_GLSL

// SDF model shared by the brick baker and the raymarcher.
// Mirrors include/Astral/Render/SdfMath.hpp. The including shader must define
// SHAPE_BINDING and VOLUME_BINDING before including this file.

struct SdfShape {
    vec4 positionBlend;     // xyz position, w blend radius
    vec4 rotation;          // quaternion xyzw
    vec4 size;              // primitive parameters
    vec4 albedoRoughness;
    vec4 emissiveMetallic;
    uint primitive;
    uint operation;
    uint objectId;
    float boundRadius;
};

layout(std430, set = 0, binding = SHAPE_BINDING) readonly buffer SdfShapes { SdfShape shapes[]; };

// Mirrors Detail::SdfVolumeParams (std140).
layout(std140, set = 0, binding = VOLUME_BINDING) uniform SdfVolumeUBO {
    vec4  originVoxel;   // xyz = volume min corner, w = voxel size
    uvec4 gridDim;       // xyz = cells, w = shape count
    uvec4 atlasBricks;   // xyz = atlas size in bricks, w = capacity
    vec4  metrics;       // x = cell size, y = half cell diagonal, z = brick clamp band, w = max empty skip
} vol;

const uint  BRICK_FLAG    = 0x80000000u;
const uint  BRICK_MASK    = 0x7FFFFFFFu;
const int   BRICK_SAMPLES = 8;   // 8^3 samples, 7 voxel intervals per cell
const float SDF_FAR       = 1e10;

const uint OP_UNION = 0u, OP_SUBTRACT = 1u, OP_INTERSECT = 2u, OP_SMOOTH_UNION = 3u, OP_SMOOTH_SUBTRACT = 4u;

float sdSphere(vec3 p, float r) { return length(p) - r; }

float sdBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - (b - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdTorus(vec3 p, float ring, float tube) { return length(vec2(length(p.xz) - ring, p.y)) - tube; }

float sdCapsule(vec3 p, float r, float halfLength) {
    p.y -= clamp(p.y, -halfLength, halfLength);
    return length(p) - r;
}

float sdCylinder(vec3 p, float r, float halfHeight, float rounding) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r - rounding, halfHeight - rounding);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - rounding;
}

vec3 QuatRotate(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

float ShapeDistance(SdfShape s, vec3 p) {
    vec3 lp = QuatRotate(vec4(-s.rotation.xyz, s.rotation.w), p - s.positionBlend.xyz);
    switch (s.primitive) {
        case 0u: return sdSphere(lp, s.size.x);
        case 1u: return sdBox(lp, s.size.xyz, min(s.size.w, min(s.size.x, min(s.size.y, s.size.z))));
        case 2u: return sdTorus(lp, s.size.x, s.size.y);
        case 3u: return sdCapsule(lp, s.size.x, s.size.y);
        case 4u: return sdCylinder(lp, s.size.x, s.size.y, min(s.size.w, min(s.size.x, s.size.y)));
    }
    return sdSphere(lp, s.size.x);
}

float SmoothUnion(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float SmoothSubtract(float a, float b, float k) {
    float h = clamp(0.5 - 0.5 * (a + b) / k, 0.0, 1.0);
    return mix(a, -b, h) + k * h * (1.0 - h);
}

float Combine(float acc, float d, uint op, float blend) {
    float k = max(blend, 0.001);
    switch (op) {
        case OP_UNION:          return min(acc, d);
        case OP_SUBTRACT:       return max(acc, -d);
        case OP_INTERSECT:      return max(acc, d);
        case OP_SMOOTH_UNION:   return SmoothUnion(acc, d, k);
        case OP_SMOOTH_SUBTRACT:return SmoothSubtract(acc, d, k);
    }
    return acc;
}

// True if the shape cannot change the field anywhere within `radius` of p by more than `band`.
bool ShapeCulled(SdfShape s, vec3 p, float radius, float band) {
    if (s.operation == OP_INTERSECT) return false; // intersection clips everything
    float gap = distance(p, s.positionBlend.xyz) - s.boundRadius - max(s.positionBlend.w, 0.0) - radius;
    return gap > band;
}

uint ShapeCount() { return vol.gridDim.w; }

// Field value at p. With band >= 0 shapes that cannot affect [-band, band] are skipped and
// the result is clamped to the band: exact inside it, a conservative bound outside.
float EvaluateShapes(vec3 p, float band) {
    float d = SDF_FAR;
    uint n = ShapeCount();
    for (uint i = 0u; i < n; ++i) {
        SdfShape s = shapes[i];
        if (band >= 0.0 && ShapeCulled(s, p, 0.0, band)) continue;
        d = Combine(d, ShapeDistance(s, p), s.operation, s.positionBlend.w);
    }
    return band >= 0.0 ? clamp(d, -band, band) : d;
}

struct SdfSurface {
    float dist;
    vec3 albedo;
    float roughness;
    float metallic;
    vec3 emissive;
    uint objectId;
};

// Material-aware evaluation used once per pixel at the hit point.
SdfSurface EvaluateSurface(vec3 p, float band) {
    SdfSurface r;
    r.dist = SDF_FAR;
    r.albedo = vec3(0.5);
    r.roughness = 0.5;
    r.metallic = 0.0;
    r.emissive = vec3(0.0);
    r.objectId = 0u;
    uint n = ShapeCount();
    for (uint i = 0u; i < n; ++i) {
        SdfShape s = shapes[i];
        if (ShapeCulled(s, p, 0.0, band)) continue;
        float d = ShapeDistance(s, p);
        float k = max(s.positionBlend.w, 0.001);
        vec3 albedo = s.albedoRoughness.rgb;
        float rough = s.albedoRoughness.a;
        vec3 emissive = s.emissiveMetallic.rgb;
        float metal = s.emissiveMetallic.a;
        switch (s.operation) {
            case OP_UNION:
                if (d < r.dist) { r.albedo = albedo; r.roughness = rough; r.metallic = metal; r.emissive = emissive; r.objectId = s.objectId; }
                r.dist = min(r.dist, d);
                break;
            case OP_SUBTRACT:
                r.dist = max(r.dist, -d);
                break;
            case OP_INTERSECT:
                if (d > r.dist) r.objectId = s.objectId;
                r.dist = max(r.dist, d);
                break;
            case OP_SMOOTH_UNION: {
                float h = clamp(0.5 + 0.5 * (d - r.dist) / k, 0.0, 1.0); // 1 keeps the previous surface
                r.albedo = mix(albedo, r.albedo, h);
                r.roughness = mix(rough, r.roughness, h);
                r.metallic = mix(metal, r.metallic, h);
                r.emissive = mix(emissive, r.emissive, h);
                if (h < 0.5) r.objectId = s.objectId;
                r.dist = SmoothUnion(r.dist, d, k);
                break;
            }
            case OP_SMOOTH_SUBTRACT: {
                float h = clamp(0.5 - 0.5 * (r.dist + d) / k, 0.0, 1.0);
                r.albedo = mix(r.albedo, albedo, h * 0.5); // tint the cut surface
                r.dist = SmoothSubtract(r.dist, d, k);
                break;
            }
        }
    }
    return r;
}

// ---------------- volume addressing ----------------
float CellSize()  { return vol.metrics.x; }
float VoxelSize() { return vol.originVoxel.w; }
vec3 VolumeMin()  { return vol.originVoxel.xyz; }
vec3 VolumeMax()  { return vol.originVoxel.xyz + vec3(vol.gridDim.xyz) * CellSize(); }

uvec3 BrickAtlasCoord(uint id) {
    uint ax = vol.atlasBricks.x, ay = vol.atlasBricks.y;
    return uvec3(id % ax, (id / ax) % ay, id / (ax * ay));
}

uint CellLinear(uvec3 c) { return c.x + c.y * vol.gridDim.x + c.z * vol.gridDim.x * vol.gridDim.y; }

uvec3 CellFromLinear(uint i) {
    uint gx = vol.gridDim.x, gy = vol.gridDim.y;
    return uvec3(i % gx, (i / gx) % gy, i / (gx * gy));
}

vec3 CellMin(uvec3 c) { return VolumeMin() + vec3(c) * CellSize(); }

#endif
