// Physically Based Rendering (PBR) functions and utilities
// Author: inkbytefo
// Project: AstralEngine

#ifndef PBR_GLSL
#define PBR_GLSL

#include "common.glsl"
#include "lighting.glsl"

// Material structure for PBR
struct PBRMaterial
{
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 normal;
    vec3 emissive;
    float alpha;
};

// PBR surface properties
struct PBSurface
{
    vec3 albedo;
    vec3 normal;
    vec3 viewDir;
    vec3 F0;
    float roughness;
    float metallic;
    float ao;
    vec3 emissive;
    float alpha;
};

// Calculate base reflectivity (F0) from metallic and albedo
vec3 CalculateF0(vec3 albedo, float metallic)
{
    vec3 F0 = vec3(0.04); // Dielectric base reflectivity
    return mix(F0, albedo, metallic);
}

// Cook-Torrance BRDF calculation
vec3 CalculateBRDF(vec3 lightDir, vec3 viewDir, vec3 normal, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Calculate individual BRDF components
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    // Calculate numerator and denominator
    vec3 numerator = NDF * G * F;
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    
    return numerator / denominator;
}

// Full PBR lighting calculation
vec3 CalculatePBRLighting(PBSurface surface, 
                         DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS],
                         PointLight pointLights[MAX_POINT_LIGHTS],
                         SpotLight spotLights[MAX_SPOT_LIGHTS],
                         vec3 ambientLight,
                         sampler2D brdfLUT,
                         samplerCube irradianceMap,
                         samplerCube prefilterMap)
{
    vec3 Lo = vec3(0.0); // Radiance outgoing
    
    // Calculate F0
    vec3 F0 = CalculateF0(surface.albedo, surface.metallic);
    
    // Directional lights
    for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; ++i)
    {
        if (directionalLights[i].intensity > 0.0)
        {
            Lo += CalculateDirectionalLight(directionalLights[i], surface.normal, surface.viewDir, 
                                          surface.albedo, surface.metallic, surface.roughness, F0);
        }
    }
    
    // Point lights
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        if (pointLights[i].intensity > 0.0)
        {
            Lo += CalculatePointLight(pointLights[i], surface.viewDir, surface.normal, surface.viewDir,
                                    surface.albedo, surface.metallic, surface.roughness, F0);
        }
    }
    
    // Spot lights
    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
    {
        if (spotLights[i].intensity > 0.0)
        {
            Lo += CalculateSpotLight(spotLights[i], surface.viewDir, surface.normal, surface.viewDir,
                                   surface.albedo, surface.metallic, surface.roughness, F0);
        }
    }
    
    // Ambient and IBL
    vec3 ambient = CalculateAmbient(surface.albedo, ambientLight, surface.ao, surface.metallic);
    vec3 ibl = CalculateIBL(surface.normal, surface.viewDir, surface.albedo, surface.metallic, 
                           surface.roughness, F0, brdfLUT, irradianceMap, prefilterMap);
    
    // Combine all lighting
    vec3 color = Lo + ambient + ibl * surface.ao;
    
    // Add emissive
    color += surface.emissive;
    
    return color;
}

// Simplified PBR for mobile/low-end devices
vec3 CalculateSimplePBR(PBSurface surface, vec3 lightDir, vec3 lightColor, float lightIntensity)
{
    vec3 halfwayDir = normalize(lightDir + surface.viewDir);
    
    // Calculate F0
    vec3 F0 = CalculateF0(surface.albedo, surface.metallic);
    
    // Diffuse (Lambert)
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    vec3 diffuse = surface.albedo / PI * NdotL;
    
    // Specular (simplified Cook-Torrance)
    float NDF = DistributionGGX(surface.normal, halfwayDir, surface.roughness);
    float G = GeometrySmith(surface.normal, surface.viewDir, lightDir, surface.roughness);
    vec3 F = CalculateFresnel(F0, surface.viewDir, halfwayDir);
    
    vec3 numerator = NDF * G * F;
    float NdotV = max(dot(surface.normal, surface.viewDir), 0.0);
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - surface.metallic;
    
    // Combine
    vec3 Lo = (kD * diffuse + specular) * lightColor * lightIntensity;
    
    // Add ambient
    vec3 ambient = surface.albedo * 0.1 * surface.ao;
    
    return Lo + ambient + surface.emissive;
}

// PBR material utilities
PBRMaterial CreatePBRMaterial(vec3 albedo, float metallic, float roughness, float ao, vec3 normal, vec3 emissive, float alpha)
{
    PBRMaterial material;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;
    material.normal = normalize(normal);
    material.emissive = emissive;
    material.alpha = alpha;
    return material;
}

