// Common shader utilities and constants
// Author: inkbytefo
// Project: AstralEngine

#ifndef COMMON_GLSL
#define COMMON_GLSL

// Precision qualifiers
precision highp float;
precision highp int;

// Constants
const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float HALF_PI = 1.57079632679;
const float INV_PI = 0.31830988618;
const float INV_TWO_PI = 0.15915494309;
const float EPSILON = 0.0001;

// Gamma correction constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

// Maximum lights
const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_POINT_LIGHTS = 16;
const int MAX_SPOT_LIGHTS = 8;

// Shadow mapping constants
const int MAX_SHADOW_CASCADES = 4;
const float SHADOW_BIAS = 0.00001;

// Texture sampling utilities
vec4 SampleTexture(sampler2D tex, vec2 uv)
{
    return texture(tex, uv);
}

vec4 SampleTextureLod(sampler2D tex, vec2 uv, float lod)
{
    return textureLod(tex, uv, lod);
}

vec4 SampleTextureGrad(sampler2D tex, vec2 uv, vec2 dPdx, vec2 dPdy)
{
    return textureGrad(tex, uv, dPdx, dPdy);
}

// Color space conversions
vec3 LinearToSRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

vec3 SRGBToLinear(vec3 color)
{
    return pow(color, vec3(GAMMA));
}

vec4 LinearToSRGB(vec4 color)
{
    return vec4(LinearToSRGB(color.rgb), color.a);
}

vec4 SRGBToLinear(vec4 color)
{
    return vec4(SRGBToLinear(color.rgb), color.a);
}

// Vector utilities
float LengthSquared(vec3 v)
{
    return dot(v, v);
}

float LengthSquared(vec2 v)
{
    return dot(v, v);
}

vec3 NormalizeSafe(vec3 v)
{
    float len = length(v);
    return len > EPSILON ? v / len : vec3(0.0);
}

vec2 NormalizeSafe(vec2 v)
{
    float len = length(v);
    return len > EPSILON ? v / len : vec2(0.0);
}

// Matrix utilities
mat3 CreateNormalMatrix(mat4 modelMatrix)
{
    return transpose(inverse(mat3(modelMatrix)));
}

// Interpolation utilities
float SmoothStep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float SmootherStep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Range mapping
float MapRange(float value, float from1, float to1, float from2, float to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

vec2 MapRange(vec2 value, vec2 from1, vec2 to1, vec2 from2, vec2 to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

vec3 MapRange(vec3 value, vec3 from1, vec3 to1, vec3 from2, vec3 to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

// Depth utilities
float LinearizeDepth(float depth, float near, float far)
{
    return (2.0 * near * far) / (far + near - depth * (far - near));
}

float ViewSpaceDepth(vec4 clipPos)
{
    return clipPos.z / clipPos.w;
}

// Screen space utilities
vec2 ScreenSpaceToUV(vec2 screenPos, vec2 screenSize)
{
    return screenPos / screenSize;
}

vec2 UVToScreenSpace(vec2 uv, vec2 screenSize)
{
    return uv * screenSize;
}

// Random number generation
float Random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float Random(vec3 st)
{
    return fract(sin(dot(st.xyz, vec3(12.9898, 78.233, 54.53))) * 43758.5453123);
}

vec2 Random2(vec2 st)
{
    return vec2(Random(st), Random(st + 0.5));
}

vec3 Random3(vec3 st)
{
    return vec3(Random(st), Random(st + 0.3), Random(st + 0.7));
}

// Noise functions
float Noise(vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);
    
    float a = Random(i);
    float b = Random(i + vec2(1.0, 0.0));
    float c = Random(i + vec2(0.0, 1.0));
    float d = Random(i + vec2(1.0, 1.0));
    
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// Debug utilities
vec3 DebugColor(float value)
{
    if (value < 0.25)
        return vec3(0.0, value * 4.0, 1.0);
    else if (value < 0.5)
        return vec3(0.0, 1.0, 1.0 - (value - 0.25) * 4.0);
    else if (value < 0.75)
        return vec3((value - 0.5) * 4.0, 1.0, 0.0);
    else
        return vec3(1.0, 1.0 - (value - 0.75) * 4.0, 0.0);
}

vec3 DebugColor(vec3 value)
{
    return normalize(value) * 0.5 + 0.5;
}

// Simplex Noise 2D (integrated from noise.glsl to avoid circular dependency)
vec2 Mod289(vec2 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 Mod289(vec3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 Mod289(vec4 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 Permute289(vec3 x)
{
    return Mod289(((x * 34.0) + 1.0) * x);
}

vec4 Permute289(vec4 x)
{
    return Mod289(((x * 34.0) + 1.0) * x);
}

vec2 Fade2D(vec2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float SimplexNoise2D(vec2 P)
{
    // Skew the input space to determine which simplex cell we're in
    const float F2 = 0.5 * (sqrt(3.0) - 1.0);
    vec2 s = vec2((P.x + P.y) * F2);
    
    // Unskew the cell origin back to (x,y) space
    const float G2 = (3.0 - sqrt(3.0)) / 6.0;
    vec2 i = floor(P + s);
    vec2 t = vec2((i.x + i.y) * G2);
    
    // Work out the hashed gradient indices of the three simplex corners
    vec2 p0 = i - t;
    vec2 p1 = p0 + vec2(G2, 1.0 - G2);
    vec2 p2 = p0 + vec2(1.0 - G2, G2);
    
    // Calculate the distance from the point to each corner
    vec2 d0 = P - p0;
    vec2 d1 = P - p1;
    vec2 d2 = P - p2;
    
    // Hash the gradient indices
    vec3 m = max(0.5 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2)), 0.0);
    m = m * m;
    m = m * m;
    
    // Calculate the gradients
    vec3 x = 2.0 * fract(p0.xxx * vec3(303.8, 303.8, 303.8)) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    
    // Calculate the final noise value
    vec3 g = vec3(dot(d0, vec2(a0.x, h.x)), dot(d1, vec2(a0.y, h.y)), dot(d2, vec2(a0.z, h.z)));
    return 130.0 * dot(m, g);
}

#endif // COMMON_GLSL
