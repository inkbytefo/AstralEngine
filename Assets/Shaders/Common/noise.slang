// Noise generation and procedural texture functions
// Author: inkbytefo
// Project: AstralEngine
// Description: Noise generation functions for SLANG shaders

// Classic Perlin noise
float2 Fade(float2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float4 Permute(float4 x)
{
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float4 TaylorInvSqrt(float4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float ClassicPerlinNoise(float2 P)
{
    float4 Pi = floor(P.xyxy) + float4(0.0, 0.0, 1.0, 1.0);
    float4 Pf = fract(P.xyxy) - float4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, 289.0); // To avoid truncation effects in permutation
    
    float4 ix = Pi.xzxz;
    float4 iy = Pi.yyww;
    float4 fx = Pf.xzxz;
    float4 fy = Pf.yyww;
    
    float4 i = Permute(Permute(ix) + iy);
    
    float4 gx = 2.0 * frac(i * 0.0243902439) - 1.0; // 1/41 = 0.024...
    float4 gy = abs(gx) - 0.5;
    float4 tx = floor(gx + 0.5);
    gx = gx - tx;
    
    float2 g00 = float2(gx.x, gy.x);
    float2 g10 = float2(gx.y, gy.y);
    float2 g01 = float2(gx.z, gy.z);
    float2 g11 = float2(gx.w, gy.w);
    
    float4 norm = 1.79284291400159 - 0.85373472095314 * float4(dot(g00, g00), dot(g10, g10), dot(g01, g01), dot(g11, g11));
    g00 *= norm.x;
    g10 *= norm.y;
    g01 *= norm.z;
    g11 *= norm.w;
    
    float n00 = dot(g00, float2(fx.x, fy.x));
    float n10 = dot(g10, float2(fx.y, fy.y));
    float n01 = dot(g01, float2(fx.z, fy.z));
    float n11 = dot(g11, float2(fx.w, fy.w));
    
    float2 fade_xy = Fade(Pf.xy);
    float2 n_x = lerp(float2(n00, n01), float2(n10, n11), fade_xy.x);
    float n_xy = lerp(n_x.x, n_x.y, fade_xy.y);
    
    return 2.3 * n_xy;
}

// Simplex Noise 2D
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
    float2 s = (P.x + P.y) * F2;
    
    // Unskew the cell origin back to (x,y) space
    const float G2 = (3.0 - sqrt(3.0)) / 6.0;
    float2 i = floor(P + s);
    float2 t = (i.x + i.y) * G2;
    
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
    float3 x = 2.0 * frac(p0.xyyy * float3(303.8, 303.8, 303.8)) - 1.0;
    float3 h = abs(x) - 0.5;
    float3 ox = floor(x + 0.5);
    float3 a0 = x - ox;
    
    // Calculate the final noise value
    float3 g = float3(dot(d0, float2(a0.x, h.x)), dot(d1, float2(a0.y, h.y)), dot(d2, float2(a0.z, h.z)));
    return 130.0 * dot(m, g);
}

// Fractional Brownian Motion (fBm)
float FractionalBrownianMotion(float2 P, int octaves, float lacunarity, float gain)
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
float Turbulence(float2 P, int octaves, float lacunarity, float gain)
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
float WorleyNoise(float2 P)
{
    // Grid cell coordinates
    float2 cell = floor(P);
    float2 local = fract(P);
    
    float minDist = 1.0;
    
    // Check neighboring cells
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 neighbor = float2(float(x), float(y));
            float2 point = neighbor + Random2(cell + neighbor);
            float dist = length(local - point);
            minDist = min(minDist, dist);
        }
    }
    
    return minDist;
}

