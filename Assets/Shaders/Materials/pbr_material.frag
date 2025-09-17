#version 310 es
#extension GL_GOOGLE_include_directive : enable

// PBR Material Fragment Shader
// Author: inkbytefo
// Project: AstralEngine

#include "../Common/common.glsl"
#include "../Common/lighting.glsl"
#include "../Common/pbr.glsl"
#include "../Common/shadows.glsl"
#include "../Include/material_properties.glsl"

// Inputs from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inColor;
layout(location = 6) in vec3 inViewDir;
layout(location = 7) in flat int inMaterialID;

// Output
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outEmissive; // For bloom pass
layout(location = 2) out vec4 outNormal;   // For deferred rendering
layout(location = 3) out vec4 outPosition; // For deferred rendering
layout(location = 4) out vec4 outMaterial; // For deferred rendering (R: metallic, G: roughness, B: AO, A: unused)

// Texture samplers
layout(binding = 0) uniform sampler2D albedoMap;
layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D metallicMap;
layout(binding = 3) uniform sampler2D roughnessMap;
layout(binding = 4) uniform sampler2D aoMap;
layout(binding = 5) uniform sampler2D emissiveMap;
layout(binding = 6) uniform sampler2D heightMap; // For parallax mapping

// PBR textures
layout(binding = 7) uniform samplerCube irradianceMap;
layout(binding = 8) uniform samplerCube prefilterMap;
layout(binding = 9) uniform sampler2D brdfLUT;

// Shadow maps
layout(binding = 10) uniform sampler2D shadowMaps[MAX_SHADOW_CASCADES];

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

layout(binding = 2) uniform LightUBO {
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    vec3 ambientLight;
    float padding;
} lights;

layout(binding = 3) uniform ShadowUBO {
    ShadowCascade cascades[MAX_SHADOW_CASCADES];
    ShadowSettings settings;
    int useShadows;
    int debugCascades;
} shadowData;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 instanceColor;
    float metallicFactor;
    float roughnessFactor;
    int useVertexColors;
    int hasSkinning;
} pushConstants;

// Texture availability flags
layout(binding = 4) uniform TextureFlags {
    int hasAlbedoMap;
    int hasNormalMap;
    int hasMetallicMap;
    int hasRoughnessMap;
    int hasAOMap;
    int hasEmissiveMap;
    int hasHeightMap;
    int usePackedTextures;
} textureFlags;

// Parallax mapping
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{
    const float heightScale = 0.1;
    float height = texture(heightMap, texCoords).r;
    
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
}

