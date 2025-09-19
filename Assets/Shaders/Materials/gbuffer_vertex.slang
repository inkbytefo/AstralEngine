// gbuffer.slang
// G-Buffer Pass Shader for Deferred Shading

// Includes common structures and functions
#include "../Include/vertex_attributes.slang"
#include "../Include/camera.slang"
#include "../Include/material_properties.slang"

struct VertexInput {
    float3 position      : POSITION;
    float3 normal        : NORMAL;
    float2 uv            : TEXCOORD0;
    float3 tangent       : TANGENT;
    float3 bitangent     : BITANGENT;
    // Per-instance data
    float4 instanceRow1  : INSTANCE_ROW1; 
    float4 instanceRow2  : INSTANCE_ROW2;
    float4 instanceRow3  : INSTANCE_ROW3;
    float4 instanceRow4  : INSTANCE_ROW4;
};

struct VertexOutput {
    float4 position     : SV_Position;
    float3 worldPosition : WORLDPOSITION;
    float3 worldNormal   : NORMAL;
    float2 uv           : TEXCOORD0;
    float3 worldTangent : TANGENT;
};

[vk::binding(0, 0)]
ConstantBuffer<CameraData> cameraData;

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;
    
    float4x4 modelMatrix = float4x4(input.instanceRow1, input.instanceRow2, input.instanceRow3, input.instanceRow4);

    // Transform position and normal to world space
    output.worldPosition = mul(modelMatrix, float4(input.position, 1.0)).xyz;
    output.worldNormal = normalize(mul((float3x3)modelMatrix, input.normal));
    output.worldTangent = normalize(mul((float3x3)modelMatrix, input.tangent));

    // Transform position to clip space
    output.position = mul(cameraData.viewProjection, float4(output.worldPosition, 1.0));
    
    // Pass UV coordinates
    output.uv = input.uv;
    
    return output;
}

// G-Buffer output structure
struct GBufferOutput {
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 pbr    : SV_Target2; // Metallic, Roughness, AO
};

// Material Textures
[vk::binding(1, 1)]
Texture2D albedoTexture;
[vk::binding(2, 1)]
Texture2D normalTexture;
[vk::binding(3, 1)]
Texture2D metallicTexture;
[vk::binding(4, 1)]
Texture2D roughnessTexture;
[vk::binding(5, 1)]
Texture2D aoTexture;
[vk::binding(6, 1)]
SamplerState materialSampler;

GBufferOutput fs_main(VertexOutput input) {
    GBufferOutput output;

    // Albedo
    float4 albedoSample = albedoTexture.Sample(materialSampler, input.uv);
    // Basic alpha testing
    if (albedoSample.a < 0.1) {
        discard;
    }
    output.albedo = albedoSample;

    // Normals (from Normal Map)
    float3 tangentNormal = normalTexture.Sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
    float3 N = normalize(input.worldNormal);
    float3 T = normalize(input.worldTangent);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt orthogonalize
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    float3 worldNormal = normalize(mul(TBN, tangentNormal));
    output.normal = float4(worldNormal, 0.0);

    // PBR Properties
    float metallic = metallicTexture.Sample(materialSampler, input.uv).r;
    float roughness = roughnessTexture.Sample(materialSampler, input.uv).r;
    float ao = aoTexture.Sample(materialSampler, input.uv).r;
    output.pbr = float4(metallic, roughness, ao, 0.0);

    return output;
}
