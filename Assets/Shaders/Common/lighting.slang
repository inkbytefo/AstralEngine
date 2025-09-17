// Lighting calculations and utilities
// Author: inkbytefo
// Project: AstralEngine
// Description: Lighting calculations for SLANG shaders

// Light structures
struct DirectionalLight
{
    float3 direction;
    float3 color;
    float intensity;
    float4 shadowCascadeSplits;
    float4x4 shadowMatrices[4];
};

struct PointLight
{
    float3 position;
    float3 color;
    float intensity;
    float radius;
    float attenuation;
};

struct SpotLight
{
    float3 position;
    float3 direction;
    float3 color;
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
float CalculateSpotFalloff(float3 lightDir, float3 spotDir, float innerCutoff, float outerCutoff)
{
    float theta = dot(lightDir, normalize(-spotDir));
    float epsilon = innerCutoff - outerCutoff;
    float falloff = saturate((theta - outerCutoff) / epsilon);
    return falloff * falloff;
}

// Diffuse lighting (Lambert)
float3 CalculateDiffuse(float3 normal, float3 lightDir, float3 lightColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    return lightColor * NdotL;
}

// Specular lighting (Blinn-Phong)
float3 CalculateSpecularBlinnPhong(float3 normal, float3 lightDir, float3 viewDir, float3 lightColor, float shininess)
{
    float3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);
    float specular = pow(NdotH, shininess);
    return lightColor * specular;
}

// Specular lighting (Phong)
float3 CalculateSpecularPhong(float3 normal, float3 lightDir, float3 viewDir, float3 lightColor, float shininess)
{
    float3 reflectDir = reflect(-lightDir, normal);
    float VdotR = max(dot(viewDir, reflectDir), 0.0);
    float specular = pow(VdotR, shininess);
    return lightColor * specular;
}

// Fresnel effect (Schlick's approximation)
float3 CalculateFresnel(float3 F0, float3 viewDir, float3 normal)
{
    float VdotH = max(dot(viewDir, normal), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

// Normal distribution function (Trowbridge-Reitz GGX)
float DistributionGGX(float3 normal, float3 halfway, float roughness)
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
float GeometrySmith(float3 normal, float3 viewDir, float3 lightDir, float roughness)
{
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Directional light calculation
float3 CalculateDirectionalLight(DirectionalLight light, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float3 F0)
{
    float3 lightDir = normalize(-light.direction);
    float3 halfwayDir = normalize(lightDir + viewDir);
    
    // Attenuation
    float attenuation = light.intensity;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    // Energy conservation
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Point light calculation
float3 CalculatePointLight(PointLight light, float3 worldPos, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float3 F0)
{
    float3 lightDir = normalize(light.position - worldPos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    
    // Distance and attenuation
    float distance = length(light.position - worldPos);
    float attenuation = CalculateAttenuation(distance, light.radius, light.attenuation) * light.intensity;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    // Energy conservation
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Spot light calculation
float3 CalculateSpotLight(SpotLight light, float3 worldPos, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float3 F0)
{
    float3 lightDir = normalize(light.position - worldPos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    
    // Distance and attenuation
    float distance = length(light.position - worldPos);
    float attenuation = CalculateAttenuation(distance, light.radius, light.attenuation) * light.intensity;
    
    // Spot falloff
    float spotFalloff = CalculateSpotFalloff(lightDir, light.direction, light.innerCutoff, light.outerCutoff);
    attenuation *= spotFalloff;
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = CalculateDiffuse(normal, lightDir, light.color) * attenuation;
    
    // Specular
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = CalculateFresnel(F0, viewDir, halfwayDir);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    // Energy conservation
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    
    return (kD * albedo / PI + specular) * light.color * NdotL * attenuation;
}

// Ambient lighting
float3 CalculateAmbient(float3 albedo, float3 ambientLight, float ao, float metallic)
{
    float3 ambient = albedo * ambientLight * ao;
    ambient *= (1.0 - metallic) * 0.1; // Metals have less ambient
    return ambient;
}

// Image Based Lighting (IBL) - Simplified
float3 CalculateIBL(float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float3 F0, 
                    Texture2D brdfLUT, TextureCube irradianceMap, TextureCube prefilterMap, SamplerState samp)
{
    float3 N = normal;
    float3 V = viewDir;
    float3 R = reflect(-V, N);
    
    float NdotV = max(dot(N, V), 0.0);
    
    // Indirect diffuse
    float3 F = CalculateFresnel(F0, V, N);
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    
    float3 irradiance = irradianceMap.Sample(samp, N).rgb;
    float3 diffuse = irradiance * albedo;
    
    // Indirect specular
    const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = prefilterMap.SampleLevel(samp, R, roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = brdfLUT.Sample(samp, float2(NdotV, roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    return kD * diffuse + specular;
}
