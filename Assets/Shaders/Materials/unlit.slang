// Unlit Material Vertex Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Unlit material vertex shader using SLANG

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;      // Not used in unlit but available
    float2 texCoord : TEXCOORD0;
    float3 tangent : TANGENT;    // Not used in unlit but available
    float4 color : COLOR0;       // Vertex colors
    
    // Instance data for instanced rendering
    float4 instanceMatrix0 : TEXCOORD1;
    float4 instanceMatrix1 : TEXCOORD2;
    float4 instanceMatrix2 : TEXCOORD3;
    float4 instanceMatrix3 : TEXCOORD4;
    float4 instanceColor : COLOR1; // Per-instance color
};

struct VSOutput {
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 color : COLOR0;
    uint materialID : TEXCOORD2;
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

// Skinning data (if available, but not used in unlit)
#ifdef HAS_SKINNING
struct SkinningUBO {
    float4x4 jointMatrices[256];
};
#endif

[shader("vertex")]
VSOutput main(VSInput input, 
              UniformBufferObject ubo : register(b0),
              MaterialUBO material : register(b1),
              PushConstants pushConstants : register(b2)
#ifdef HAS_SKINNING
              , SkinningUBO skinning : register(b3)
#endif
)
{
    VSOutput output;
    
    // Calculate final model matrix
    float4x4 modelMatrix = pushConstants.model;
    
    // Apply instancing if available
    float4x4 instanceMatrix = float4x4(
        input.instanceMatrix0,
        input.instanceMatrix1,
        input.instanceMatrix2,
        input.instanceMatrix3
    );
    modelMatrix = mul(modelMatrix, instanceMatrix);
    
    // Calculate world position
    float4 worldPos = mul(modelMatrix, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;
    
    // Pass through texture coordinates
    output.texCoord = input.texCoord;
    
    // Calculate color
    float4 vertexColor = float4(1.0, 1.0, 1.0, 1.0);
    if (pushConstants.useVertexColors > 0) {
        vertexColor = input.color;
    }
    
    // Combine vertex color with instance color and material base color
    float4 instanceColor = input.instanceColor * pushConstants.instanceColor;
    output.color = vertexColor * instanceColor * material.baseColorFactor;
    
    // Set material ID (can be used for material variation)
    output.materialID = 0; // Can be set based on material index
    
    // Calculate final position
    output.position = mul(ubo.projection, mul(ubo.view, worldPos));
    
    // Apply depth bias for shadow mapping if needed
#ifdef SHADOW_PASS
    const float depthBias = 0.00001;
    output.position.z -= depthBias;
#endif
    
    return output;
}
