// Lighting calculations and utilities
// Author: inkbytefo
// Project: AstralEngine

#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "common.glsl"

// Light structures
struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
    vec4 shadowCascadeSplits;
    mat4 shadowMatrices[MAX_SHADOW_CASCADES];
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
    float attenuation;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float radius;
    float innerCutoff;
    float outerCutoff;
    float attenuation;
};

// Light attenuation
float CalculateAttenuation(float distance, float radius, float attenuation)
{
    float attenuationFactor = 1.0 / (1.0 + attenuation * distance * distance);
    float radiusAttenuation = 1.0 - smoothstep(radius * 0.5, radius, distance);
    return attenuationFactor * radiusAttenuation;
}

// Spot light falloff
float CalculateSpotFalloff(vec3 lightDir, vec3 spotDir, float innerCutoff, float outerCutoff)
{
    float theta = dot(lightDir, normalize(-spotDir));
    float epsilon = innerCutoff - outerCutoff;
    float falloff = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    return falloff * falloff;
}

// Diffuse lighting (Lambert)
vec3 CalculateDiffuse(vec3 normal, vec3 lightDir, vec3 lightColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    return lightColor * NdotL;
}

// Specular lighting (Blinn-Phong)
vec3 CalculateSpecularBlinnPhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor, float shininess)
{
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);
    float specular = pow(NdotH, shininess);
    return lightColor * specular;
}

// Specular lighting (Phong)
vec3 CalculateSpecularPhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor, float shininess)
{
    vec3 reflectDir = reflect(-lightDir, normal);
    float VdotR = max(dot(viewDir, reflectDir), 0.0);
    float specular = pow(VdotR, shininess);
    return lightColor * specular;
}

// Fresnel effect (Schlick's approximation)
vec3 CalculateFresnel(vec3 F0, vec3 viewDir, vec3 normal)
{
    float VdotH = max(dot(viewDir, normal), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

// Normal distribution function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(normal, halfway), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    
    return numerator / denominator;
}

// Geometry function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float numerator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
    
    return numerator / denominator;
}

// Geometry function (Smith)
float GeometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness)
{
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Directional light calculation
vec3 CalculateDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 lightDir = normalize(-light.direction);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Attenuation
    float attenuation = light.intensity;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Point light calculation
vec3 CalculatePointLight(PointLight light, vec3 worldPos, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 lightDir = normalize(light.position - worldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Distance and attenuation
    float distance = length(light.position - worldPos);
    float attenuation = CalculateAttenuation(distance, light.radius, light.attenuation) * light.intensity;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Spot light calculation
vec3 CalculateSpotLight(SpotLight light, vec3 worldPos, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 lightDir = normalize(light.position - worldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Distance and attenuation
    float distance = length(light.position - worldPos);
    float attenuation = CalculateAttenuation(distance, light.radius, light.attenuation) * light.intensity;
    
    // Spot falloff
    float spotFalloff = CalculateSpotFalloff(lightDir, light.direction, light.innerCutoff, light.outerCutoff);
    attenuation *= spotFalloff;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Ambient lighting
vec3 CalculateAmbient(vec3 albedo, vec3 ambientLight, float ao, float metallic)
{
    vec3 ambient = albedo * ambientLight * ao;
    ambient *= (1.0 - metallic) * 0.1; // Metals have less ambient
    return ambient;
}

// Image Based Lighting (IBL) - Simplified
vec3 CalculateIBL(vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness, vec3 F0, sampler2D brdfLUT, samplerCube irradianceMap, samplerCube prefilterMap)
{
    vec3 N = normal;
    vec3 V = viewDir;
    vec3 R = reflect(-V, N);
    
    float NdotV = max(dot(N, V), 0.0);
    
    // Indirect diffuse
    vec3 F = CalculateFresnel(F0, V, N);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    // Indirect specular
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    return kD * diffuse + specular;
}

#endif // LIGHTING_GLSL
