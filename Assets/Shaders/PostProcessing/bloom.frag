#version 450

// Bloom Fragment Shader
// Author: inkbytefo
// Project: AstralEngine

// Constants
const float PI = 3.14159265359;

// Inputs from vertex shader
layout(location = 0) in vec2 inTexCoord;

// Output
layout(location = 0) out vec4 outColor;

// Texture samplers
layout(binding = 0) uniform sampler2D sceneColor;    // Input texture (changes based on pass)
layout(binding = 1) uniform sampler2D brightPass;    // Bright pass texture
layout(binding = 2) uniform sampler2D blurTexture;   // Blur texture for composite

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

layout(binding = 1) uniform BloomUBO {
    float threshold;           // Bright pass threshold
    float knee;               // Soft knee threshold
    float intensity;          // Bloom intensity
    float radius;             // Blur radius
    int quality;             // 0: low, 1: medium, 2: high
    int useDirt;            // Use lens dirt
    float dirtIntensity;     // Lens dirt intensity
    vec2 padding;
} bloomSettings;

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    int bloomPass; // 0: bright pass, 1: horizontal blur, 2: vertical blur, 3: composite
} pushConstants;

// Simplex Noise 2D implementation
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
    vec3 m = max(0.5 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2)), vec3(0.0));
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

// Bright pass filter - extracts bright areas for bloom
vec3 BrightPass(vec3 color, float threshold, float knee)
{
    // Calculate luminance
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
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
vec3 GaussianBlur(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    
    // Calculate sigma based on radius
    float sigma = radius * 0.5;
    
    // 13-tap blur
    const int taps = 13;
    float offsets[taps] = float[](-6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    
    for (int i = 0; i < taps; i++) {
        float weight = Gaussian(offsets[i], sigma);
        vec2 offset = vec2(offsets[i] * texelSize.x, 0.0); // Horizontal blur
        
        if (pushConstants.bloomPass == 2) { // Vertical blur
            offset = vec2(0.0, offsets[i] * texelSize.y);
        }
        
        color += texture(tex, uv + offset).rgb * weight;
        totalWeight += weight;
    }
    
    return color / totalWeight;
}

// 5-tap Kawase blur (faster, good for bloom)
vec3 KawaseBlur(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec3 color = vec3(0.0);
    float offset = radius * texelSize.x;
    
    if (pushConstants.bloomPass == 2) { // Vertical blur
        offset = radius * texelSize.y;
    }
    
    // 5-tap cross pattern
    color += texture(tex, uv).rgb * 0.4;
    color += texture(tex, uv + vec2(offset, 0.0)).rgb * 0.15;
    color += texture(tex, uv + vec2(-offset, 0.0)).rgb * 0.15;
    color += texture(tex, uv + vec2(0.0, offset)).rgb * 0.15;
    color += texture(tex, uv + vec2(0.0, -offset)).rgb * 0.15;
    
    return color;
}

// Dual Kawase blur (even faster, good for mobile)
vec3 DualKawaseBlur(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec3 color = vec3(0.0);
    float offset = radius * texelSize.x;
    
    if (pushConstants.bloomPass == 2) { // Vertical blur
        offset = radius * texelSize.y;
    }
    
    // Dual Kawase pattern
    color += texture(tex, uv + vec2(-offset, -offset)).rgb * 0.25;
    color += texture(tex, uv + vec2(offset, -offset)).rgb * 0.25;
    color += texture(tex, uv + vec2(-offset, offset)).rgb * 0.25;
    color += texture(tex, uv + vec2(offset, offset)).rgb * 0.25;
    
    return color;
}

// Composite bloom with original scene
vec3 CompositeBloom(vec3 sceneColor, vec3 bloomColor, float intensity)
{
    return sceneColor + bloomColor * intensity;
}

// Anamorphic flare effect
vec3 AnamorphicFlare(vec3 color, vec2 uv, float time)
{
    vec2 center = vec2(0.5, 0.5);
    vec2 dir = normalize(uv - center);
    
    // Create horizontal streaks
    float streak = pow(abs(dir.x), 8.0) * 0.1;
    
    // Add some noise for realism
    float noise = SimplexNoise2D(uv * 50.0 + time * 0.1);
    streak *= (0.5 + noise * 0.5);
    
    // Color the flare
    vec3 flareColor = vec3(1.0, 0.9, 0.7) * streak;
    
    return color + flareColor;
}

void main() {
    vec3 color = vec3(0.0);
    
    switch (pushConstants.bloomPass) {
        case 0: // Bright pass
        {
            // Sample scene color
            vec3 sceneColor = texture(sceneColor, inTexCoord).rgb;
            
            // Apply bright pass filter
            color = BrightPass(sceneColor, bloomSettings.threshold, bloomSettings.knee);
            
            // Add some anamorphic flare to bright areas
            float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
            if (luminance > bloomSettings.threshold) {
                color = AnamorphicFlare(color, inTexCoord, ubo.time);
            }
            break;
        }
        
        case 1: // Horizontal blur
        case 2: // Vertical blur
        {
            // Choose blur quality
            if (bloomSettings.quality == 0) {
                // Low quality - Dual Kawase
                color = DualKawaseBlur(sceneColor, inTexCoord, pushConstants.texelSize, bloomSettings.radius);
            } else if (bloomSettings.quality == 1) {
                // Medium quality - Kawase
                color = KawaseBlur(sceneColor, inTexCoord, pushConstants.texelSize, bloomSettings.radius);
            } else {
                // High quality - Gaussian
                color = GaussianBlur(sceneColor, inTexCoord, pushConstants.texelSize, bloomSettings.radius);
            }
            break;
        }
        
        case 3: // Composite
        {
            // Sample original scene
            vec3 sceneColor = texture(sceneColor, inTexCoord).rgb;
            
            // Sample blurred bloom texture
            vec3 bloomColor = texture(blurTexture, inTexCoord).rgb;
            
            // Composite bloom
            color = CompositeBloom(sceneColor, bloomColor, bloomSettings.intensity);
            
            // Add lens dirt if enabled
            if (bloomSettings.useDirt > 0) {
                vec3 dirt = texture(brightPass, inTexCoord).rgb; // Reuse bright pass for dirt
                color += dirt * bloomSettings.dirtIntensity;
            }
            break;
        }
    }
    
    // Output final color
    outColor = vec4(color, 1.0);
}
