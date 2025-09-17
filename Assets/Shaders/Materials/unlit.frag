#version 450

// Unlit Material Fragment Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Common/common.glsl"
#include "../Include/material_properties.glsl"

// Inputs from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;
layout(location = 3) in flat int inMaterialID;

// Output
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outEmissive; // For bloom pass
layout(location = 2) out vec4 outNormal;   // For deferred rendering
layout(location = 3) out vec4 outPosition; // For deferred rendering
layout(location = 4) out vec4 outMaterial; // For deferred rendering

// Texture samplers
layout(binding = 0) uniform sampler2D albedoMap;
layout(binding = 1) uniform sampler2D emissiveMap;

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

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 instanceColor;
    float metallicFactor;    // Not used in unlit
    float roughnessFactor;   // Not used in unlit
    int useVertexColors;
    int hasSkinning;         // Not used in unlit
} pushConstants;

// Texture availability flags
layout(binding = 4) uniform TextureFlags {
    int hasAlbedoMap;
    int hasNormalMap;        // Not used in unlit
    int hasMetallicMap;      // Not used in unlit
    int hasRoughnessMap;     // Not used in unlit
    int hasAOMap;            // Not used in unlit
    int hasEmissiveMap;
    int hasHeightMap;        // Not used in unlit
    int usePackedTextures;   // Not used in unlit
} textureFlags;

// Simple UV animation
vec2 AnimateUV(vec2 uv, float time, vec2 speed, vec2 tiling)
{
    return (uv * tiling) + (time * speed);
}

// Dissolve effect
float Dissolve(vec2 uv, float time, float edgeWidth, float noiseScale)
{
    float noise = SimplexNoise2D(uv * noiseScale + time * 0.1);
    float threshold = sin(time * 2.0) * 0.5 + 0.5;
    float edge = smoothstep(threshold - edgeWidth, threshold + edgeWidth, noise);
    return edge;
}

// Flicker effect
float Flicker(float time, float frequency, float intensity)
{
    float flicker = sin(time * frequency) * 0.5 + 0.5;
    return 1.0 + flicker * intensity;
}

void main() {
    // Alpha testing
    if (material.alphaMode == 1) { // MASK
        vec4 albedoSample = texture(albedoMap, inTexCoord);
        if (albedoSample.a < material.alphaCutoff) {
            discard;
        }
    }
    
    // Sample albedo
    vec4 albedo = vec4(1.0);
    if (textureFlags.hasAlbedoMap > 0) {
        vec4 albedoSample = texture(albedoMap, inTexCoord);
        albedo.rgb = SRGBToLinear(albedoSample.rgb);
        albedo.a = albedoSample.a;
    } else {
        albedo = material.baseColorFactor;
    }
    
    // Apply vertex colors if enabled
    if (pushConstants.useVertexColors > 0) {
        albedo *= inColor;
    }
    
    // Sample emissive
    vec3 emissive = material.emissiveFactor;
    if (textureFlags.hasEmissiveMap > 0) {
        emissive *= SRGBToLinear(texture(emissiveMap, inTexCoord).rgb);
    }
    
    // Apply emissive flicker effect
    emissive *= Flicker(ubo.time, 10.0, 0.1);
    
    // Calculate final color (unlit - no lighting calculations)
    vec3 finalColor = albedo.rgb + emissive;
    
    // Apply dissolve effect (optional, can be controlled by material parameters)
    float dissolveEdge = Dissolve(inTexCoord, ubo.time, 0.05, 5.0);
    finalColor *= dissolveEdge;
    
    // Apply alpha blending
    if (material.alphaMode == 2) { // BLEND
        outColor = vec4(finalColor, albedo.a);
    } else {
        outColor = vec4(finalColor, 1.0);
    }
    
    // Output for deferred rendering
    outEmissive = vec4(emissive, 1.0);
    outNormal = vec4(0.5, 0.5, 1.0, 1.0); // Default normal for unlit
    outPosition = vec4(inWorldPos, 1.0);
    outMaterial = vec4(0.0, 1.0, 1.0, 1.0); // Default material properties for unlit
}
