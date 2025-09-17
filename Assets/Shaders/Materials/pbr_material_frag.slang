// PBR Material Fragment Shader
// Author: inkbytefo
// Project: AstralEngine

#include "../Common/common.slang"
#include "../Common/pbr_structs.slang"
#include "../Common/lighting.slang"
#include "../Common/pbr.slang"
#include "../Common/shadows.slang"
#include "../Include/material_properties.glsl"

// Inputs from vertex shader
struct VertexOutput {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
    [[vk::location(1)]] float3 normal : TEXCOORD1;
    [[vk::location(2)]] float2 texCoord : TEXCOORD2;
    [[vk::location(3)]] float3 tangent : TEXCOORD3;
    [[vk::location(4)]] float3 bitangent : TEXCOORD4;
    [[vk::location(5)]] float4 color : TEXCOORD5;
    [[vk::location(6)]] float3 viewDir : TEXCOORD6;
    [[vk::location(7)]] nointerpolation int materialID : TEXCOORD7;
    float4 position : SV_POSITION;
};

// Output
struct FragmentOutput {
    [[vk::location(0)]] float4 color : SV_TARGET0;
    [[vk::location(1)]] float4 emissive : SV_TARGET1; // For bloom pass
    [[vk::location(2)]] float4 normal : SV_TARGET2;   // For deferred rendering
    [[vk::location(3)]] float4 position : SV_TARGET3; // For deferred rendering
    [[vk::location(4)]] float4 material : SV_TARGET4; // For deferred rendering (R: metallic, G: roughness, B: AO, A: unused)
};

// Texture samplers
Texture2D albedoMap : register(t14);
SamplerState albedoSampler : register(s14);

Texture2D normalMap : register(t15);
SamplerState normalSampler : register(s15);

Texture2D metallicMap : register(t16);
SamplerState metallicSampler : register(s16);

Texture2D roughnessMap : register(t17);
SamplerState roughnessSampler : register(s17);

Texture2D aoMap : register(t18);
SamplerState aoSampler : register(s18);

Texture2D emissiveMap : register(t19);
SamplerState emissiveSampler : register(s19);

Texture2D heightMap : register(t20);
SamplerState heightSampler : register(s20);

// PBR textures
TextureCube irradianceMap : register(t21);
SamplerState irradianceSampler : register(s21);

TextureCube prefilterMap : register(t22);
SamplerState prefilterSampler : register(s22);

Texture2D brdfLUT : register(t23);
SamplerState brdfLUTSampler : register(s23);

// Shadow maps - separate bindings to avoid dynamic indexing
Texture2D shadowMap0 : register(t24);
SamplerState shadowSampler0 : register(s24);

Texture2D shadowMap1 : register(t25);
SamplerState shadowSampler1 : register(s25);

Texture2D shadowMap2 : register(t26);
SamplerState shadowSampler2 : register(s26);

Texture2D shadowMap3 : register(t27);
SamplerState shadowSampler3 : register(s27);

