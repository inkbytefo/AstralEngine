// pbr_lighting.slang
// PBR helper functions based on Cook-Torrance BRDF

#ifndef PBR_LIGHTING_SLANG
#define PBR_LIGHTING_SLANG

#include "lighting_structs.slang"

const float PI = 3.14159265359;

// Normal Distribution Function (NDF) - Trowbridge-Reitz GGX
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Geometry Function - Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

// Smith's method for geometry obstruction
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Calculates the lighting contribution for a single light source
float3 calculatePBR(GPULight light, float3 worldPos, float3 normal, float3 viewDir,
                    float3 albedo, float metallic, float roughness, float ao)
{
    float3 L = float3(0,0,0);
    if (light.type == LIGHT_TYPE_DIRECTIONAL) {
        L = normalize(-light.direction);
    } else {
        L = normalize(light.position - worldPos);
    }

    float3 H = normalize(viewDir + L);
    float distance = length(light.position - worldPos);

    // Radiance
    float attenuation = 1.0;
    if (light.type != LIGHT_TYPE_DIRECTIONAL) {
        if (distance > light.range) return float3(0,0,0);
        attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    }
    float3 radiance = light.color * light.intensity * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, viewDir, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), lerp(float3(0.04, 0.04, 0.04), albedo, metallic));

    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic);

    float NdotL = max(dot(normal, L), 0.0);
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + 0.0001; // Add epsilon to prevent division by zero
    float3 specular = numerator / denominator;

    // Add to outgoing radiance
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

#endif // PBR_LIGHTING_SLANG
