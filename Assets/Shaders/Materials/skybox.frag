#version 450

// Skybox Fragment Shader
// Author: inkbytefo
// Project: AstralEngine



#include "../Common/common.glsl"

// Inputs from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in flat int inMaterialID;

// Output
layout(location = 0) out vec4 outColor;

// Texture samplers
layout(binding = 0) uniform samplerCube environmentMap;
layout(binding = 1) uniform samplerCube irradianceMap;  // For diffuse IBL
layout(binding = 2) uniform samplerCube prefilterMap;  // For specular IBL
layout(binding = 3) uniform sampler2D brdfLUT;          // For BRDF lookup

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    float intensity;
    int useHDR;
    int rotationEnabled;
} pushConstants;

// Skybox modes
const int SKYBOX_MODE_STATIC = 0;
const int SKYBOX_MODE_DYNAMIC = 1;
const int SKYBOX_MODE_PROCEDURAL = 2;

// Procedural sky generation
vec3 CalculateProceduralSky(vec3 viewDir, float time)
{
    vec3 skyColor = vec3(0.0);
    
    // Simple day/night cycle based on time
    float dayTime = mod(time * 0.1, TWO_PI);
    float sunHeight = sin(dayTime);
    
    // Calculate sun position
    vec3 sunDir = normalize(vec3(cos(dayTime), sunHeight, sin(dayTime)));
    
    // Sky gradient
    float horizonBlend = smoothstep(-0.1, 0.1, viewDir.y);
    
    // Day colors
    vec3 dayTop = vec3(0.3, 0.6, 1.0);    // Light blue
    vec3 dayHorizon = vec3(0.8, 0.9, 1.0); // Light cyan
    
    // Night colors
    vec3 nightTop = vec3(0.0, 0.0, 0.1);    // Dark blue
    vec3 nightHorizon = vec3(0.1, 0.1, 0.2); // Dark gray
    
    // Interpolate between day and night
    float dayFactor = max(sunHeight, 0.0);
    vec3 topColor = mix(nightTop, dayTop, dayFactor);
    vec3 horizonColor = mix(nightHorizon, dayHorizon, dayFactor);
    
    // Create gradient
    skyColor = mix(horizonColor, topColor, horizonBlend);
    
    // Add sun
    float sunDot = max(dot(viewDir, sunDir), 0.0);
    float sun = pow(sunDot, 256.0) * 10.0;
    skyColor += vec3(1.0, 0.9, 0.7) * sun * dayFactor;
    
    // Add stars at night
    if (dayFactor < 0.3) {
        float starNoise = SimplexNoise2D(viewDir.xz * 100.0);
        float stars = step(0.99, starNoise) * (1.0 - dayFactor) * 0.3;
        skyColor += vec3(stars);
    }
    
    // Add clouds
    float cloudNoise = FractionalBrownianMotion(viewDir.xz * 2.0 + time * 0.05, 4, 2.0, 0.5);
    float clouds = smoothstep(0.3, 0.7, cloudNoise) * dayFactor * 0.5;
    skyColor = mix(skyColor, vec3(1.0), clouds);
    
    return skyColor;
}

// Atmospheric scattering approximation
vec3 CalculateAtmosphericScattering(vec3 viewDir, vec3 sunDir, float intensity)
{
    // Rayleigh scattering (blue sky)
    float rayleigh = 0.5 + 0.5 * dot(viewDir, vec3(0.0, 1.0, 0.0));
    vec3 rayleighColor = vec3(0.3, 0.6, 1.0) * rayleigh;
    
    // Mie scattering (sun halo)
    float mie = pow(max(dot(viewDir, sunDir), 0.0), 8.0);
    vec3 mieColor = vec3(1.0, 0.9, 0.7) * mie;
    
    // Combine scattering
    vec3 scattering = rayleighColor + mieColor * intensity;
    
    return scattering;
}

// Tonemapping functions
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

void main() {
    vec3 viewDir = normalize(inTexCoord);
    vec3 finalColor = vec3(0.0);
    
    // Sample environment map
    vec3 envColor = texture(environmentMap, viewDir).rgb;
    
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
        vec3 proceduralSky = CalculateProceduralSky(viewDir, ubo.time);
        float blendFactor = sin(ubo.time * 0.5) * 0.5 + 0.5;
        finalColor = mix(envColor, proceduralSky, blendFactor);
    } else if (skyboxMode == SKYBOX_MODE_PROCEDURAL) {
        finalColor = CalculateProceduralSky(viewDir, ubo.time);
    } else {
        finalColor = envColor;
    }
    
    // Add atmospheric scattering
    vec3 sunDir = normalize(vec3(1.0, 1.0, 0.0)); // Fixed sun direction
    vec3 scattering = CalculateAtmosphericScattering(viewDir, sunDir, pushConstants.intensity);
    finalColor += scattering * 0.1;
    
    // Apply gamma correction
    finalColor = LinearToSRGB(finalColor);
    
    // Output final color
    outColor = vec4(finalColor, 1.0);
}
