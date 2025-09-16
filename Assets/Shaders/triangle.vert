#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragColor;

void main() {
    // DEBUG: Direct screen space rendering - bypass all matrices
    // This should definitely show a triangle if the pipeline works
    vec2 screenPos = inPosition * 0.8; // Scale down a bit to ensure visibility
    gl_Position = vec4(screenPos, 0.0, 1.0);
    fragColor = inColor;
}