// Worley Noise with multiple feature points
float WorleyNoise2(float2 P, float jitter)
{
    float2 cell = floor(P);
    float2 local = fract(P);
    
    float f1 = 1.0; // Distance to closest point
    float f2 = 1.0; // Distance to second closest point
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 neighbor = float2(float(x), float(y));
            float2 point = neighbor + Random2(cell + neighbor) * jitter;
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
float ValueNoise(float2 P)
{
    float2 i = floor(P);
    float2 f = fract(P);
    
    // Four corners in 2D of a tile
    float a = Random(i);
    float b = Random(i + float2(1.0, 0.0));
    float c = Random(i + float2(0.0, 1.0));
    float d = Random(i + float2(1.0, 1.0));
    
    // Smooth interpolation
    float2 u = f * f * (3.0 - 2.0 * f);
    
    // Mix 4 corners percentages
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Domain Warping
float2 DomainWarp(float2 P, float strength)
{
    float2 offset = float2(
        SimplexNoise2D(P),
        SimplexNoise2D(P + float2(5.2, 1.3))
    );
    return P + offset * strength;
}

// Multiple octaves of domain warping
float2 DomainWarpFBM(float2 P, int octaves, float strength)
{
    float2 result = P;
    float amplitude = strength;
    
    for (int i = 0; i < octaves; i++)
    {
        float2 offset = float2(
            SimplexNoise2D(result),
            SimplexNoise2D(result + float2(5.2, 1.3))
        );
        result += offset * amplitude;
        amplitude *= 0.5;
    }
    
    return result;
}

// Ridge Noise
float RidgeNoise(float2 P, int octaves, float lacunarity, float gain, float offset)
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
float MarblePattern(float2 P, float scale, float turbulence)
{
    float n = Turbulence(P * scale, 6, 2.0, 0.5) * turbulence;
    return sin((P.x + n) * 6.28318);
}

// Wood Pattern
float WoodPattern(float2 P, float scale, float turbulence)
{
    float n = Turbulence(P * scale, 4, 2.0, 0.5) * turbulence;
    float dist = length(P + float2(n, n));
    return abs(sin(dist * 20.0));
}

// Cloud Pattern
float CloudPattern(float2 P, float scale, float coverage)
{
    float noise = FractionalBrownianMotion(P * scale, 6, 2.0, 0.5);
    return smoothstep(coverage - 0.1, coverage + 0.1, noise);
}

// Fire Pattern
float3 FirePattern(float2 P, float time)
{
    float2 uv = P;
    uv.y -= time * 0.5;
    
    float noise1 = FractionalBrownianMotion(uv * 3.0, 4, 2.0, 0.5);
    float noise2 = FractionalBrownianMotion(uv * 6.0 + float2(100.0), 4, 2.0, 0.5);
    
    float fire = noise1 * noise2;
    fire = pow(fire, 2.0);
    
    float3 color = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 1.0, 0.0), fire);
    color = lerp(color, float3(1.0, 1.0, 1.0), fire * fire);
    
    return color;
}

// Water Caustics Pattern
float WaterCaustics(float2 P, float time)
{
    float2 uv = P + time * 0.1;
    
    float caustic1 = SimplexNoise2D(uv * 10.0);
    float caustic2 = SimplexNoise2D(uv * 15.0 + float2(5.0));
    float caustic3 = SimplexNoise2D(uv * 20.0 + float2(10.0));
    
    float caustic = caustic1 * caustic2 * caustic3;
    caustic = pow(abs(caustic), 3.0);
    
    return caustic;
}

// 3D Simplex Noise
float SimplexNoise3D(float3 P)
{
    const float F3 = 1.0 / 3.0;
    const float G3 = 1.0 / 6.0;
    
    float3 s = floor(P + dot(P, float3(F3, F3, F3)));
    float3 x = P - s + dot(s, float3(G3, G3, G3));
    
    float3 e = step(float3(0.0, 0.0, 0.0), x - x.yzx);
    float3 i1 = e * (1.0 - e.zxy);
    float3 i2 = 1.0 - e.zxy * (1.0 - e);
    
    float3 x1 = x - i1 + G3;
    float3 x2 = x - i2 + 2.0 * G3;
    float3 x3 = x - 1.0 + 3.0 * G3;
    
    float4 w, d;
    
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
    
    return dot(d, float4(52.0, 52.0, 52.0, 52.0));
}

// 3D Fractional Brownian Motion
float FractionalBrownianMotion3D(float3 P, int octaves, float lacunarity, float gain)
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