// Uniform buffer objects
cbuffer UniformBufferObject : register(b0) {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

cbuffer MaterialUBO : register(b1) {
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float3 emissiveFactor;
    float alphaCutoff;
    int alphaMode; // 0: OPAQUE, 1: MASK, 2: BLEND
    int doubleSided;
};

cbuffer LightUBO : register(b2) {
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    float3 ambientLight;
    float padding;
};

cbuffer ShadowUBO : register(b3) {
    ShadowCascade cascades[MAX_SHADOW_CASCADES];
    ShadowSettings settings;
    int useShadows;
    int debugCascades;
};

// Push constants
[[vk::push_constant]] cbuffer PushConstants {
    float4x4 model;
    float4 instanceColor;
    float metallicFactor;
    float roughnessFactor;
    int useVertexColors;
    int hasSkinning;
};

// Texture availability flags
cbuffer TextureFlags : register(b4) {
    int hasAlbedoMap;
    int hasNormalMap;
    int hasMetallicMap;
    int hasRoughnessMap;
    int hasAOMap;
    int hasEmissiveMap;
    int hasHeightMap;
    int usePackedTextures;
};

// Parallax mapping
float2 ParallaxMapping(float2 texCoords, float3 viewDir)
{
    const float heightScale = 0.1;
    float height = heightMap.Sample(heightSampler, texCoords).r;
    
    float2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
}

// Steep parallax mapping
float2 SteepParallaxMapping(float2 texCoords, float3 viewDir)
{
    const float heightScale = 0.1;
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    float2 P = viewDir.xy / viewDir.z * heightScale;
    float2 deltaTexCoords = P / numLayers;
    
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = heightMap.Sample(heightSampler, currentTexCoords).r;
    
    while (currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = heightMap.Sample(heightSampler, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }
    
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = heightMap.Sample(heightSampler, prevTexCoords).r - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

FragmentOutput main(VertexOutput input) {
    FragmentOutput output;
    
    // Alpha testing
    if (alphaMode == 1) { // MASK
        float4 albedoSample = albedoMap.Sample(albedoSampler, input.texCoord);
        if (albedoSample.a < alphaCutoff) {
            discard;
        }
    }
    
    // Calculate final texture coordinates with parallax mapping
    float2 texCoords = input.texCoord;
    if (hasHeightMap > 0) {
        texCoords = SteepParallaxMapping(input.texCoord, normalize(input.viewDir));
    }
    
    // Sample material properties
    PBRMaterial pbrMaterial;
    
    // Albedo
    if (hasAlbedoMap > 0) {
        float4 albedoSample = albedoMap.Sample(albedoSampler, texCoords);
        pbrMaterial.albedo = SRGBToLinear(albedoSample.rgb);
        pbrMaterial.alpha = albedoSample.a;
    } else {
        pbrMaterial.albedo = baseColorFactor.rgb;
        pbrMaterial.alpha = baseColorFactor.a;
    }
    
    // Apply vertex colors if enabled
    if (useVertexColors > 0) {
        pbrMaterial.albedo *= input.color.rgb;
        pbrMaterial.alpha *= input.color.a;
    }
    
    // Normal mapping
    if (hasNormalMap > 0) {
        float3 normalSample = normalMap.Sample(normalSampler, texCoords).rgb * 2.0 - 1.0;
        normalSample.xy *= normalScale;
        
        // Create TBN matrix using column-wise assignment to avoid MAT2 syntax error
        float3 t = normalize(input.tangent);
        float3 b = normalize(input.bitangent);
        float3 n = normalize(input.normal);
        float3x3 TBN = float3x3(t, b, n);
        pbrMaterial.normal = normalize(mul(normalSample, TBN));
    } else {
        pbrMaterial.normal = normalize(input.normal);
    }
    
    // Handle double-sided materials
    if (doubleSided > 0 && !IsFrontFace()) {
        pbrMaterial.normal = -pbrMaterial.normal;
    }
    
    // Metallic and roughness
    if (usePackedTextures > 0) {
        // Packed texture (metallic in R, roughness in G, AO in B)
        float3 mraSample = metallicMap.Sample(metallicSampler, texCoords).rgb;
        pbrMaterial.metallic = mraSample.r * metallicFactor * metallicFactor;
        pbrMaterial.roughness = mraSample.g * roughnessFactor * roughnessFactor;
        pbrMaterial.ao = mraSample.b * occlusionStrength;
    } else {
        // Separate textures
        if (hasMetallicMap > 0) {
            pbrMaterial.metallic = metallicMap.Sample(metallicSampler, texCoords).r * metallicFactor * metallicFactor;
        } else {
            pbrMaterial.metallic = metallicFactor * metallicFactor;
        }
        
        if (hasRoughnessMap > 0) {
            pbrMaterial.roughness = roughnessMap.Sample(roughnessSampler, texCoords).r * roughnessFactor * roughnessFactor;
        } else {
            pbrMaterial.roughness = roughnessFactor * roughnessFactor;
        }
        
        if (hasAOMap > 0) {
            pbrMaterial.ao = aoMap.Sample(aoSampler, texCoords).r * occlusionStrength;
        } else {
            pbrMaterial.ao = occlusionStrength;
        }
    }
    
    // Emissive
    if (hasEmissiveMap > 0) {
        pbrMaterial.emissive = SRGBToLinear(emissiveMap.Sample(emissiveSampler, texCoords).rgb) * emissiveFactor;
    } else {
        pbrMaterial.emissive = emissiveFactor;
    }
    
    // Validate material parameters
    ValidatePBRMaterial(pbrMaterial);
    
    // Create PBR surface
    PBSurface surface = CreatePBSurface(pbrMaterial, input.worldPos, cameraPosition);
    
    // Calculate shadows
    float shadowFactor = 1.0;
    if (useShadows > 0) {
        shadowFactor = CalculateCascadedShadow(shadowMap0, shadowMap1, shadowMap2, shadowMap3,
                                             cascades, 
                                             input.worldPos, pbrMaterial.normal, 
                                             -directionalLights[0].direction,
                                             settings);
    }
    
    // Calculate lighting
    float3 lighting = CalculatePBRLighting(surface, directionalLights, pointLights, 
                                       spotLights, ambientLight, brdfLUT, 
                                       irradianceMap, prefilterMap);
    
    // Apply shadows to directional lights
    if (useShadows > 0 && directionalLights[0].intensity > 0.0) {
        float3 directionalContribution = CalculateDirectionalLight(directionalLights[0], 
                                                               pbrMaterial.normal, input.viewDir,
                                                               pbrMaterial.albedo, pbrMaterial.metallic, 
                                                               pbrMaterial.roughness, CalculateF0(pbrMaterial.albedo, pbrMaterial.metallic));
        lighting -= directionalContribution * (1.0 - shadowFactor);
    }
    
    // Apply alpha blending
    if (alphaMode == 2) { // BLEND
        output.color = float4(lighting, pbrMaterial.alpha);
    } else {
        output.color = float4(lighting, 1.0);
    }
    
    // Debug cascade visualization
    if (debugCascades > 0) {
        int cascadeIndex = 0;
        for (int i = 0; i < MAX_SHADOW_CASCADES - 1; ++i) {
            if (input.worldPos.z < cascades[i + 1].splitDepth) {
                cascadeIndex = i + 1;
                break;
            }
        }
        output.color.rgb = lerp(output.color.rgb, GetCascadeColor(cascadeIndex), 0.3);
    }
    
    // Output for deferred rendering
    output.emissive = float4(pbrMaterial.emissive, 1.0);
    output.normal = float4(pbrMaterial.normal * 0.5 + 0.5, 1.0);
    output.position = float4(input.worldPos, 1.0);
    output.material = float4(pbrMaterial.metallic, pbrMaterial.roughness, pbrMaterial.ao, 1.0);
    
    return output;
}
