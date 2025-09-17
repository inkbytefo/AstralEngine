#version 450

// Unlit Material Vertex Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Include/vertex_attributes.glsl"
#include "../Include/camera.glsl"

// Vertex shader inputs
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal; // Not used in unlit but available
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent; // Not used in unlit but available
layout(location = 4) in vec4 inColor;   // Vertex colors

// Instance data for instanced rendering
layout(location = 5) in vec4 inInstanceMatrix0;
layout(location = 6) in vec4 inInstanceMatrix1;
layout(location = 7) in vec4 inInstanceMatrix2;
layout(location = 8) in vec4 inInstanceMatrix3;
layout(location = 9) in vec4 inInstanceColor; // Per-instance color

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

layout(binding = 1) uniform MaterialUBO {
    vec4 baseColorFactor;
    float metallicFactor;    // Not used in unlit
    float roughnessFactor;   // Not used in unlit
    float normalScale;       // Not used in unlit
    float occlusionStrength; // Not used in unlit
    vec3 emissiveFactor;
    float alphaCutoff;
    int alphaMode; // 0: OPAQUE, 1: MASK, 2: BLEND
    int doubleSided;
} material;

// Push constants for dynamic data
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 instanceColor;
    float metallicFactor;    // Not used in unlit
    float roughnessFactor;   // Not used in unlit
    int useVertexColors;
    int hasSkinning;         // Not used in unlit
} pushConstants;

// Outputs to fragment shader
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec4 outColor;
layout(location = 3) out flat int outMaterialID;

// Skinning data (if available, but not used in unlit)
#ifdef HAS_SKINNING
layout(binding = 2) uniform SkinningUBO {
    mat4 jointMatrices[256];
} skinning;

layout(location = 10) in ivec4 inJointIndices;
layout(location = 11) in vec4 inJointWeights;
#endif

void main() {
    // Calculate final model matrix
    mat4 modelMatrix = pushConstants.model;
    
    // Apply instancing if available
    mat4 instanceMatrix = mat4(
        inInstanceMatrix0,
        inInstanceMatrix1,
        inInstanceMatrix2,
        inInstanceMatrix3
    );
    modelMatrix = modelMatrix * instanceMatrix;
    
    // Calculate world position
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    
    // Pass through texture coordinates
    outTexCoord = inTexCoord;
    
    // Calculate color
    vec4 vertexColor = vec4(1.0);
    if (pushConstants.useVertexColors > 0) {
        vertexColor = inColor;
    }
    
    // Combine vertex color with instance color and material base color
    vec4 instanceColor = inInstanceColor * pushConstants.instanceColor;
    outColor = vertexColor * instanceColor * material.baseColorFactor;
    
    // Set material ID (can be used for material variation)
    outMaterialID = 0; // Can be set based on material index
    
    // Calculate final position
    gl_Position = ubo.projection * ubo.view * worldPos;
    
    // Apply depth bias for shadow mapping if needed
#ifdef SHADOW_PASS
    const float depthBias = 0.00001;
    gl_Position.z -= depthBias;
#endif
}
