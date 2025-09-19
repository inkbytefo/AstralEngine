// deferred_lighting.slang
// Main shader for the deferred lighting pass.

#include "../Common/lighting_structs.slang"
#include "../Common/pbr_lighting.slang"

// G-Buffer Textures
[vk::binding(0, 0)] Texture2D gAlbedo;
[vk::binding(1, 0)] Texture2D gNormal;
[vk::binding(2, 0)] Texture2D gPBR;
[vk::binding(3, 0)] Texture2D gDepth;
[vk::binding(4, 0)] SamplerState gBufferSampler;

// Shadow Map
[vk::binding(5, 0)] Texture2D shadowMap;
[vk::binding(6, 0)] SamplerComparisonState shadowSampler;

// Scene Uniform Buffer
struct SceneUBO {
    float4x4 view;
    float4x4 projection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float4 viewPosition;
    int lightCount;
    float _padding[3];
    GPULight lights[16]; // MAX_LIGHTS
};

[vk::binding(7, 0)] ConstantBuffer<SceneUBO> sceneUBO;

VSOutput vs_main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.position.y = -output.position.y;
    return output;
}

float CalculateShadow(float3 worldPos, GPULight light) {
    if (light.castsShadows < 0.5) return 1.0; // Not a shadow caster

    float4 shadowCoord = mul(light.lightSpaceMatrix, float4(worldPos, 1.0));
    
    // Perform perspective divide and map to [0, 1] range
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

    // Sample shadow map (using SamplerComparisonState for hardware PCF)
    // The hardware does the comparison between shadowCoord.z and the sampled depth
    return shadowMap.SampleCmpLevelZero(shadowSampler, shadowCoord.xy, shadowCoord.z);
}

float4 fs_main(VSOutput input) : SV_Target0 {
    float3 albedo = gAlbedo.Sample(gBufferSampler, input.uv).rgb;
    float3 normal = gNormal.Sample(gBufferSampler, input.uv).rgb;
    float3 pbr = gPBR.Sample(gBufferSampler, input.uv).rgb;
    float depth = gDepth.Sample(gBufferSampler, input.uv).r;

    float metallic = pbr.r;
    float roughness = pbr.g;
    float ao = pbr.b;

    float4 worldPosH = mul(sceneUBO.inverseProjection, float4(input.uv * 2.0 - 1.0, depth, 1.0));
    worldPosH /= worldPosH.w;
    worldPosH = mul(sceneUBO.inverseView, worldPosH);
    float3 worldPos = worldPosH.xyz;

    float3 finalColor = float3(0.0, 0.0, 0.0);
    float3 viewDir = normalize(sceneUBO.viewPosition.xyz - worldPos);

    for (int i = 0; i < sceneUBO.lightCount; ++i) {
        float shadow = CalculateShadow(worldPos, sceneUBO.lights[i]);
        finalColor += calculatePBR(sceneUBO.lights[i], worldPos, normalize(normal), viewDir, albedo, metallic, roughness, ao) * shadow;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
    finalColor += ambient;

    return float4(finalColor, 1.0);
}