// Steep parallax mapping
vec2 SteepParallaxMapping(vec2 texCoords, vec3 viewDir)
{
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;
    
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(heightMap, currentTexCoords).r;
    
    while (currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(heightMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(heightMap, prevTexCoords).r - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

void main() {
    // Alpha testing
    if (material.alphaMode == 1) { // MASK
        vec4 albedoSample = texture(albedoMap, inTexCoord);
        if (albedoSample.a < material.alphaCutoff) {
            discard;
        }
    }
    
    // Calculate final texture coordinates with parallax mapping
    vec2 texCoords = inTexCoord;
    if (textureFlags.hasHeightMap > 0) {
        texCoords = SteepParallaxMapping(inTexCoord, normalize(inViewDir));
    }
    
    // Sample material properties
    PBRMaterial pbrMaterial;
    
    // Albedo
    if (textureFlags.hasAlbedoMap > 0) {
        vec4 albedoSample = texture(albedoMap, texCoords);
        pbrMaterial.albedo = SRGBToLinear(albedoSample.rgb);
        pbrMaterial.alpha = albedoSample.a;
    } else {
        pbrMaterial.albedo = material.baseColorFactor.rgb;
        pbrMaterial.alpha = material.baseColorFactor.a;
    }
    
    // Apply vertex colors if enabled
    if (pushConstants.useVertexColors > 0) {
        pbrMaterial.albedo *= inColor.rgb;
        pbrMaterial.alpha *= inColor.a;
    }
    
    // Normal mapping
    if (textureFlags.hasNormalMap > 0) {
        vec3 normalSample = texture(normalMap, texCoords).rgb * 2.0 - 1.0;
        normalSample.xy *= material.normalScale;
        
        mat3 TBN = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));
        pbrMaterial.normal = normalize(TBN * normalSample);
    } else {
        pbrMaterial.normal = normalize(inNormal);
    }
    
    // Handle double-sided materials
    if (material.doubleSided > 0 && !gl_FrontFacing) {
        pbrMaterial.normal = -pbrMaterial.normal;
    }
    
    // Metallic and roughness
    if (textureFlags.usePackedTextures > 0) {
        // Packed texture (metallic in R, roughness in G, AO in B)
        vec3 mraSample = texture(metallicMap, texCoords).rgb;
        pbrMaterial.metallic = mraSample.r * material.metallicFactor * pushConstants.metallicFactor;
        pbrMaterial.roughness = mraSample.g * material.roughnessFactor * pushConstants.roughnessFactor;
        pbrMaterial.ao = mraSample.b * material.occlusionStrength;
    } else {
        // Separate textures
        if (textureFlags.hasMetallicMap > 0) {
            pbrMaterial.metallic = texture(metallicMap, texCoords).r * material.metallicFactor * pushConstants.metallicFactor;
        } else {
            pbrMaterial.metallic = material.metallicFactor * pushConstants.metallicFactor;
        }
        
        if (textureFlags.hasRoughnessMap > 0) {
            pbrMaterial.roughness = texture(roughnessMap, texCoords).r * material.roughnessFactor * pushConstants.roughnessFactor;
        } else {
            pbrMaterial.roughness = material.roughnessFactor * pushConstants.roughnessFactor;
        }
        
        if (textureFlags.hasAOMap > 0) {
            pbrMaterial.ao = texture(aoMap, texCoords).r * material.occlusionStrength;
        } else {
            pbrMaterial.ao = material.occlusionStrength;
        }
    }
    
    // Emissive
    if (textureFlags.hasEmissiveMap > 0) {
        pbrMaterial.emissive = SRGBToLinear(texture(emissiveMap, texCoords).rgb) * material.emissiveFactor;
    } else {
        pbrMaterial.emissive = material.emissiveFactor;
    }
    
    // Validate material parameters
    ValidatePBRMaterial(pbrMaterial);
    
    // Create PBR surface
    PBSurface surface = CreatePBSurface(pbrMaterial, inWorldPos, ubo.cameraPosition);
    
    // Calculate shadows
    float shadowFactor = 1.0;
    if (shadowData.useShadows > 0) {
        shadowFactor = CalculateCascadedShadow(shadowMaps, shadowData.cascades, 
                                             inWorldPos, pbrMaterial.normal, 
                                             -lights.directionalLights[0].direction,
                                             shadowData.settings);
    }
    
    // Calculate lighting
    vec3 lighting = CalculatePBRLighting(surface, lights.directionalLights, lights.pointLights, 
                                       lights.spotLights, lights.ambientLight, brdfLUT, 
                                       irradianceMap, prefilterMap);
    
    // Apply shadows to directional lights
    if (shadowData.useShadows > 0 && lights.directionalLights[0].intensity > 0.0) {
        vec3 directionalContribution = CalculateDirectionalLight(lights.directionalLights[0], 
                                                               pbrMaterial.normal, inViewDir,
                                                               pbrMaterial.albedo, pbrMaterial.metallic, 
                                                               pbrMaterial.roughness, CalculateF0(pbrMaterial.albedo, pbrMaterial.metallic));
        lighting -= directionalContribution * (1.0 - shadowFactor);
    }
    
    // Apply alpha blending
    if (material.alphaMode == 2) { // BLEND
        outColor = vec4(lighting, pbrMaterial.alpha);
    } else {
        outColor = vec4(lighting, 1.0);
    }
    
    // Debug cascade visualization
    if (shadowData.debugCascades > 0) {
        int cascadeIndex = 0;
        for (int i = 0; i < MAX_SHADOW_CASCADES - 1; ++i) {
            if (inWorldPos.z < shadowData.cascades[i + 1].splitDepth) {
                cascadeIndex = i + 1;
                break;
            }
        }
        outColor.rgb = mix(outColor.rgb, GetCascadeColor(cascadeIndex), 0.3);
    }
    
    // Output for deferred rendering
    outEmissive = vec4(pbrMaterial.emissive, 1.0);
    outNormal = vec4(pbrMaterial.normal * 0.5 + 0.5, 1.0);
    outPosition = vec4(inWorldPos, 1.0);
    outMaterial = vec4(pbrMaterial.metallic, pbrMaterial.roughness, pbrMaterial.ao, 1.0);
}
