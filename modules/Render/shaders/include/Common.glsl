#ifndef ASTRAL_COMMON_GLSL
#define ASTRAL_COMMON_GLSL

// Per-view, per-frame constants. Mirrors Detail::FrameData (std140).
struct FrameData {
    mat4 view;
    mat4 proj;              // jittered
    mat4 viewProj;          // jittered
    mat4 invViewProj;       // jittered
    mat4 viewProjNoJitter;
    mat4 prevViewProjNoJitter;
    mat4 shadowViewProj;
    vec4 cameraPos;         // xyz, w = time (s)
    vec4 viewport;          // width, height, 1/width, 1/height
    vec4 jitter;            // xy = current jitter (NDC), zw = previous
    vec4 sunDirection;      // xyz = direction towards the sun, w = intensity
    vec4 sunColor;          // rgb, w = 1 if a sun exists
    vec4 shadowParams;      // x = shadow map texel (world), y = enabled, z = sdf shadows, w = sdf ao
    vec4 envParams;         // x = ambient intensity, y = exposure, z = sky intensity, w = prefiltered mip count
    vec4 zenithColor;
    vec4 horizonColor;
    vec4 groundColor;
    uvec4 counts;           // x = point lights, y = sdf shapes, z = debug view, w = frame index
    vec4 clip;              // x = near, y = far
};

const float PI = 3.14159265359;
const uint INVALID_INDEX = 0xFFFFFFFFu;

// Debug views (Render::DebugView)
const uint DEBUG_NONE = 0u, DEBUG_ALBEDO = 1u, DEBUG_NORMALS = 2u, DEBUG_ROUGHNESS = 3u, DEBUG_METALLIC = 4u,
           DEBUG_DEPTH = 5u, DEBUG_MOTION = 6u, DEBUG_OBJECT_ID = 7u, DEBUG_SHADOWS = 8u, DEBUG_AO = 9u,
           DEBUG_SDF_BRICKS = 10u, DEBUG_SDF_STEPS = 11u;

// Reversed-Z: depth 1 at the near plane, 0 at the far plane / sky.
vec3 WorldFromDepth(FrameData f, vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = f.invViewProj * ndc;
    return world.xyz / world.w;
}

vec3 ViewRay(FrameData f, vec2 uv) {
    vec3 far = WorldFromDepth(f, uv, 0.0001);
    return normalize(far - f.cameraPos.xyz);
}

float Luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

vec3 HashColor(uint x) {
    x = (x ^ 61u) ^ (x >> 16u); x *= 9u; x ^= x >> 4u; x *= 0x27d4eb2du; x ^= x >> 15u;
    return vec3(x & 255u, (x >> 8u) & 255u, (x >> 16u) & 255u) / 255.0;
}

#endif
