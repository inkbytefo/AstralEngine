// Unlit Material Fragment Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Unlit material fragment shader using SLANG

struct PSInput {
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 color : COLOR0;
    uint materialID : TEXCOORD2;
};

struct PSOutput {
    float4 color : SV_Target0;
    float4 emissive : SV_Target1; // For bloom pass
    float4 normal : SV_Target2;   // For deferred rendering
    float4 position : SV_Target3; // For deferred rendering
    float4 material : SV_Target4; // For deferred rendering
};

struct UniformBufferObject {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

struct MaterialUBO {
    float4 baseColorFactor;
    float metallicFactor;    // Not used in unlit
    float roughnessFactor;   // Not used in unlit
    float normalScale;       // Not used in unlit
    float occlusionStrength; // Not used in unlit
    float3 emissiveFactor;
    float alphaCutoff;
    int alphaMode; // 0: OPAQUE, 1: MASK, 2: BLEND
    int doubleSided;
};

struct PushConstants {
    float4x4 model;
    float4 instanceColor;
    float metallicFactor;    // Not used in unlit
    float roughnessFactor;   // Not used in unlit
    int useVertexColors;
    int hasSkinning;         // Not used in unlit
};

struct TextureFlags {
    int hasAlbedoMap;
    int hasNormalMap;        // Not used in unlit
    int hasMetallicMap;      // Not used in unlit
    int hasRoughnessMap;     // Not used in unlit
    int hasAOMap;            // Not used in unlit
    int hasEmissiveMap;
    int hasHeightMap;        // Not used in unlit
    int usePackedTextures;   // Not used in unlit
};

// Texture samplers
Texture2D albedoMap : register(t0);
Texture2D emissiveMap : register(t1);
SamplerState linearSampler : register(s0);

// Helper functions
float3 SRGBToLinear(float3 srgb)
{
    return pow(srgb, 2.2);
}

float SimplexNoise2D(float2 uv)
{
    // Simple implementation of simplex noise
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

// Simple UV animation
float2 AnimateUV(float2 uv, float time, float2 speed, float2 tiling)
{
    return (uv * tiling) + (time * speed);
}

// Dissolve effect
float Dissolve(float2 uv, float time, float edgeWidth, float noiseScale)
{
    float noise = SimplexNoise2D(uv * noiseScale + time * 0.1);
    float threshold = sin(time * 2.0) * 0.5 + 0.5;
    float edge = smoothstep(threshold - edgeWidth, threshold + edgeWidth, noise);
    return edge;
}

// Flicker effect
float Flicker(float time, float frequency, float intensity)
{
    float flicker = sin(time * frequency) * 0.5 + 0.5;
    return 1.0 + flicker * intensity;
}

[shader("pixel")]
PSOutput main(PSInput input,
              UniformBufferObject ubo : register(b0),
              MaterialUBO material : register(b1),
              PushConstants pushConstants : register(b2),
              TextureFlags textureFlags : register(b3))
{
    PSOutput output;
    
    // Alpha testing
    if (material.alphaMode == 1) { // MASK
        float4 albedoSample = albedoMap.Sample(linearSampler, input.texCoord);
        if (albedoSample.a < material.alphaCutoff) {
            discard;
        }
    }
    
    // Sample albedo
    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);
    if (textureFlags.hasAlbedoMap > 0) {
        float4 albedoSample = albedoMap.Sample(linearSampler, input.texCoord);
        albedo.rgb = SRGBToLinear(albedoSample.rgb);
        albedo.a = albedoSample.a;
    } else {
        albedo = material.baseColorFactor;
    }
    
    // Apply vertex colors if enabled
    if (pushConstants.useVertexColors > 0) {
        albedo *= input.color;
    }
    
    // Sample emissive
    float3 emissive = material.emissiveFactor;
    if (textureFlags.hasEmissiveMap > 0) {
        emissive *= SRGBToLinear(emissiveMap.Sample(linearSampler, input.texCoord).rgb);
    }
    
    // Apply emissive flicker effect
    emissive *= Flicker(ubo.time, 10.0, 0.1);
    
    // Calculate final color (unlit - no lighting calculations)
    float3 finalColor = albedo.rgb + emissive;
    
    // Apply dissolve effect (optional, can be controlled by material parameters)
    float dissolveEdge = Dissolve(input.texCoord, ubo.time, 0.05, 5.0);
    finalColor *= dissolveEdge;
    
    // Apply alpha blending
    if (material.alphaMode == 2) { // BLEND
        output.color = float4(finalColor, albedo.a);
    } else {
        output.color = float4(finalColor, 1.0);
    }
    
    // Output for deferred rendering
    output.emissive = float4(emissive, 1.0);
    output.normal = float4(0.5, 0.5, 1.0, 1.0); // Default normal for unlit
    output.position = float4(input.worldPos, 1.0);
    output.material = float4(0.0, 1.0, 1.0, 1.0); // Default material properties for unlit
    
    return output;
}