PBSurface CreatePBSurface(PBRMaterial material, vec3 worldPos, vec3 viewPos)
{
    PBSurface surface;
    surface.albedo = material.albedo;
    surface.normal = material.normal;
    surface.viewDir = normalize(viewPos - worldPos);
    surface.roughness = max(material.roughness, 0.04); // Prevent division by zero
    surface.metallic = clamp(material.metallic, 0.0, 1.0);
    surface.ao = material.ao;
    surface.emissive = material.emissive;
    surface.alpha = material.alpha;
    return surface;
}

// Material parameter validation and correction
void ValidatePBRMaterial(inout PBRMaterial material)
{
    // Clamp values to valid ranges
    material.metallic = clamp(material.metallic, 0.0, 1.0);
    material.roughness = clamp(material.roughness, 0.04, 1.0); // Prevent division by zero
    material.ao = clamp(material.ao, 0.0, 1.0);
    material.alpha = clamp(material.alpha, 0.0, 1.0);
    
    // Ensure normal is normalized
    material.normal = normalize(material.normal);
    
    // Ensure albedo is non-negative
    material.albedo = max(material.albedo, vec3(0.0));
    material.emissive = max(material.emissive, vec3(0.0));
}

// PBR material blending
PBRMaterial BlendPBRMaterials(PBRMaterial mat1, PBRMaterial mat2, float t)
{
    PBRMaterial result;
    result.albedo = mix(mat1.albedo, mat2.albedo, t);
    result.metallic = mix(mat1.metallic, mat2.metallic, t);
    result.roughness = mix(mat1.roughness, mat2.roughness, t);
    result.ao = mix(mat1.ao, mat2.ao, t);
    result.normal = normalize(mix(mat1.normal, mat2.normal, t));
    result.emissive = mix(mat1.emissive, mat2.emissive, t);
    result.alpha = mix(mat1.alpha, mat2.alpha, t);
    return result;
}

// PBR material from texture maps
PBRMaterial SamplePBRMaterial(sampler2D albedoMap, sampler2D normalMap, sampler2D metallicMap, 
                             sampler2D roughnessMap, sampler2D aoMap, sampler2D emissiveMap,
                             vec2 uv, vec3 tangent, vec3 bitangent, vec3 normal)
{
    PBRMaterial material;
    
    // Sample textures
    vec4 albedoSample = SampleTexture(albedoMap, uv);
    material.albedo = SRGBToLinear(albedoSample.rgb);
    material.alpha = albedoSample.a;
    
    // Normal mapping
    vec3 normalSample = SampleTexture(normalMap, uv).rgb * 2.0 - 1.0;
    vec3 t = normalize(tangent);
    vec3 b = normalize(bitangent);
    vec3 n = normalize(normal);
    mat3 TBN;
    TBN[0] = t;
    TBN[1] = b;
    TBN[2] = n;
    material.normal = normalize(TBN * normalSample);
    
    // Metallic and roughness
    material.metallic = SampleTexture(metallicMap, uv).r;
    material.roughness = SampleTexture(roughnessMap, uv).r;
    
    // Ambient occlusion
    material.ao = SampleTexture(aoMap, uv).r;
    
    // Emissive
    material.emissive = SRGBToLinear(SampleTexture(emissiveMap, uv).rgb);
    
    // Validate
    ValidatePBRMaterial(material);
    
    return material;
}

// PBR material from packed texture (metallic in R, roughness in G, AO in B)
PBRMaterial SamplePBRMaterialPacked(sampler2D albedoMap, sampler2D normalMap, sampler2D metallicRoughnessAOMap,
                                   sampler2D emissiveMap, vec2 uv, vec3 tangent, vec3 bitangent, vec3 normal)
{
    PBRMaterial material;
    
    // Sample textures
    vec4 albedoSample = SampleTexture(albedoMap, uv);
    material.albedo = SRGBToLinear(albedoSample.rgb);
    material.alpha = albedoSample.a;
    
    // Normal mapping
    vec3 normalSample = SampleTexture(normalMap, uv).rgb * 2.0 - 1.0;
    vec3 t = normalize(tangent);
    vec3 b = normalize(bitangent);
    vec3 n = normalize(normal);
    mat3 TBN = mat3(t.x, b.x, n.x, t.y, b.y, n.y, t.z, b.z, n.z);
    material.normal = normalize(TBN * normalSample);
    
    // Packed metallic/roughness/AO
    vec3 mraSample = SampleTexture(metallicRoughnessAOMap, uv).rgb;
    material.metallic = mraSample.r;
    material.roughness = mraSample.g;
    material.ao = mraSample.b;
    
    // Emissive
    material.emissive = SRGBToLinear(SampleTexture(emissiveMap, uv).rgb);
    
    // Validate
    ValidatePBRMaterial(material);
    
    return material;
}

#endif // PBR_GLSL
