// Common shader utilities and constants
// Author: inkbytefo
// Project: AstralEngine
// Description: Common utilities and constants for SLANG shaders

// Constants
static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;
static const float HALF_PI = 1.57079632679;
static const float INV_PI = 0.31830988618;
static const float INV_TWO_PI = 0.15915494309;
static const float EPSILON = 0.0001;

// Gamma correction constants
static const float GAMMA = 2.2;
static const float INV_GAMMA = 1.0 / GAMMA;

// Maximum lights
static const int MAX_DIRECTIONAL_LIGHTS = 4;
static const int MAX_POINT_LIGHTS = 16;
static const int MAX_SPOT_LIGHTS = 8;

// Shadow mapping constants
static const int MAX_SHADOW_CASCADES = 4;
static const float SHADOW_BIAS = 0.00001;

// Texture sampling utilities
float4 SampleTexture(Texture2D tex, SamplerState samp, float2 uv)
{
    return tex.Sample(samp, uv);
}

float4 SampleTextureLod(Texture2D tex, SamplerState samp, float2 uv, float lod)
{
    return tex.SampleLevel(samp, uv, lod);
}

float4 SampleTextureGrad(Texture2D tex, SamplerState samp, float2 uv, float2 dPdx, float2 dPdy)
{
    return tex.SampleGrad(samp, uv, dPdx, dPdy);
}

// Color space conversions
float3 LinearToSRGB(float3 color)
{
    return pow(color, float3(INV_GAMMA, INV_GAMMA, INV_GAMMA));
}

float3 SRGBToLinear(float3 color)
{
    return pow(color, float3(GAMMA, GAMMA, GAMMA));
}

float4 LinearToSRGB(float4 color)
{
    return float4(LinearToSRGB(color.rgb), color.a);
}

float4 SRGBToLinear(float4 color)
{
    return float4(SRGBToLinear(color.rgb), color.a);
}

// Vector utilities
float LengthSquared(float3 v)
{
    return dot(v, v);
}

float LengthSquared(float2 v)
{
    return dot(v, v);
}

float3 NormalizeSafe(float3 v)
{
    float len = length(v);
    return len > EPSILON ? v / len : float3(0.0, 0.0, 0.0);
}

float2 NormalizeSafe(float2 v)
{
    float len = length(v);
    return len > EPSILON ? v / len : float2(0.0, 0.0);
}

// Matrix utilities
float3x3 CreateNormalMatrix(float4x4 modelMatrix)
{
    return transpose(inverse(float3x3(modelMatrix[0].xyz, modelMatrix[1].xyz, modelMatrix[2].xyz)));
}

// Interpolation utilities
float SmoothStep(float edge0, float edge1, float x)
{
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

float SmootherStep(float edge0, float edge1, float x)
{
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Range mapping
float MapRange(float value, float from1, float to1, float from2, float to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

float2 MapRange(float2 value, float2 from1, float2 to1, float2 from2, float2 to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

float3 MapRange(float3 value, float3 from1, float3 to1, float3 from2, float3 to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

// Depth utilities
float LinearizeDepth(float depth, float near, float far)
{
    return (2.0 * near * far) / (far + near - depth * (far - near));
}

float ViewSpaceDepth(float4 clipPos)
{
    return clipPos.z / clipPos.w;
}

// Screen space utilities
float2 ScreenSpaceToUV(float2 screenPos, float2 screenSize)
{
    return screenPos / screenSize;
}

float2 UVToScreenSpace(float2 uv, float2 screenSize)
{
    return uv * screenSize;
}

// Random number generation
float Random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

float Random(float3 st)
{
    return frac(sin(dot(st.xyz, float3(12.9898, 78.233, 54.53))) * 43758.5453123);
}

float2 Random2(float2 st)
{
    return float2(Random(st), Random(st + 0.5));
}

float3 Random3(float3 st)
{
    return float3(Random(st), Random(st + 0.3), Random(st + 0.7));
}

// Noise functions
float Noise(float2 st)
{
    float2 i = floor(st);
    float2 f = fract(st);
    
    float a = Random(i);
    float b = Random(i + float2(1.0, 0.0));
    float c = Random(i + float2(0.0, 1.0));
    float d = Random(i + float2(1.0, 1.0));
    
    float2 u = f * f * (3.0 - 2.0 * f);
    
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Debug utilities
float3 DebugColor(float value)
{
    if (value < 0.25)
        return float3(0.0, value * 4.0, 1.0);
    else if (value < 0.5)
        return float3(0.0, 1.0, 1.0 - (value - 0.25) * 4.0);
    else if (value < 0.75)
        return float3((value - 0.5) * 4.0, 1.0, 0.0);
    else
        return float3(1.0, 1.0 - (value - 0.75) * 4.0, 0.0);
}

float3 DebugColor(float3 value)
{
    return normalize(value) * 0.5 + 0.5;
}

// Simplex Noise 2D
float2 Mod289(float2 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float3 Mod289(float3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 Mod289(float4 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float3 Permute289(float3 x)
{
    return Mod289(((x * 34.0) + 1.0) * x);
}

float4 Permute289(float4 x)
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
    float3 m = max(0.5 - float3(dot(d0, d0), dot(d1, d1), dot(d2, d2)), 0.0);
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

// Fractional Brownian Motion
float FractionalBrownianMotion(float2 st, int octaves, float lacunarity, float gain)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * Noise(st * frequency);
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return value;
}

// Utility functions for SLANG compatibility
float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float2 saturate(float2 x)
{
    return clamp(x, 0.0, 1.0);
}

float3 saturate(float3 x)
{
    return clamp(x, 0.0, 1.0);
}

float4 saturate(float4 x)
{
    return clamp(x, 0.0, 1.0);
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float2 lerp(float2 a, float2 b, float t)
{
    return a + (b - a) * t;
}

float3 lerp(float3 a, float3 b, float t)
{
    return a + (b - a) * t;
}

float4 lerp(float4 a, float4 b, float t)
{
    return a + (b - a) * t;
}
