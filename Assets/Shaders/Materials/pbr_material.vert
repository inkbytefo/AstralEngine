#version 310 es

// PBR Material Vertex Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Include/vertex_attributes.glsl"
#include "../Include/camera.glsl"

// Vertex shader inputs
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inColor; // Optional vertex colors

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
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    vec3 emissiveFactor;
    float alphaCutoff;
    int alphaMode; // 0: OPAQUE, 1: MASK, 2: BLEND
    int doubleSided;
} material;

// Push constants for dynamic data
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 instanceColor;
    float metallicFactor;
    float roughnessFactor;
    int useVertexColors;
    int hasSkinning;
} pushConstants;

// Outputs to fragment shader
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;
layout(location = 5) out vec4 outColor;
layout(location = 6) out vec3 outViewDir;
layout(location = 7) out flat int outMaterialID;

// Skinning data (if available)
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
    
    // Calculate normal matrix for proper normal transformation
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    
    // Apply skinning if available
#ifdef HAS_SKINNING
    if (pushConstants.hasSkinning > 0) {
        mat4 skinMatrix = mat4(0.0);
        for (int i = 0; i < 4; i++) {
            if (inJointWeights[i] > 0.0) {
                int jointIndex = inJointIndices[i];
                skinMatrix += skinning.jointMatrices[jointIndex] * inJointWeights[i];
            }
        }
        
        worldPos = skinMatrix * vec4(inPosition, 1.0);
        outWorldPos = worldPos.xyz;
        
        // Transform normal with skinning
        mat3 skinNormalMatrix = mat3(transpose(inverse(skinMatrix)));
        outNormal = normalize(skinNormalMatrix * inNormal);
        
        // Transform tangent and bitangent with skinning
        outTangent = normalize(skinNormalMatrix * inTangent);
        outBitangent = normalize(cross(outNormal, outTangent));
    } else {
#endif
        // Transform normal, tangent, and bitangent
        outNormal = normalize(normalMatrix * inNormal);
        outTangent = normalize(normalMatrix * inTangent);
        outBitangent = normalize(cross(outNormal, outTangent));
#ifdef HAS_SKINNING
    }
#endif
    
    // Pass through texture coordinates
    outTexCoord = inTexCoord;
    
    // Calculate view direction
    outViewDir = normalize(ubo.cameraPosition - outWorldPos);
    
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
