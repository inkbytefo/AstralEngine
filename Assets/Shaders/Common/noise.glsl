// Noise generation and procedural texture functions
// Author: inkbytefo
// Project: AstralEngine

#ifndef NOISE_GLSL
#define NOISE_GLSL

// Classic Perlin noise
vec2 Fade(vec2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

vec4 Permute(vec4 x)
{
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

vec4 TaylorInvSqrt(vec4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float ClassicPerlinNoise(vec2 P)
{
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, 289.0); // To avoid truncation effects in permutation
    
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;
    
    vec4 i = Permute(Permute(ix) + iy);
    
    vec4 gx = 2.0 * fract(i * 0.0243902439) - 1.0; // 1/41 = 0.024...
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;
    
    vec2 g00 = vec2(gx.x, gy.x);
    vec2 g10 = vec2(gx.y, gy.y);
    vec2 g01 = vec2(gx.z, gy.z);
    vec2 g11 = vec2(gx.w, gy.w);
    
    vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(g00, g00), dot(g10, g10), dot(g01, g01), dot(g11, g11));
    g00 *= norm.x;
    g10 *= norm.y;
    g01 *= norm.z;
    g11 *= norm.w;
    
    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));
    
    vec2 fade_xy = Fade(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
    
    return 2.3 * n_xy;
}

// Simplex Noise 2D
vec3 Mod289(vec3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 Mod289(vec2 x)
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
    vec2 s = (P.x + P.y) * F2;
    
    // Unskew the cell origin back to (x,y) space
    const float G2 = (3.0 - sqrt(3.0)) / 6.0;
    vec2 i = floor(P + s);
    vec2 t = (i.x + i.y) * G2;
    
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
    vec3 x = 2.0 * fract(p0.xyyy * vec3(303.8, 303.8, 303.8)) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    
    // Calculate the final noise value
    vec3 g = vec3(dot(d0, vec2(a0.x, h.x)), dot(d1, vec2(a0.y, h.y)), dot(d2, vec2(a0.z, h.z)));
    return 130.0 * dot(m, g);
}

// Fractional Brownian Motion (fBm)
float FractionalBrownianMotion(vec2 P, int octaves, float lacunarity, float gain)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    float maxValue = 0.0;
    
    for (int i = 0; i < octaves; i++)
    {
        total += SimplexNoise2D(P * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return total / maxValue;
}

// Turbulence
float Turbulence(vec2 P, int octaves, float lacunarity, float gain)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    
    for (int i = 0; i < octaves; i++)
    {
        total += abs(SimplexNoise2D(P * frequency)) * amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return total;
}

// Worley Noise (Cellular Noise)
float WorleyNoise(vec2 P)
{
    // Grid cell coordinates
    vec2 cell = floor(P);
    vec2 local = fract(P);
    
    float minDist = 1.0;
    
    // Check neighboring cells
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = neighbor + Random2(cell + neighbor);
            float dist = length(local - point);
            minDist = min(minDist, dist);
        }
    }
    
    return minDist;
}

// Worley Noise with multiple feature points
float WorleyNoise2(vec2 P, float jitter)
{
    vec2 cell = floor(P);
    vec2 local = fract(P);
    
    float f1 = 1.0; // Distance to closest point
    float f2 = 1.0; // Distance to second closest point
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = neighbor + Random2(cell + neighbor) * jitter;
            float dist = length(local - point);
            
            if (dist < f1)
            {
                f2 = f1;
                f1 = dist;
            }
            else if (dist < f2)
            {
                f2 = dist;
            }
        }
    }
    
    return f2 - f1;
}

