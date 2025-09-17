// Shadow mapping and shadow utilities
// Author: inkbytefo
// Project: AstralEngine

#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

#include "common.glsl"

// Shadow map types
const int SHADOW_TYPE_HARD = 0;
const int SHADOW_TYPE_SOFT = 1;
const int SHADOW_TYPE_PCF = 2;
const int SHADOW_TYPE_PCSS = 3;
const int SHADOW_TYPE_VSM = 4;

// Shadow cascade data
struct ShadowCascade
{
    mat4 viewProj;
    float splitDepth;
    vec2 texelSize;
};

// Shadow settings
struct ShadowSettings
{
    int shadowType;
    float bias;
    float normalBias;
    int pcfSamples;
    float lightSize;
    float blockerSearchSamples;
    float pcfFilterSize;
};

// Basic shadow calculation
float CalculateShadowHard(sampler2D shadowMap, vec3 shadowPos, float bias)
{
    float currentDepth = shadowPos.z;
    float closestDepth = texture(shadowMap, shadowPos.xy).r;
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
    return shadow;
}

// Percentage-Closer Filtering (PCF)
float CalculateShadowPCF(sampler2D shadowMap, vec3 shadowPos, float bias, int samples, vec2 texelSize)
{
    float shadow = 0.0;
    float currentDepth = shadowPos.z;
    
    for (int x = -samples; x <= samples; ++x)
    {
        for (int y = -samples; y <= samples; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize;
            float closestDepth = texture(shadowMap, shadowPos.xy + offset).r;
            shadow += currentDepth - bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * samples + 1) * (2 * samples + 1));
    return shadow / totalSamples;
}

// Percentage-Closer Soft Shadows (PCSS)
float CalculateShadowPCSS(sampler2D shadowMap, vec3 shadowPos, ShadowSettings settings, vec2 texelSize)
{
    float currentDepth = shadowPos.z;
    
    // Blocker search
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    
    for (int x = -int(settings.blockerSearchSamples); x <= int(settings.blockerSearchSamples); ++x)
    {
        for (int y = -int(settings.blockerSearchSamples); y <= int(settings.blockerSearchSamples); ++y)
        {
            vec2 offset = vec2(x, y) * texelSize * settings.lightSize;
            float closestDepth = texture(shadowMap, shadowPos.xy + offset).r;
            
            if (closestDepth < currentDepth)
            {
                blockerSum += currentDepth - closestDepth;
                blockerCount += 1.0;
            }
        }
    }
    
    if (blockerCount < 1.0)
        return 1.0;
    
    float avgBlockerDepth = blockerSum / blockerCount;
    float penumbraRatio = (currentDepth - avgBlockerDepth) / avgBlockerDepth;
    float filterSize = penumbraRatio * settings.lightSize * texelSize.x;
    
    // PCF with dynamic filter size
    float shadow = 0.0;
    int filterSamples = int(settings.pcfFilterSize);
    
    for (int x = -filterSamples; x <= filterSamples; ++x)
    {
        for (int y = -filterSamples; y <= filterSamples; ++y)
        {
            vec2 offset = vec2(x, y) * filterSize;
            float closestDepth = texture(shadowMap, shadowPos.xy + offset).r;
            shadow += currentDepth - settings.bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * filterSamples + 1) * (2 * filterSamples + 1));
    return shadow / totalSamples;
}

// Variance Shadow Maps (VSM)
float CalculateShadowVSM(sampler2D shadowMap, vec3 shadowPos, float bias, float lightBleedingReduction)
{
    vec2 moments = texture(shadowMap, shadowPos.xy).rg;
    float currentDepth = shadowPos.z;
    
    // Check if fragment is in shadow
    if (currentDepth <= moments.x + bias)
        return 1.0;
    
    // Calculate variance
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, 0.00002);
    
    // Calculate probability of light
    float d = currentDepth - moments.x;
    float p_max = variance / (variance + d * d);
    
    // Reduce light bleeding
    p_max = clamp((p_max - lightBleedingReduction) / (1.0 - lightBleedingReduction), 0.0, 1.0);
    
    return p_max;
}

