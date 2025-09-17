// Skybox Fragment Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Skybox fragment shader using SLANG

struct PSInput {
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 texCoord : TEXCOORD1;
    uint materialID : TEXCOORD2;
};

struct UniformBufferObject {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

struct PushConstants {
    float4x4 model;
    float intensity;
    int useHDR;
    int rotationEnabled;
};

// Texture samplers
TextureCube environmentMap : register(t0);
TextureCube irradianceMap : register(t1);  // For diffuse IBL
TextureCube prefilterMap : register(t2);  // For specular IBL
Texture2D brdfLUT : register(t3);          // For BRDF lookup
SamplerState linearSampler : register(s0);

// Constants
static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;

// Skybox modes
static const int SKYBOX_MODE_STATIC = 0;
static const int SKYBOX_MODE_DYNAMIC = 1;
static const int SKYBOX_MODE_PROCEDURAL = 2;

// Helper functions
float3 SRGBToLinear(float3 srgb)
{
    return pow(srgb, 2.2);
}

float3 LinearToSRGB(float3 linear)
{
    return pow(linear, 1.0 / 2.2);
}

float SimplexNoise2D(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float FractionalBrownianMotion(float2 uv, int octaves, float lacunarity, float gain)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * SimplexNoise2D(uv * frequency);
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return value;
}

// Procedural sky generation
float3 CalculateProceduralSky(float3 viewDir, float time)
{
    float3 skyColor = float3(0.0, 0.0, 0.0);
    
    // Simple day/night cycle based on time
    float dayTime = frac(time * 0.1 / TWO_PI) * TWO_PI;
    float sunHeight = sin(dayTime);
    
    // Calculate sun position
    float3 sunDir = normalize(float3(cos(dayTime), sunHeight, sin(dayTime)));
    
    // Sky gradient
    float horizonBlend = smoothstep(-0.1, 0.1, viewDir.y);
    
    // Day colors
    float3 dayTop = float3(0.3, 0.6, 1.0);    // Light blue
    float3 dayHorizon = float3(0.8, 0.9, 1.0); // Light cyan
    
    // Night colors
    float3 nightTop = float3(0.0, 0.0, 0.1);    // Dark blue
    float3 nightHorizon = float3(0.1, 0.1, 0.2); // Dark gray
    
    // Interpolate between day and night
    float dayFactor = max(sunHeight, 0.0);
    float3 topColor = lerp(nightTop, dayTop, dayFactor);
    float3 horizonColor = lerp(nightHorizon, dayHorizon, dayFactor);
    
    // Create gradient
    skyColor = lerp(horizonColor, topColor, horizonBlend);
    
    // Add sun
    float sunDot = max(dot(viewDir, sunDir), 0.0);
    float sun = pow(sunDot, 256.0) * 10.0;
    skyColor += float3(1.0, 0.9, 0.7) * sun * dayFactor;
    
    // Add stars at night
    if (dayFactor < 0.3) {
        float starNoise = SimplexNoise2D(viewDir.xz * 100.0);
        float stars = step(0.99, starNoise) * (1.0 - dayFactor) * 0.3;
        skyColor += float3(stars, stars, stars);
    }
    
    // Add clouds
    float cloudNoise = FractionalBrownianMotion(viewDir.xz * 2.0 + time * 0.05, 4, 2.0, 0.5);
    float clouds = smoothstep(0.3, 0.7, cloudNoise) * dayFactor * 0.5;
    skyColor = lerp(skyColor, float3(1.0, 1.0, 1.0), clouds);
    
    return skyColor;
}

// Atmospheric scattering approximation
float3 CalculateAtmosphericScattering(float3 viewDir, float3 sunDir, float intensity)
{
    // Rayleigh scattering (blue sky)
    float rayleigh = 0.5 + 0.5 * dot(viewDir, float3(0.0, 1.0, 0.0));
    float3 rayleighColor = float3(0.3, 0.6, 1.0) * rayleigh;
    
    // Mie scattering (sun halo)
    float mie = pow(max(dot(viewDir, sunDir), 0.0), 8.0);
    float3 mieColor = float3(1.0, 0.9, 0.7) * mie;
    
    // Combine scattering
    float3 scattering = rayleighColor + mieColor * intensity;
    
    return scattering;
}

// Tonemapping functions
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

[shader("pixel")]
float4 main(PSInput input,
            UniformBufferObject ubo : register(b0),
            PushConstants pushConstants : register(b1)) : SV_Target
{
    float3 viewDir = normalize(input.texCoord);
    float3 finalColor = float3(0.0, 0.0, 0.0);
    
    // Sample environment map
    float3 envColor = environmentMap.Sample(linearSampler, viewDir).rgb;
    
    // Apply HDR to LDR conversion if needed
    if (pushConstants.useHDR > 0) {
        // Tonemap HDR color
        envColor = TonemapACES(envColor);
        envColor = LinearToSRGB(envColor);
    } else {
        // Convert from linear to sRGB
        envColor = SRGBToLinear(envColor);
    }
    
    // Apply intensity
    envColor *= pushConstants.intensity;
    
    // For dynamic skybox, blend with procedural sky
    int skyboxMode = SKYBOX_MODE_STATIC; // This could be a uniform
    if (skyboxMode == SKYBOX_MODE_DYNAMIC) {
        float3 proceduralSky = CalculateProceduralSky(viewDir, ubo.time);
        float blendFactor = sin(ubo.time * 0.5) * 0.5 + 0.5;
        finalColor = lerp(envColor, proceduralSky, blendFactor);
    } else if (skyboxMode == SKYBOX_MODE_PROCEDURAL) {
        finalColor = CalculateProceduralSky(viewDir, ubo.time);
    } else {
        finalColor = envColor;
    }
    
    // Add atmospheric scattering
    float3 sunDir = normalize(float3(1.0, 1.0, 0.0)); // Fixed sun direction
    float3 scattering = CalculateAtmosphericScattering(viewDir, sunDir, pushConstants.intensity);
    finalColor += scattering * 0.1;
    
    // Apply gamma correction
    finalColor = LinearToSRGB(finalColor);
    
    // Output final color
    return float4(finalColor, 1.0);
}
