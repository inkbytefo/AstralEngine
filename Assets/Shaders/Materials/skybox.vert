#version 450

// Skybox Vertex Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Include/vertex_attributes.glsl"
#include "../Include/camera.glsl"

// Vertex shader inputs
layout(location = 0) in vec3 inPosition;

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

// Push constants for dynamic data
layout(push_constant) uniform PushConstants {
    mat4 model;
    float intensity;
    int useHDR;
    int rotationEnabled;
} pushConstants;

// Outputs to fragment shader
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outTexCoord;
layout(location = 2) out flat int outMaterialID;

void main() {
    // Remove translation from view matrix for skybox
    mat4 viewNoTranslation = mat4(mat3(ubo.view));
    
    // Apply rotation if enabled
    mat4 modelMatrix = pushConstants.model;
    if (pushConstants.rotationEnabled > 0) {
        float angle = ubo.time * 0.1; // Slow rotation
        mat4 rotation = mat4(
            cos(angle), 0.0, sin(angle), 0.0,
            0.0, 1.0, 0.0, 0.0,
            -sin(angle), 0.0, cos(angle), 0.0,
            0.0, 0.0, 0.0, 1.0
        );
        modelMatrix = rotation * modelMatrix;
    }
    
    // Calculate world position (skybox is always at camera position)
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    
    // Pass texture coordinates (same as position for cube mapping)
    outTexCoord = inPosition;
    
    // Set material ID
    outMaterialID = 0; // Skybox material ID
    
    // Calculate final position
    gl_Position = ubo.projection * viewNoTranslation * worldPos;
    
    // Ensure skybox is always at the far plane
    gl_Position.z = gl_Position.w;
}