// Cascaded Shadow Maps (CSM)
float CalculateCascadedShadow(sampler2D shadowMaps[MAX_SHADOW_CASCADES], 
                             ShadowCascade cascades[MAX_SHADOW_CASCADES],
                             vec3 worldPos, vec3 normal, vec3 lightDir,
                             ShadowSettings settings)
{
    // Select cascade
    int cascadeIndex = 0;
    for (int i = 0; i < MAX_SHADOW_CASCADES - 1; ++i)
    {
        if (worldPos.z < cascades[i + 1].splitDepth)
        {
            cascadeIndex = i + 1;
            break;
        }
    }
    
    // Transform to shadow space
    vec4 shadowPos = cascades[cascadeIndex].viewProj * vec4(worldPos, 1.0);
    shadowPos.xyz = shadowPos.xyz / shadowPos.w;
    shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
    
    // Apply normal bias
    float normalBias = settings.normalBias * tan(acos(max(dot(normal, lightDir), 0.0)));
    shadowPos.z -= normalBias;
    
    // Calculate shadow based on type
    float shadow = 1.0;
    
    switch (settings.shadowType)
    {
        case SHADOW_TYPE_HARD:
            shadow = CalculateShadowHard(shadowMaps[cascadeIndex], shadowPos.xyz, settings.bias);
            break;
        case SHADOW_TYPE_SOFT:
            shadow = CalculateShadowPCF(shadowMaps[cascadeIndex], shadowPos.xyz, settings.bias, 1, cascades[cascadeIndex].texelSize);
            break;
        case SHADOW_TYPE_PCF:
            shadow = CalculateShadowPCF(shadowMaps[cascadeIndex], shadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex].texelSize);
            break;
        case SHADOW_TYPE_PCSS:
            shadow = CalculateShadowPCSS(shadowMaps[cascadeIndex], shadowPos.xyz, settings, cascades[cascadeIndex].texelSize);
            break;
        case SHADOW_TYPE_VSM:
            shadow = CalculateShadowVSM(shadowMaps[cascadeIndex], shadowPos.xyz, settings.bias, 0.2);
            break;
    }
    
    // Cascade blending
    float blendFactor = 0.0;
    if (cascadeIndex < MAX_SHADOW_CASCADES - 1)
    {
        float nextSplit = cascades[cascadeIndex + 1].splitDepth;
        float currentSplit = cascades[cascadeIndex].splitDepth;
        blendFactor = (worldPos.z - currentSplit) / (nextSplit - currentSplit);
        blendFactor = clamp(blendFactor, 0.0, 1.0);
    }
    
    // Blend with next cascade if needed
    if (blendFactor > 0.0 && cascadeIndex < MAX_SHADOW_CASCADES - 1)
    {
        vec4 nextShadowPos = cascades[cascadeIndex + 1].viewProj * vec4(worldPos, 1.0);
        nextShadowPos.xyz = nextShadowPos.xyz / nextShadowPos.w;
        nextShadowPos.xy = nextShadowPos.xy * 0.5 + 0.5;
        nextShadowPos.z -= normalBias;
        
        float nextShadow = 1.0;
        switch (settings.shadowType)
        {
            case SHADOW_TYPE_HARD:
                nextShadow = CalculateShadowHard(shadowMaps[cascadeIndex + 1], nextShadowPos.xyz, settings.bias);
                break;
            case SHADOW_TYPE_SOFT:
                nextShadow = CalculateShadowPCF(shadowMaps[cascadeIndex + 1], nextShadowPos.xyz, settings.bias, 1, cascades[cascadeIndex + 1].texelSize);
                break;
            case SHADOW_TYPE_PCF:
                nextShadow = CalculateShadowPCF(shadowMaps[cascadeIndex + 1], nextShadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex + 1].texelSize);
                break;
            case SHADOW_TYPE_PCSS:
                nextShadow = CalculateShadowPCSS(shadowMaps[cascadeIndex + 1], nextShadowPos.xyz, settings, cascades[cascadeIndex + 1].texelSize);
                break;
            case SHADOW_TYPE_VSM:
                nextShadow = CalculateShadowVSM(shadowMaps[cascadeIndex + 1], nextShadowPos.xyz, settings.bias, 0.2);
                break;
        }
        
        shadow = mix(shadow, nextShadow, blendFactor);
    }
    
    return shadow;
}

// Contact hardening shadows
float CalculateContactHardeningShadow(sampler2D shadowMap, vec3 shadowPos, float bias, vec2 texelSize)
{
    float currentDepth = shadowPos.z;
    
    // Calculate average blocker depth
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize;
            float closestDepth = texture(shadowMap, shadowPos.xy + offset).r;
            
            if (closestDepth < currentDepth)
            {
                blockerSum += closestDepth;
                blockerCount += 1.0;
            }
        }
    }
    
    if (blockerCount < 1.0)
        return 1.0;
    
    float avgBlockerDepth = blockerSum / blockerCount;
    float penumbraSize = (currentDepth - avgBlockerDepth) / avgBlockerDepth;
    
    // Dynamic PCF based on penumbra size
    int samples = int(mix(1, 5, penumbraSize));
    float shadow = 0.0;
    
    for (int x = -samples; x <= samples; ++x)
    {
        for (int y = -samples; y <= samples; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize * penumbraSize;
            float closestDepth = texture(shadowMap, shadowPos.xy + offset).r;
            shadow += currentDepth - bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * samples + 1) * (2 * samples + 1));
    return shadow / totalSamples;
}

// Shadow utilities
vec3 GetShadowCoord(mat4 shadowMatrix, vec3 worldPos)
{
    vec4 shadowPos = shadowMatrix * vec4(worldPos, 1.0);
    shadowPos.xyz = shadowPos.xyz / shadowPos.w;
    shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
    return shadowPos.xyz;
}

float GetShadowBias(vec3 normal, vec3 lightDir, float minBias, float maxBias)
{
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float bias = max(maxBias * (1.0 - cosTheta), minBias);
    return bias;
}

// Debug shadow cascades
vec3 GetCascadeColor(int cascadeIndex)
{
    vec3 colors[MAX_SHADOW_CASCADES] = vec3[MAX_SHADOW_CASCADES](
        vec3(1.0, 0.0, 0.0),  // Red
        vec3(0.0, 1.0, 0.0),  // Green
        vec3(0.0, 0.0, 1.0),  // Blue
        vec3(1.0, 1.0, 0.0)   // Yellow
    );
    return colors[cascadeIndex];
}

// Shadow map visualization
vec3 VisualizeShadowMap(sampler2D shadowMap, vec2 uv)
{
    float depth = texture(shadowMap, uv).r;
    return vec3(depth, depth, depth);
}

// Shadow settings creation
ShadowSettings CreateShadowSettings(int shadowType, float bias, float normalBias, int pcfSamples, float lightSize)
{
    ShadowSettings settings;
    settings.shadowType = shadowType;
    settings.bias = bias;
    settings.normalBias = normalBias;
    settings.pcfSamples = pcfSamples;
    settings.lightSize = lightSize;
    settings.blockerSearchSamples = 3.0;
    settings.pcfFilterSize = 5.0;
    return settings;
}

#endif // SHADOWS_GLSL
