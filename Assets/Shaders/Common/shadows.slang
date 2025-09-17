// Shadow mapping and shadow utilities
// Author: inkbytefo
// Project: AstralEngine
// Description: Shadow mapping utilities for SLANG shaders

// Shadow map types
static const int SHADOW_TYPE_HARD = 0;
static const int SHADOW_TYPE_SOFT = 1;
static const int SHADOW_TYPE_PCF = 2;
static const int SHADOW_TYPE_PCSS = 3;
static const int SHADOW_TYPE_VSM = 4;

// Shadow cascade data
struct ShadowCascade
{
    float4x4 viewProj;
    float splitDepth;
    float2 texelSize;
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
float CalculateShadowHard(Texture2D shadowMap, SamplerState samp, float3 shadowPos, float bias)
{
    float currentDepth = shadowPos.z;
    float closestDepth = shadowMap.Sample(samp, shadowPos.xy).r;
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
    return shadow;
}

// Percentage-Closer Filtering (PCF)
float CalculateShadowPCF(Texture2D shadowMap, SamplerState samp, float3 shadowPos, float bias, int samples, float2 texelSize)
{
    float shadow = 0.0;
    float currentDepth = shadowPos.z;
    
    for (int x = -samples; x <= samples; ++x)
    {
        for (int y = -samples; y <= samples; ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            float closestDepth = shadowMap.Sample(samp, shadowPos.xy + offset).r;
            shadow += currentDepth - bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * samples + 1) * (2 * samples + 1));
    return shadow / totalSamples;
}

// Percentage-Closer Soft Shadows (PCSS)
float CalculateShadowPCSS(Texture2D shadowMap, SamplerState samp, float3 shadowPos, ShadowSettings settings, float2 texelSize)
{
    float currentDepth = shadowPos.z;
    
    // Blocker search
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    
    for (int x = -int(settings.blockerSearchSamples); x <= int(settings.blockerSearchSamples); ++x)
    {
        for (int y = -int(settings.blockerSearchSamples); y <= int(settings.blockerSearchSamples); ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize * settings.lightSize;
            float closestDepth = shadowMap.Sample(samp, shadowPos.xy + offset).r;
            
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
            float2 offset = float2(float(x), float(y)) * filterSize;
            float closestDepth = shadowMap.Sample(samp, shadowPos.xy + offset).r;
            shadow += currentDepth - settings.bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * filterSamples + 1) * (2 * filterSamples + 1));
    return shadow / totalSamples;
}

// Variance Shadow Maps (VSM)
float CalculateShadowVSM(Texture2D shadowMap, SamplerState samp, float3 shadowPos, float bias, float lightBleedingReduction)
{
    float2 moments = shadowMap.Sample(samp, shadowPos.xy).rg;
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
    p_max = saturate((p_max - lightBleedingReduction) / (1.0 - lightBleedingReduction));
    
    return p_max;
}

// Cascaded Shadow Maps (CSM) - Fixed version without dynamic indexing
float CalculateCascadedShadow(Texture2D shadowMap0, Texture2D shadowMap1, Texture2D shadowMap2, Texture2D shadowMap3,
                             ShadowCascade cascades[4],
                             float3 worldPos, float3 normal, float3 lightDir,
                             ShadowSettings settings, SamplerState samp)
{
    // Select cascade
    int cascadeIndex = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (worldPos.z < cascades[i + 1].splitDepth)
        {
            cascadeIndex = i + 1;
            break;
        }
    }
    
    // Transform to shadow space
    float4 shadowPos = mul(cascades[cascadeIndex].viewProj, float4(worldPos, 1.0));
    shadowPos.xyz = shadowPos.xyz / shadowPos.w;
    shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
    
    // Apply normal bias
    float normalBias = settings.normalBias * tan(acos(max(dot(normal, lightDir), 0.0)));
    shadowPos.z -= normalBias;
    
    // Calculate shadow based on type - use if-else instead of dynamic indexing
    float shadow = 1.0;
    
