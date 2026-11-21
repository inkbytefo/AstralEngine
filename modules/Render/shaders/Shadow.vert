#version 460
#extension GL_GOOGLE_include_directive : require
#include "include/Common.glsl"
#include "include/Scene.glsl"

layout(set = 0, binding = 0) uniform FrameUBO { FrameData frame; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { Instance instances[]; };

layout(push_constant) uniform Push { uint instanceIndex; } pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = frame.shadowViewProj * instances[pc.instanceIndex].model * vec4(inPosition, 1.0);
}
