#ifndef ASTRAL_LIGHTING_GLSL
#define ASTRAL_LIGHTING_GLSL

#include "Common.glsl"

// ---------------- Cook-Torrance GGX ----------------
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-5);
}

vec3 F_Schlick(vec3 f0, float VoH) { return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0); }

vec3 F_SchlickRoughness(vec3 f0, float NoV, float roughness) {
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - NoV, 5.0);
}

// Radiance reflected towards V from a light arriving along L with irradiance `radiance`.
vec3 ShadeDirect(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0) return vec3(0.0);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    float a = max(roughness * roughness, 0.002);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F_Schlick(f0, VoH);
    vec3 specular = D_GGX(NoH, a) * V_SmithGGXCorrelated(NoV, NoL, a) * F;
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;
    return (diffuse + specular) * radiance * NoL;
}

// ---------------- Procedural sky ----------------
// Gradient sky driving both the background and image-based lighting.
vec3 SkyRadiance(FrameData f, vec3 dir, bool includeSun) {
    float y = dir.y;
    vec3 c = y >= 0.0 ? mix(f.horizonColor.rgb, f.zenithColor.rgb, pow(clamp(y, 0.0, 1.0), 0.45))
                      : mix(f.horizonColor.rgb, f.groundColor.rgb, pow(clamp(-y, 0.0, 1.0), 0.35));
    if (f.sunColor.w > 0.0) {
        float s = max(dot(dir, f.sunDirection.xyz), 0.0);
        c += f.sunColor.rgb * pow(s, 12.0) * 0.15;                 // atmospheric glow
        if (includeSun) c += f.sunColor.rgb * smoothstep(0.9995, 0.9998, s) * 40.0;
    }
    return c * f.envParams.z;
}

// ---------------- Sampling helpers ----------------
vec2 Hammersley(uint i, uint n) {
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return vec2(float(i) / float(n), float(bits) * 2.3283064365386963e-10);
}

vec3 ImportanceSampleGGX(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    return normalize(T * H.x + B * H.y + N * H.z);
}

// Direction for texel (x, y) of a cube face, standard Vulkan face order (+X -X +Y -Y +Z -Z).
vec3 CubeDirection(uint face, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    switch (face) {
        case 0u: return normalize(vec3(1.0, -p.y, -p.x));
        case 1u: return normalize(vec3(-1.0, -p.y, p.x));
        case 2u: return normalize(vec3(p.x, 1.0, p.y));
        case 3u: return normalize(vec3(p.x, -1.0, -p.y));
        case 4u: return normalize(vec3(p.x, -p.y, 1.0));
        default: return normalize(vec3(-p.x, -p.y, -1.0));
    }
}

#endif
