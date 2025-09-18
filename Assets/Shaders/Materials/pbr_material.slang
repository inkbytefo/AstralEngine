// AstralEngine - PBR Material Vertex Shader
// Uses Slang modules for a clean and modern structure.

#include "../Include/camera.slang"
#include "../Include/vertex_attributes.slang"
#include "../Include/material_properties.slang"

// Constant buffer for camera data, bound at register 0.
ConstantBuffer<CameraData> g_camera : register(b0);

// Constant buffer for material data, bound at register 1.
ConstantBuffer<MaterialData> g_material : register(b1);

// Push constants for dynamic per-draw data.
[[vk::push_constant]]
cbuffer PushConstants 
{
    float4x4 modelMatrix;
    int materialID;
};

// Outputs from the vertex shader to the fragment shader.
struct VertexOutput 
{
    float3 worldPos     : TEXCOORD0;
    float3 normal       : TEXCOORD1;
    float2 texCoord     : TEXCOORD2;
    float3 tangent      : TEXCOORD3;
    float3 bitangent    : TEXCOORD4;
    float4 color        : TEXCOORD5;
    float3 viewDir      : TEXCOORD6;
    nointerpolation int materialID : TEXCOORD7;
    float4 position     : SV_POSITION;
};

#ifdef HAS_SKINNING
// Skinning data is provided via separate buffers/textures.
cbuffer SkinningUBO : register(b2) 
{
    float4x4 jointMatrices[256];
};
#endif

// Main vertex shader entry point.
// Accepts standard vertex attributes plus optional instancing and skinning attributes.
[shader("vertex")]
VertexOutput main(
    VertexInput vertexIn, 
    InstanceInput instanceIn
#ifdef HAS_SKINNING
    , SkinningInput skinIn
#endif
) 
{
    VertexOutput output;

    // Combine model matrix with instance matrix.
    float4x4 finalModelMatrix = mul(modelMatrix, GetInstanceMatrix(instanceIn));

    float4 worldPos = mul(finalModelMatrix, float4(vertexIn.position, 1.0));
    
    // Apply skinning if enabled.
#ifdef HAS_SKINNING
    float4x4 skinMatrix = float4x4(0.0);
    for (int i = 0; i < 4; i++) 
    {
        if (skinIn.jointWeights[i] > 0.0) 
        {
            skinMatrix += jointMatrices[skinIn.jointIndices[i]] * skinIn.jointWeights[i];
        }
    }
    worldPos = mul(skinMatrix, float4(vertexIn.position, 1.0));
#endif

    output.worldPos = worldPos.xyz;

    // Correctly transform normals and tangents.
    float3x3 normalMatrix = (float3x3)finalModelMatrix;
#ifdef HAS_SKINNING
    normalMatrix = (float3x3)skinMatrix;
#endif
    normalMatrix = transpose(normalMatrix);

    output.normal = normalize(mul(normalMatrix, vertexIn.normal));
    output.tangent = normalize(mul(normalMatrix, vertexIn.tangent));
    output.bitangent = normalize(cross(output.normal, output.tangent));

    // Pass through texture coordinates and color.
    output.texCoord = vertexIn.texCoord;
    output.color = vertexIn.color * g_material.baseColorFactor;

    // Calculate view direction.
    output.viewDir = normalize(g_camera.position - output.worldPos);
    
    // Pass material ID.
    output.materialID = materialID;

    // Calculate final clip space position.
    output.position = mul(g_camera.viewProjection, worldPos);

    return output;
}
