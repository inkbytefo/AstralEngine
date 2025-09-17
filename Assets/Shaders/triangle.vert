#version 450

layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inColor;    // Modelimizde color verisi YOK
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragColor;
layout(location = 2) out vec2 outTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Model color verisi içermediği için sabit beyaz renk kullan
    fragColor = vec3(1.0, 1.0, 1.0);  // Beyaz renk
    outTexCoord = inTexCoord;
}
