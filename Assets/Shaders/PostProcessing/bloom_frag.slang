// Bloom Fragment Shader
// Author: inkbytefo
// Project: AstralEngine

// Constants
static const float PI = 3.14159265359;

// Inputs from vertex shader
struct VertexOutput {
    [[vk::location(0)]] float2 texCoord : TEXCOORD0;
    [[vk::location(1)]] float2 texCoords[5] : TEXCOORD1; // For sampling in bloom shader
    float4 position : SV_POSITION;
};

// Output
struct FragmentOutput {
    [[vk::location(0)]] float4 color : SV_TARGET0;
};

// Texture samplers
Texture2D sceneColor : register(t0);
SamplerState sceneColorSampler : register(s0);

Texture2D brightPass : register(t1);
SamplerState brightPassSampler : register(s1);

Texture2D blurTexture : register(t2);
SamplerState blurTextureSampler : register(s2);

// Uniform buffer objects
cbuffer UniformBufferObject : register(b0) {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

cbuffer BloomUBO : register(b1) {
    float threshold;           // Bright pass threshold
    float knee;               // Soft knee threshold
    float intensity;          // Bloom intensity
    float radius;             // Blur radius
    int quality;             // 0: low, 1: medium, 2: high
    int useDirt;            // Use lens dirt
    float dirtIntensity;     // Lens dirt intensity
    float2 padding;
};

// Push constants
[[vk::push_constant]] cbuffer PushConstants {
    float2 texelSize;
    int bloomPass; // 0: bright pass, 1: horizontal blur, 2: vertical blur, 3: composite
};

// Simplex Noise 2D implementation
float3 Mod289(float3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float2 Mod289(float2 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float3 Permute289(float3 x)
{
    return Mod289(((x * 34.0) + 1.0) * x);
}

float2 Fade2D(float2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float SimplexNoise2D(float2 P)
{
    // Skew the input space to determine which simplex cell we're in
    const float F2 = 0.5 * (sqrt(3.0) - 1.0);
    float2 s = float2((P.x + P.y) * F2);
    
    // Unskew the cell origin back to (x,y) space
    const float G2 = (3.0 - sqrt(3.0)) / 6.0;
    float2 i = floor(P + s);
    float2 t = float2((i.x + i.y) * G2);
    
    // Work out the hashed gradient indices of the three simplex corners
    float2 p0 = i - t;
    float2 p1 = p0 + float2(G2, 1.0 - G2);
    float2 p2 = p0 + float2(1.0 - G2, G2);
    
    // Calculate the distance from the point to each corner
    float2 d0 = P - p0;
    float2 d1 = P - p1;
    float2 d2 = P - p2;
    
    // Hash the gradient indices
    float3 m = max(0.5 - float3(dot(d0, d0), dot(d1, d1), dot(d2, d2)), float3(0.0));
    m = m * m;
    m = m * m;
    
    // Calculate the gradients
    float3 x = 2.0 * frac(p0.xxx * float3(303.8, 303.8, 303.8)) - 1.0;
    float3 h = abs(x) - 0.5;
    float3 ox = floor(x + 0.5);
    float3 a0 = x - ox;
    
    // Calculate the final noise value
    float3 g = float3(dot(d0, float2(a0.x, h.x)), dot(d1, float2(a0.y, h.y)), dot(d2, float2(a0.z, h.z)));
    return 130.0 * dot(m, g);
}

// Bright pass filter - extracts bright areas for bloom
float3 BrightPass(float3 color, float threshold, float knee)
{
    // Calculate luminance
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    
    // Soft knee function for smooth transition
    float kneeSquared = knee * knee;
    float softKnee = 2.0 * knee - threshold;
    float bright = max(luminance - threshold, 0.0);
    bright = max(bright, luminance - kneeSquared / (luminance - softKnee));
    
    // Scale color by brightness factor
    float scale = bright / max(luminance, 0.0001);
    return color * scale;
}

// Gaussian blur weights
float Gaussian(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * PI) * sigma);
}

// 13-tap Gaussian blur
float3 GaussianBlur(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float radius)
{
    float3 color = float3(0.0);
    float totalWeight = 0.0;
    
    // Calculate sigma based on radius
    float sigma = radius * 0.5;
    
    // 13-tap blur
    const int taps = 13;
    float offsets[13] = { -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    
    for (int i = 0; i < taps; i++) {
        float weight = Gaussian(offsets[i], sigma);
        float2 offset = float2(offsets[i] * texelSize.x, 0.0); // Horizontal blur
        
        if (bloomPass == 2) { // Vertical blur
            offset = float2(0.0, offsets[i] * texelSize.y);
        }
        
        color += tex.Sample(samp, uv + offset).rgb * weight;
        totalWeight += weight;
    }
    
    return color / totalWeight;
}

// 5-tap Kawase blur (faster, good for bloom)
float3 KawaseBlur(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float radius)
{
    float3 color = float3(0.0);
    float offset = radius * texelSize.x;
    
    if (bloomPass == 2) { // Vertical blur
        offset = radius * texelSize.y;
    }
    
    // 5-tap cross pattern
    color += tex.Sample(samp, uv).rgb * 0.4;
    color += tex.Sample(samp, uv + float2(offset, 0.0)).rgb * 0.15;
    color += tex.Sample(samp, uv + float2(-offset, 0.0)).rgb * 0.15;
    color += tex.Sample(samp, uv + float2(0.0, offset)).rgb * 0.15;
    color += tex.Sample(samp, uv + float2(0.0, -offset)).rgb * 0.15;
    
    return color;
}

// Dual Kawase blur (even faster, good for mobile)
float3 DualKawaseBlur(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float radius)
{
    float3 color = float3(0.0);
    float offset = radius * texelSize.x;
    
    if (bloomPass == 2) { // Vertical blur
        offset = radius * texelSize.y;
    }
    
    // Dual Kawase pattern
    color += tex.Sample(samp, uv + float2(-offset, -offset)).rgb * 0.25;
    color += tex.Sample(samp, uv + float2(offset, -offset)).rgb * 0.25;
    color += tex.Sample(samp, uv + float2(-offset, offset)).rgb * 0.25;
    color += tex.Sample(samp, uv + float2(offset, offset)).rgb * 0.25;
    
    return color;
}

// Composite bloom with original scene
float3 CompositeBloom(float3 sceneColor, float3 bloomColor, float intensity)
{
    return sceneColor + bloomColor * intensity;
}

// Anamorphic flare effect
float3 AnamorphicFlare(float3 color, float2 uv, float time)
{
    float2 center = float2(0.5, 0.5);
    float2 dir = normalize(uv - center);
    
    // Create horizontal streaks
    float streak = pow(abs(dir.x), 8.0) * 0.1;
    
    // Add some noise for realism
    float noise = SimplexNoise2D(uv * 50.0 + time * 0.1);
    streak *= (0.5 + noise * 0.5);
    
    // Color the flare
    float3 flareColor = float3(1.0, 0.9, 0.7) * streak;
    
    return color + flareColor;
}

[shader("fragment")]
FragmentOutput main(VertexOutput input) {
    FragmentOutput output;
    
    float3 color = float3(0.0);
    
    switch (bloomPass) {
        case 0: // Bright pass
        {
            // Sample scene color
            float3 sceneColor = sceneColor.Sample(sceneColorSampler, input.texCoord).rgb;
            
            // Apply bright pass filter
            color = BrightPass(sceneColor, threshold, knee);
            
            // Add some anamorphic flare to bright areas
            float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
            if (luminance > threshold) {
                color = AnamorphicFlare(color, input.texCoord, time);
            }
            break;
        }
        
        case 1: // Horizontal blur
        case 2: // Vertical blur
        {
            // Choose blur quality
            if (quality == 0) {
                // Low quality - Dual Kawase
                color = DualKawaseBlur(sceneColor, sceneColorSampler, input.texCoord, texelSize, radius);
            } else if (quality == 1) {
                // Medium quality - Kawase
                color = KawaseBlur(sceneColor, sceneColorSampler, input.texCoord, texelSize, radius);
            } else {
                // High quality - Gaussian
                color = GaussianBlur(sceneColor, sceneColorSampler, input.texCoord, texelSize, radius);
            }
            break;
        }
        
        case 3: // Composite
        {
            // Sample original scene
            float3 sceneColor = sceneColor.Sample(sceneColorSampler, input.texCoord).rgb;
            
            // Sample blurred bloom texture
            float3 bloomColor = blurTexture.Sample(blurTextureSampler, input.texCoord).rgb;
            
            // Composite bloom
            color = CompositeBloom(sceneColor, bloomColor, intensity);
            
            // Add lens dirt if enabled
            if (useDirt > 0) {
                float3 dirt = brightPass.Sample(brightPassSampler, input.texCoord).rgb; // Reuse bright pass for dirt
                color += dirt * dirtIntensity;
            }
            break;
        }
    }
    
    // Output final color
    output.color = float4(color, 1.0);
    
    return output;
}
