#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "include/Common.glsl"
#include "include/Scene.glsl"

layout(set = 0, binding = 0) uniform FrameUBO { FrameData frame; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { Instance instances[]; };
layout(std430, set = 0, binding = 2) readonly buffer Materials { Material materials[]; };
layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inTangent;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inCurrClip;
layout(location = 4) in vec4 inPrevClip;
layout(location = 5) flat in uint inInstance;

layout(location = 0) out vec4 outAlbedoMetal;
layout(location = 1) out vec4 outNormalRough;
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out vec2 outMotion;
layout(location = 4) out uint outObjectId;
layout(location = 5) out float outDepth;

vec4 SampleOr(uint slot, vec2 uv, vec4 fallback) {
    return slot == INVALID_INDEX ? fallback : texture(textures[nonuniformEXT(slot)], uv);
}

void main() {
    Instance inst = instances[inInstance];
    Material m = materials[inst.materialIndex];

    vec4 base = m.baseColor * SampleOr(m.baseColorTexture, inUv, vec4(1.0));
    vec4 mr = SampleOr(m.metallicRoughnessTexture, inUv, vec4(1.0));
    float metallic = clamp(m.metallic * mr.b, 0.0, 1.0);
    float roughness = clamp(m.roughness * mr.g, 0.03, 1.0);
    vec3 emissive = m.emissiveNormalScale.rgb * SampleOr(m.emissiveTexture, inUv, vec4(1.0)).rgb;

    vec3 N = normalize(inNormal);
    if (!gl_FrontFacing) N = -N;
    if (m.normalTexture != INVALID_INDEX) {
        vec3 T = normalize(inTangent.xyz - N * dot(N, inTangent.xyz));
        vec3 B = cross(N, T) * inTangent.w;
        vec3 tn = texture(textures[nonuniformEXT(m.normalTexture)], inUv).xyz * 2.0 - 1.0;
        tn.xy *= m.emissiveNormalScale.w;
        N = normalize(mat3(T, B, N) * tn);
    }

    vec2 curr = inCurrClip.xy / inCurrClip.w;
    vec2 prev = inPrevClip.xy / inPrevClip.w;

    outAlbedoMetal = vec4(base.rgb, metallic);
    outNormalRough = vec4(N, roughness);
    outEmissive = vec4(emissive, 0.0);
    outMotion = (curr - prev) * 0.5;   // in UV units: current - previous
    outObjectId = inst.objectId;
    outDepth = gl_FragCoord.z;
}
