// Tonemapping Fragment Shader
// Author: inkbytefo
// Project: AstralEngine

#include "../Common/common.slang"

// Inputs from vertex shader
struct VertexOutput {
    [[vk::location(0)]] float2 texCoord : TEXCOORD0;
    [[vk::location(1)]] float3 viewRay : TEXCOORD1;
    float4 position : SV_POSITION;
};

// Output
struct FragmentOutput {
    [[vk::location(0)]] float4 color : SV_TARGET0;
};

// Texture samplers
Texture2D sceneColor : register(t0);
SamplerState sceneColorSampler : register(s0);

Texture2D sceneDepth : register(t1);
SamplerState sceneDepthSampler : register(s1);

Texture2D bloomTexture : register(t2);
SamplerState bloomTextureSampler : register(s2);

Texture2D lensDirt : register(t3);
SamplerState lensDirtSampler : register(s3);

Texture2D colorGrading : register(t4);
SamplerState colorGradingSampler : register(s4);

// Uniform buffer objects
cbuffer UniformBufferObject : register(b0) {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

cbuffer PostProcessingUBO : register(b1) {
    float exposure;
    float gamma;
    int tonemapper;    // 0: None, 1: ACES, 2: Reinhard, 3: Filmic, 4: Custom
    float contrast;
    float brightness;
    float saturation;
    float vignetteIntensity;
    float vignetteRadius;
    float chromaticAberrationIntensity;
    float bloomIntensity;
    float lensDirtIntensity;
    int useColorGrading;
    int useDithering;
    float2 padding;
};

// Push constants
[[vk::push_constant]] cbuffer PushConstants {
    float2 texelSize;
    int useBloom;
    int useVignette;
    int useChromaticAberration;
};

// Tonemapping operators
float3 TonemapNone(float3 color)
{
    return color;
}

float3 TonemapACES(float3 color)
{
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

float3 TonemapReinhard(float3 color)
{
    return color / (color + float3(1.0, 1.0, 1.0));
}

float3 TonemapFilmic(float3 color)
{
    color = max(float3(0.0, 0.0, 0.0), color - float3(0.004, 0.004, 0.004));
    return (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
}

float3 TonemapCustom(float3 color)
{
    // Custom tonemapping curve
    float shoulder = 0.9;
    float midtones = 0.5;
    float highlights = 0.1;
    
    color = color * exposure;
    
    float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
    float tonedLum = lum / (lum + shoulder) * midtones + highlights * lum;
    
    return color * (tonedLum / max(lum, 0.0001));
}

// Color adjustments
float3 AdjustContrast(float3 color, float contrast)
{
    return ((color - 0.5) * max(contrast, 0.0)) + 0.5;
}

float3 AdjustBrightness(float3 color, float brightness)
{
    return color + brightness;
}

float3 AdjustSaturation(float3 color, float saturation)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    return lerp(float3(luminance, luminance, luminance), color, max(saturation, 0.0));
}

// Vignette effect
float3 ApplyVignette(float3 color, float2 uv, float intensity, float radius)
{
    float2 center = float2(0.5, 0.5);
    float dist = distance(uv, center);
    float vignette = smoothstep(radius, radius - 0.1, dist);
    vignette = lerp(1.0, vignette, intensity);
    return color * vignette;
}

// Chromatic aberration
float3 ApplyChromaticAberration(Texture2D tex, SamplerState samp, float2 uv, float intensity)
{
    float2 center = float2(0.5, 0.5);
    float2 dir = uv - center;
    float dist = length(dir);
    
    // Sample red channel slightly offset
    float2 redUV = uv + dir * intensity * 0.01;
    float3 color = float3(
        tex.Sample(samp, redUV).r,
        tex.Sample(samp, uv).g,
        tex.Sample(samp, uv - dir * intensity * 0.01).b
    );
    
    return color;
}

// Bloom application
float3 ApplyBloom(float3 color, Texture2D bloomTex, SamplerState samp, float2 uv, float intensity)
{
    float3 bloom = bloomTex.Sample(samp, uv).rgb;
    return color + bloom * intensity;
}

// Lens dirt
float3 ApplyLensDirt(float3 color, Texture2D dirtTex, SamplerState samp, float2 uv, float intensity)
{
    float3 dirt = dirtTex.Sample(samp, uv).rgb;
    return color + dirt * intensity;
}

// Color grading using LUT
float3 ApplyColorGrading(float3 color, Texture2D lutTex, SamplerState samp)
{
    // Assuming LUT is 32x32x32 or similar
    float blue = color.b * 31.0;
    float2 lutUV;
    
    lutUV.y = floor(blue) / 32.0;
    lutUV.x = (color.r * 31.0 + floor(blue)) / 1024.0;
    
    float3 gradedColor = lutTex.Sample(samp, lutUV).rgb;
    
    // Interpolate with next slice
    lutUV.y = ceil(blue) / 32.0;
    lutUV.x = (color.r * 31.0 + ceil(blue)) / 1024.0;
    
    float3 gradedColor2 = lutTex.Sample(samp, lutUV).rgb;
    float t = frac(blue);
    
    return lerp(gradedColor, gradedColor2, t);
}

// Dithering
float3 ApplyDithering(float3 color, float2 uv, float time)
{
    // Bayer matrix dithering
    float bayer8x8[64] = {
        0.0, 32.0, 8.0, 40.0, 2.0, 34.0, 10.0, 42.0,
        48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
        12.0, 44.0, 4.0, 36.0, 14.0, 46.0, 6.0, 38.0,
        60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
        3.0, 35.0, 11.0, 43.0, 1.0, 33.0, 9.0, 41.0,
        51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
        15.0, 47.0, 7.0, 39.0, 13.0, 45.0, 5.0, 37.0,
        63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
    };
    
    int2 pixelCoord = int2(uv * texelSize);
    int index = (pixelCoord.x % 8) + (pixelCoord.y % 8) * 8;
    float dither = (bayer8x8[index] / 64.0 - 0.5) / 255.0;
    
    return color + dither;
}

// Film grain
float3 ApplyFilmGrain(float3 color, float2 uv, float time)
{
    float grain = Random(uv + time * 0.01) * 0.1;
    return color + grain;
}

FragmentOutput main(VertexOutput input) {
    FragmentOutput output;
    
    // Sample scene color
    float3 color = sceneColor.Sample(sceneColorSampler, input.texCoord).rgb;
    
    // Apply chromatic aberration
    if (useChromaticAberration > 0) {
        color = ApplyChromaticAberration(sceneColor, sceneColorSampler, input.texCoord, chromaticAberrationIntensity);
    }
    
    // Apply bloom
    if (useBloom > 0) {
        color = ApplyBloom(color, bloomTexture, bloomTextureSampler, input.texCoord, bloomIntensity);
        
        // Apply lens dirt if bloom is enabled
        color = ApplyLensDirt(color, lensDirt, lensDirtSampler, input.texCoord, lensDirtIntensity);
    }
    
    // Apply exposure
    color *= exposure;
    
    // Apply tonemapping
    switch (tonemapper) {
        case 0: color = TonemapNone(color); break;
        case 1: color = TonemapACES(color); break;
        case 2: color = TonemapReinhard(color); break;
        case 3: color = TonemapFilmic(color); break;
        case 4: color = TonemapCustom(color); break;
    }
    
    // Apply color adjustments
    color = AdjustContrast(color, contrast);
    color = AdjustBrightness(color, brightness);
    color = AdjustSaturation(color, saturation);
    
    // Apply color grading
    if (useColorGrading > 0) {
        color = ApplyColorGrading(color, colorGrading, colorGradingSampler);
    }
    
    // Apply vignette
    if (useVignette > 0) {
        color = ApplyVignette(color, input.texCoord, vignetteIntensity, vignetteRadius);
    }
    
    // Apply film grain
    color = ApplyFilmGrain(color, input.texCoord, time);
    
    // Apply dithering
    if (useDithering > 0) {
        color = ApplyDithering(color, input.texCoord, time);
    }
    
    // Apply gamma correction
    color = pow(color, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));
    
    // Output final color
    output.color = float4(color, 1.0);
    
    return output;
}
