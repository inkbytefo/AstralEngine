// Material Properties and Utilities
// Author: inkbytefo
// Project: AstralEngine

#ifndef MATERIAL_PROPERTIES_GLSL
#define MATERIAL_PROPERTIES_GLSL

#include "common.glsl"

// Material types
const int MATERIAL_TYPE_PBR = 0;
const int MATERIAL_TYPE_UNLIT = 1;
const int MATERIAL_TYPE_SKYBOX = 2;
const int MATERIAL_TYPE_CUSTOM = 3;

// Alpha modes
const int ALPHA_MODE_OPAQUE = 0;
const int ALPHA_MODE_MASK = 1;
const int ALPHA_MODE_BLEND = 2;

// Material structure
struct MaterialProperties {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    vec3 emissiveFactor;
    float alphaCutoff;
    int alphaMode;
    int doubleSided;
    int materialType;
    int padding;
};

// Texture flags structure
struct TextureFlags {
    int hasAlbedoMap;
    int hasNormalMap;
    int hasMetallicMap;
    int hasRoughnessMap;
    int hasAOMap;
    int hasEmissiveMap;
    int hasHeightMap;
    int usePackedTextures;
};

// PBR material parameters
struct PBRParameters {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 normal;
    vec3 emissive;
    float alpha;
};

// Material utility functions
MaterialProperties CreateDefaultMaterial() {
    MaterialProperties material;
    material.baseColorFactor = vec4(1.0, 1.0, 1.0, 1.0);
    material.metallicFactor = 0.0;
    material.roughnessFactor = 0.5;
    material.normalScale = 1.0;
    material.occlusionStrength = 1.0;
    material.emissiveFactor = vec3(0.0);
    material.alphaCutoff = 0.5;
    material.alphaMode = ALPHA_MODE_OPAQUE;
    material.doubleSided = 0;
    material.materialType = MATERIAL_TYPE_PBR;
    return material;
}

TextureFlags CreateDefaultTextureFlags() {
    TextureFlags flags;
    flags.hasAlbedoMap = 0;
    flags.hasNormalMap = 0;
    flags.hasMetallicMap = 0;
    flags.hasRoughnessMap = 0;
    flags.hasAOMap = 0;
    flags.hasEmissiveMap = 0;
    flags.hasHeightMap = 0;
    flags.usePackedTextures = 0;
    return flags;
}

PBRParameters CreateDefaultPBRParameters() {
    PBRParameters pbr;
    pbr.albedo = vec3(1.0);
    pbr.metallic = 0.0;
    pbr.roughness = 0.5;
    pbr.ao = 1.0;
    pbr.normal = vec3(0.0, 1.0, 0.0);
    pbr.emissive = vec3(0.0);
    pbr.alpha = 1.0;
    return pbr;
}

// Material validation and clamping
void ValidateMaterialProperties(inout MaterialProperties material) {
    // Clamp factors to valid ranges
    material.metallicFactor = clamp(material.metallicFactor, 0.0, 1.0);
    material.roughnessFactor = clamp(material.roughnessFactor, 0.04, 1.0); // Prevent division by zero
    material.normalScale = max(material.normalScale, 0.0);
    material.occlusionStrength = clamp(material.occlusionStrength, 0.0, 1.0);
    material.alphaCutoff = clamp(material.alphaCutoff, 0.0, 1.0);
    
    // Validate alpha mode
    material.alphaMode = clamp(material.alphaMode, ALPHA_MODE_OPAQUE, ALPHA_MODE_BLEND);
    
    // Ensure base color is non-negative
    material.baseColorFactor = max(material.baseColorFactor, vec4(0.0));
    material.emissiveFactor = max(material.emissiveFactor, vec3(0.0));
}

void ValidatePBRParameters(inout PBRParameters pbr) {
    // Clamp values to valid ranges
    pbr.metallic = clamp(pbr.metallic, 0.0, 1.0);
    pbr.roughness = clamp(pbr.roughness, 0.04, 1.0); // Prevent division by zero
    pbr.ao = clamp(pbr.ao, 0.0, 1.0);
    pbr.alpha = clamp(pbr.alpha, 0.0, 1.0);
    
    // Ensure vectors are normalized
    pbr.normal = normalize(pbr.normal);
    
    // Ensure colors are non-negative
    pbr.albedo = max(pbr.albedo, vec3(0.0));
    pbr.emissive = max(pbr.emissive, vec3(0.0));
}

// Material sampling functions
PBRParameters SamplePBRMaterial(
    sampler2D albedoMap,
    sampler2D normalMap,
    sampler2D metallicMap,
    sampler2D roughnessMap,
    sampler2D aoMap,
    sampler2D emissiveMap,
    vec2 uv,
    vec3 tangent,
    vec3 bitangent,
    vec3 normal,
    MaterialProperties material,
    TextureFlags flags
) {
    PBRParameters pbr = CreateDefaultPBRParameters();
    
    // Sample albedo
    if (flags.hasAlbedoMap > 0) {
        vec4 albedoSample = texture(albedoMap, uv);
        pbr.albedo = SRGBToLinear(albedoSample.rgb);
        pbr.alpha = albedoSample.a;
    } else {
        pbr.albedo = material.baseColorFactor.rgb;
        pbr.alpha = material.baseColorFactor.a;
    }
    
    // Sample normal map
    if (flags.hasNormalMap > 0) {
        vec3 normalSample = texture(normalMap, uv).rgb * 2.0 - 1.0;
        normalSample.xy *= material.normalScale;
        
        mat3 TBN = mat3(normalize(tangent), normalize(bitangent), normalize(normal));
        pbr.normal = normalize(TBN * normalSample);
    } else {
        pbr.normal = normalize(normal);
    }
    
    // Sample metallic and roughness
    if (flags.usePackedTextures > 0) {
        // Packed texture (metallic in R