// Value Noise
float ValueNoise(vec2 P)
{
    vec2 i = floor(P);
    vec2 f = fract(P);
    
    // Four corners in 2D of a tile
    float a = Random(i);
    float b = Random(i + vec2(1.0, 0.0));
    float c = Random(i + vec2(0.0, 1.0));
    float d = Random(i + vec2(1.0, 1.0));
    
    // Smooth interpolation
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    // Mix 4 corners percentages
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// Domain Warping
vec2 DomainWarp(vec2 P, float strength)
{
    vec2 offset = vec2(
        SimplexNoise2D(P),
        SimplexNoise2D(P + vec2(5.2, 1.3))
    );
    return P + offset * strength;
}

// Multiple octaves of domain warping
vec2 DomainWarpFBM(vec2 P, int octaves, float strength)
{
    vec2 result = P;
    float amplitude = strength;
    
    for (int i = 0; i < octaves; i++)
    {
        vec2 offset = vec2(
            SimplexNoise2D(result),
            SimplexNoise2D(result + vec2(5.2, 1.3))
        );
        result += offset * amplitude;
        amplitude *= 0.5;
    }
    
    return result;
}

// Ridge Noise
float RidgeNoise(vec2 P, int octaves, float lacunarity, float gain, float offset)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    float maxValue = 0.0;
    
    for (int i = 0; i < octaves; i++)
    {
        float noise = SimplexNoise2D(P * frequency);
        total += (offset - abs(noise)) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return total / maxValue;
}

// Marble Pattern
float MarblePattern(vec2 P, float scale, float turbulence)
{
    float n = Turbulence(P * scale, 6, 2.0, 0.5) * turbulence;
    return sin((P.x + n) * 6.28318);
}

// Wood Pattern
float WoodPattern(vec2 P, float scale, float turbulence)
{
    float n = Turbulence(P * scale, 4, 2.0, 0.5) * turbulence;
    float dist = length(P + vec2(n, n));
    return abs(sin(dist * 20.0));
}

// Cloud Pattern
float CloudPattern(vec2 P, float scale, float coverage)
{
    float noise = FractionalBrownianMotion(P * scale, 6, 2.0, 0.5);
    return smoothstep(coverage - 0.1, coverage + 0.1, noise);
}

// Fire Pattern
vec3 FirePattern(vec2 P, float time)
{
    vec2 uv = P;
    uv.y -= time * 0.5;
    
    float noise1 = FractionalBrownianMotion(uv * 3.0, 4, 2.0, 0.5);
    float noise2 = FractionalBrownianMotion(uv * 6.0 + vec2(100.0), 4, 2.0, 0.5);
    
    float fire = noise1 * noise2;
    fire = pow(fire, 2.0);
    
    vec3 color = mix(vec3(1.0, 0.0, 0.0), vec3(1.0, 1.0, 0.0), fire);
    color = mix(color, vec3(1.0, 1.0, 1.0), fire * fire);
    
    return color;
}

// Water Caustics Pattern
float WaterCaustics(vec2 P, float time)
{
    vec2 uv = P + time * 0.1;
    
    float caustic1 = SimplexNoise2D(uv * 10.0);
    float caustic2 = SimplexNoise2D(uv * 15.0 + vec2(5.0));
    float caustic3 = SimplexNoise2D(uv * 20.0 + vec2(10.0));
    
    float caustic = caustic1 * caustic2 * caustic3;
    caustic = pow(abs(caustic), 3.0);
    
    return caustic;
}

// 3D Simplex Noise
float SimplexNoise3D(vec3 P)
{
    const float F3 = 1.0 / 3.0;
    const float G3 = 1.0 / 6.0;
    
    vec3 s = floor(P + dot(P, vec3(F3)));
    vec3 x = P - s + dot(s, vec3(G3));
    
    vec3 e = step(vec3(0.0), x - x.yzx);
    vec3 i1 = e * (1.0 - e.zxy);
    vec3 i2 = 1.0 - e.zxy * (1.0 - e);
    
    vec3 x1 = x - i1 + G3;
    vec3 x2 = x - i2 + 2.0 * G3;
    vec3 x3 = x - 1.0 + 3.0 * G3;
    
    vec4 w, d;
    
    w.x = dot(x, x);
    w.y = dot(x1, x1);
    w.z = dot(x2, x2);
    w.w = dot(x3, x3);
    
    w = max(0.6 - w, 0.0);
    
    d.x = dot(Random3(s), x);
    d.y = dot(Random3(s + i1), x1);
    d.z = dot(Random3(s + i2), x2);
    d.w = dot(Random3(s + 1.0), x3);
    
    w *= w;
    w *= w;
    d *= w;
    
    return dot(d, vec4(52.0));
}

// 3D Fractional Brownian Motion
float FractionalBrownianMotion3D(vec3 P, int octaves, float lacunarity, float gain)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    float maxValue = 0.0;
    
    for (int i = 0; i < octaves; i++)
    {
        total += SimplexNoise3D(P * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    
    return total / maxValue;
}

#endif // NOISE_GLSL
