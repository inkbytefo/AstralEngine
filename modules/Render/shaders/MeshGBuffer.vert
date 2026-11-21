#version 460
#extension GL_GOOGLE_include_directive : require
#include "include/Common.glsl"
#include "include/Scene.glsl"

layout(set = 0, binding = 0) uniform FrameUBO { FrameData frame; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { Instance instances[]; };

layout(push_constant) uniform Push { uint instanceIndex; } pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUv;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outTangent;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec4 outCurrClip;
layout(location = 4) out vec4 outPrevClip;
layout(location = 5) flat out uint outInstance;

void main() {
    Instance inst = instances[pc.instanceIndex];
    vec4 world = inst.model * vec4(inPosition, 1.0);
    vec4 prevWorld = inst.prevModel * vec4(inPosition, 1.0);

    gl_Position = frame.viewProj * world;
    outCurrClip = frame.viewProjNoJitter * world;
    outPrevClip = frame.prevViewProjNoJitter * prevWorld;

    mat3 normalMatrix = mat3(inst.normalMatrix);
    outNormal = normalMatrix * inNormal;
    outTangent = vec4(mat3(inst.model) * inTangent.xyz, inTangent.w);
    outUv = inUv;
    outInstance = pc.instanceIndex;
}
