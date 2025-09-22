#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    float pointSize;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = pushConstants.viewProjection * vec4(inPosition, 1.0);
    gl_PointSize = pushConstants.pointSize;
    outColor = inColor;
}