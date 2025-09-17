#version 450

// Tonemapping Fragment Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Common/common.glsl"

// Inputs from vertex shader
layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inViewRay;

// Output
layout(location = 0) out vec4 outColor;

// Texture samplers
layout(binding = 0) uniform sampler2D sceneColor;    // Main scene color
layout(binding = 1) uniform sampler2D sceneDepth;    // Scene depth buffer
layout(binding = 2) uniform sampler2D bloomTexture;  // Bloom texture
layout(binding = 3) uniform sampler2D lensDirt;      // Lens dirt texture
layout(binding = 4) uniform sampler2D colorGrading;  // Color grading LUT

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

layout(binding = 1) uniform PostProcessingUBO {
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
    vec2 padding;
} ppSettings;

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    int useBloom;
    int useVignette;
    int useChromaticAberration;
} pushConstants;

// Tonemapping operators
vec3 TonemapNone(vec3 color)
{
    return color;
}

vec3 TonemapACES(vec3 color)
{
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

vec3 TonemapReinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 TonemapFilmic(vec3 color)
{
    color = max(vec3(0.0), color - vec3(0.004));
    return (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
}

vec3 TonemapCustom(vec3 color)
{
    // Custom tonemapping curve
    float shoulder = 0.9;
    float midtones = 0.5;
    float highlights = 0.1;
    
    color = color * ppSettings.exposure;
    
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float tonedLum = lum / (lum + shoulder) * midtones + highlights * lum;
    
    return color * (tonedLum / max(lum, 0.0001));
}

// Color adjustments
vec3 AdjustContrast(vec3 color, float contrast)
{
    return ((color - 0.5) * max(contrast, 0.0)) + 0.5;
}

vec3 AdjustBrightness(vec3 color, float brightness)
{
    return color + brightness;
}

vec3 AdjustSaturation(vec3 color, float saturation)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luminance), color, max(saturation, 0.0));
}

// Vignette effect
vec3 ApplyVignette(vec3 color, vec2 uv, float intensity, float radius)
{
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center);
    float vignette = smoothstep(radius, radius - 0.1, dist);
    vignette = mix(1.0, vignette, intensity);
    return color * vignette;
}

// Chromatic aberration
vec3 ApplyChromaticAberration(sampler2D tex, vec2 uv, float intensity)
{
    vec2 center = vec2(0.5, 0.5);
    vec2 dir = uv - center;
    float dist = length(dir);
    
    // Sample red channel slightly offset
    vec2 redUV = uv + dir * intensity * 0.01;
    vec3 color = vec3(
        texture(tex, redUV).r,
        texture(tex, uv).g,
        texture(tex, uv - dir * intensity * 0.01).b
    );
    
    return color;
}

// Bloom application
vec3 ApplyBloom(vec3 color, sampler2D bloomTex, vec2 uv, float intensity)
{
    vec3 bloom = texture(bloomTex, uv).rgb;
    return color + bloom * intensity;
}

// Lens dirt
vec3 ApplyLensDirt(vec3 color, sampler2D dirtTex, vec2 uv, float intensity)
{
    vec3 dirt = texture(dirtTex, uv).rgb;
    return color + dirt * intensity;
}

// Color grading using LUT
vec3 ApplyColorGrading(vec3 color, sampler2D lutTex)
{
    // Assuming LUT is 32x32x32 or similar
    float blue = color.b * 31.0;
    vec2 lutUV;
    
    lutUV.y = floor(blue) / 32.0;
    lutUV.x = (color.r * 31.0 + floor(blue)) / 1024.0;
    
    vec3 gradedColor = texture(lutTex, lutUV).rgb;
    
    // Interpolate with next slice
    lutUV.y = ceil(blue) / 32.0;
    lutUV.x = (color.r * 31.0 + ceil(blue)) / 1024.0;
    
    vec3 gradedColor2 = texture(lutTex, lutUV).rgb;
    float t = fract(blue);
    
    return mix(gradedColor, gradedColor2, t);
}

// Dithering
vec3 ApplyDithering(vec3 color, vec2 uv, float time)
{
    // Bayer matrix dithering
    float bayer8x8[64] = float[](
        0.0, 32.0, 8.0, 40.0, 2.0, 34.0, 10.0, 42.0,
        48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
        12.0, 44.0, 4.0, 36.0, 14.0, 46.0, 6.0, 38.0,
        60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
        3.0, 35.0, 11.0, 43.0, 1.0, 33.0, 9.0, 41.0,
        51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
        15.0, 47.0, 7.0, 39.0, 13.0, 45.0, 5.0, 37.0,
        63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
    );
    
    ivec2 pixelCoord = ivec2(uv * pushConstants.texelSize);
    int index = (pixelCoord.x % 8) + (pixelCoord.y % 8) * 8;
    float dither = (bayer8x8[index] / 64.0 - 0.5) / 255.0;
    
    return color + dither;
}

// Film grain
vec3 ApplyFilmGrain(vec3 color, vec2 uv, float time)
{
    float grain = Random(uv + time * 0.01) * 0.1;
    return color + grain;
}

void main() {
    // Sample scene color
    vec3 color = texture(sceneColor, inTexCoord).rgb;
    
    // Apply chromatic aberration
    if (pushConstants.useChromaticAberration > 0) {
        color = ApplyChromaticAberration(sceneColor, inTexCoord, ppSettings.chromaticAberrationIntensity);
    }
    
    // Apply bloom
    if (pushConstants.useBloom > 0) {
        color = ApplyBloom(color, bloomTexture, inTexCoord, ppSettings.bloomIntensity);
        
        // Apply lens dirt if bloom is enabled
        color = ApplyLensDirt(color, lensDirt, inTexCoord, ppSettings.lensDirtIntensity);
    }
    
    // Apply exposure
    color *= ppSettings.exposure;
    
    // Apply tonemapping
    switch (ppSettings.tonemapper) {
        case 0: color = TonemapNone(color); break;
        case 1: color = TonemapACES(color); break;
        case 2: color = TonemapReinhard(color); break;
        case 3: color = TonemapFilmic(color); break;
        case 4: color = TonemapCustom(color); break;
    }
    
    // Apply color adjustments
    color = AdjustContrast(color, ppSettings.contrast);
    color = AdjustBrightness(color, ppSettings.brightness);
    color = AdjustSaturation(color, ppSettings.saturation);
    
    // Apply color grading
    if (ppSettings.useColorGrading > 0) {
        color = ApplyColorGrading(color, colorGrading);
    }
    
    // Apply vignette
    if (pushConstants.useVignette > 0) {
        color = ApplyVignette(color, inTexCoord, ppSettings.vignetteIntensity, ppSettings.vignetteRadius);
    }
    
    // Apply film grain
    color = ApplyFilmGrain(color, inTexCoord, ubo.time);
    
    // Apply dithering
    if (ppSettings.useDithering > 0) {
        color = ApplyDithering(color, inTexCoord, ubo.time);
    }
    
    // Apply gamma correction
    color = pow(color, vec3(1.0 / ppSettings.gamma));
    
    // Output final color
    outColor = vec4(color, 1.0);
}
