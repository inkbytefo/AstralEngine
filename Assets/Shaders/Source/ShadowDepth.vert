#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 viewPos;
    int lightCount;
    int hasShadows;
    int hasIBL;
} global;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = global.lightSpaceMatrix * pc.model * vec4(inPosition, 1.0);
}