    if (cascadeIndex == 0)
    {
        switch (settings.shadowType)
        {
            case SHADOW_TYPE_HARD:
                shadow = CalculateShadowHard(shadowMap0, samp, shadowPos.xyz, settings.bias);
                break;
            case SHADOW_TYPE_SOFT:
                shadow = CalculateShadowPCF(shadowMap0, samp, shadowPos.xyz, settings.bias, 1, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCF:
                shadow = CalculateShadowPCF(shadowMap0, samp, shadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCSS:
                shadow = CalculateShadowPCSS(shadowMap0, samp, shadowPos.xyz, settings, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_VSM:
                shadow = CalculateShadowVSM(shadowMap0, samp, shadowPos.xyz, settings.bias, 0.2);
                break;
        }
    }
    else if (cascadeIndex == 1)
    {
        switch (settings.shadowType)
        {
            case SHADOW_TYPE_HARD:
                shadow = CalculateShadowHard(shadowMap1, samp, shadowPos.xyz, settings.bias);
                break;
            case SHADOW_TYPE_SOFT:
                shadow = CalculateShadowPCF(shadowMap1, samp, shadowPos.xyz, settings.bias, 1, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCF:
                shadow = CalculateShadowPCF(shadowMap1, samp, shadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCSS:
                shadow = CalculateShadowPCSS(shadowMap1, samp, shadowPos.xyz, settings, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_VSM:
                shadow = CalculateShadowVSM(shadowMap1, samp, shadowPos.xyz, settings.bias, 0.2);
                break;
        }
    }
    else if (cascadeIndex == 2)
    {
        switch (settings.shadowType)
        {
            case SHADOW_TYPE_HARD:
                shadow = CalculateShadowHard(shadowMap2, samp, shadowPos.xyz, settings.bias);
                break;
            case SHADOW_TYPE_SOFT:
                shadow = CalculateShadowPCF(shadowMap2, samp, shadowPos.xyz, settings.bias, 1, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCF:
                shadow = CalculateShadowPCF(shadowMap2, samp, shadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCSS:
                shadow = CalculateShadowPCSS(shadowMap2, samp, shadowPos.xyz, settings, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_VSM:
                shadow = CalculateShadowVSM(shadowMap2, samp, shadowPos.xyz, settings.bias, 0.2);
                break;
        }
    }
    else if (cascadeIndex == 3)
    {
        switch (settings.shadowType)
        {
            case SHADOW_TYPE_HARD:
                shadow = CalculateShadowHard(shadowMap3, samp, shadowPos.xyz, settings.bias);
                break;
            case SHADOW_TYPE_SOFT:
                shadow = CalculateShadowPCF(shadowMap3, samp, shadowPos.xyz, settings.bias, 1, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCF:
                shadow = CalculateShadowPCF(shadowMap3, samp, shadowPos.xyz, settings.bias, settings.pcfSamples, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_PCSS:
                shadow = CalculateShadowPCSS(shadowMap3, samp, shadowPos.xyz, settings, cascades[cascadeIndex].texelSize);
                break;
            case SHADOW_TYPE_VSM:
                shadow = CalculateShadowVSM(shadowMap3, samp, shadowPos.xyz, settings.bias, 0.2);
                break;
        }
    }
    
    // Cascade blending
    float blendFactor = 0.0;
    if (cascadeIndex < 3)
    {
        float nextSplit = cascades[cascadeIndex + 1].splitDepth;
        float currentSplit = cascades[cascadeIndex].splitDepth;
        blendFactor = (worldPos.z - currentSplit) / (nextSplit - currentSplit);
        blendFactor = saturate(blendFactor);
    }
    
    // Blend with next cascade if needed
    if (blendFactor > 0.0 && cascadeIndex < 3)
    {
        float4 nextShadowPos = mul(cascades[cascadeIndex + 1].viewProj, float4(worldPos, 1.0));
        nextShadowPos.xyz = nextShadowPos.xyz / nextShadowPos.w;
        nextShadowPos.xy = nextShadowPos.xy * 0.5 + 0.5;
        nextShadowPos.z -= normalBias;
        
        float nextShadow = 1.0;
        int nextCascadeIndex = cascadeIndex + 1;
        
        if (nextCascadeIndex == 0)
        {
            switch (settings.shadowType)
            {
                case SHADOW_TYPE_HARD:
                    nextShadow = CalculateShadowHard(shadowMap0, samp, nextShadowPos.xyz, settings.bias);
                    break;
                case SHADOW_TYPE_SOFT:
                    nextShadow = CalculateShadowPCF(shadowMap0, samp, nextShadowPos.xyz, settings.bias, 1, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCF:
                    nextShadow = CalculateShadowPCF(shadowMap0, samp, nextShadowPos.xyz, settings.bias, settings.pcfSamples, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCSS:
                    nextShadow = CalculateShadowPCSS(shadowMap0, samp, nextShadowPos.xyz, settings, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_VSM:
                    nextShadow = CalculateShadowVSM(shadowMap0, samp, nextShadowPos.xyz, settings.bias, 0.2);
                    break;
            }
        }
        else if (nextCascadeIndex == 1)
        {
            switch (settings.shadowType)
            {
                case SHADOW_TYPE_HARD:
                    nextShadow = CalculateShadowHard(shadowMap1, samp, nextShadowPos.xyz, settings.bias);
                    break;
                case SHADOW_TYPE_SOFT:
                    nextShadow = CalculateShadowPCF(shadowMap1, samp, nextShadowPos.xyz, settings.bias, 1, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCF:
                    nextShadow = CalculateShadowPCF(shadowMap1, samp, nextShadowPos.xyz, settings.bias, settings.pcfSamples, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCSS:
                    nextShadow = CalculateShadowPCSS(shadowMap1, samp, nextShadowPos.xyz, settings, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_VSM:
                    nextShadow = CalculateShadowVSM(shadowMap1, samp, nextShadowPos.xyz, settings.bias, 0.2);
                    break;
            }
        }
        else if (nextCascadeIndex == 2)
        {
            switch (settings.shadowType)
            {
                case SHADOW_TYPE_HARD:
                    nextShadow = CalculateShadowHard(shadowMap2, samp, nextShadowPos.xyz, settings.bias);
                    break;
                case SHADOW_TYPE_SOFT:
                    nextShadow = CalculateShadowPCF(shadowMap2, samp, nextShadowPos.xyz, settings.bias, 1, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCF:
                    nextShadow = CalculateShadowPCF(shadowMap2, samp, nextShadowPos.xyz, settings.bias, settings.pcfSamples, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCSS:
                    nextShadow = CalculateShadowPCSS(shadowMap2, samp, nextShadowPos.xyz, settings, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_VSM:
                    nextShadow = CalculateShadowVSM(shadowMap2, samp, nextShadowPos.xyz, settings.bias, 0.2);
                    break;
            }
        }
        else if (nextCascadeIndex == 3)
        {
            switch (settings.shadowType)
            {
                case SHADOW_TYPE_HARD:
                    nextShadow = CalculateShadowHard(shadowMap3, samp, nextShadowPos.xyz, settings.bias);
                    break;
                case SHADOW_TYPE_SOFT:
                    nextShadow = CalculateShadowPCF(shadowMap3, samp, nextShadowPos.xyz, settings.bias, 1, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCF:
                    nextShadow = CalculateShadowPCF(shadowMap3, samp, nextShadowPos.xyz, settings.bias, settings.pcfSamples, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_PCSS:
                    nextShadow = CalculateShadowPCSS(shadowMap3, samp, nextShadowPos.xyz, settings, cascades[nextCascadeIndex].texelSize);
                    break;
                case SHADOW_TYPE_VSM:
                    nextShadow = CalculateShadowVSM(shadowMap3, samp, nextShadowPos.xyz, settings.bias, 0.2);
                    break;
            }
        }
        
        shadow = lerp(shadow, nextShadow, blendFactor);
    }
    
    return shadow;
}

// Contact hardening shadows
float CalculateContactHardeningShadow(Texture2D shadowMap, SamplerState samp, float3 shadowPos, float bias, float2 texelSize)
{
    float currentDepth = shadowPos.z;
    
    // Calculate average blocker depth
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            float closestDepth = shadowMap.Sample(samp, shadowPos.xy + offset).r;
            
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
    int samples = int(lerp(1.0, 5.0, penumbraSize));
    float shadow = 0.0;
    
    for (int x = -samples; x <= samples; ++x)
    {
        for (int y = -samples; y <= samples; ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize * penumbraSize;
            float closestDepth = shadowMap.Sample(samp, shadowPos.xy + offset).r;
            shadow += currentDepth - bias > closestDepth ? 0.0 : 1.0;
        }
    }
    
    float totalSamples = float((2 * samples + 1) * (2 * samples + 1));
    return shadow / totalSamples;
}

// Shadow utilities
float3 GetShadowCoord(float4x4 shadowMatrix, float3 worldPos)
{
    float4 shadowPos = mul(shadowMatrix, float4(worldPos, 1.0));
    shadowPos.xyz = shadowPos.xyz / shadowPos.w;
    shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
    return shadowPos.xyz;
}

float GetShadowBias(float3 normal, float3 lightDir, float minBias, float maxBias)
{
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float bias = max(maxBias * (1.0 - cosTheta), minBias);
    return bias;
}

// Debug shadow cascades
float3 GetCascadeColor(int cascadeIndex)
{
    float3 colors[4] = {
        float3(1.0, 0.0, 0.0),  // Red
        float3(0.0, 1.0, 0.0),  // Green
        float3(0.0, 0.0, 1.0),  // Blue
        float3(1.0, 1.0, 0.0)   // Yellow
    };
    return colors[cascadeIndex];
}

// Shadow map visualization
float3 VisualizeShadowMap(Texture2D shadowMap, SamplerState samp, float2 uv)
{
    float depth = shadowMap.Sample(samp, uv).r;
    return float3(depth, depth, depth);
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
