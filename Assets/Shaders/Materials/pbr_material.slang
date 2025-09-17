// PBR Material Vertex Shader
// Author: inkbytefo
// Project: AstralEngine

#include "../Include/vertex_attributes.glsl"
#include "../Include/camera.glsl"

// Vertex shader inputs
[[vk::location(0)]] float3 inPosition : POSITION;
[[vk::location(1)]] float3 inNormal : NORMAL;
[[vk::location(2)]] float2 inTexCoord : TEXCOORD0;
[[vk::location(3)]] float3 inTangent : TANGENT;
[[vk::location(4)]] float4 inColor : COLOR0; // Optional vertex colors

// Instance data for instanced rendering
[[vk::location(5)]] float4 inInstanceMatrix0 : INSTANCEMATRIX0;
[[vk::location(6)]] float4 inInstanceMatrix1 : INSTANCEMATRIX1;
[[vk::location(7)]] float4 inInstanceMatrix2 : INSTANCEMATRIX2;
[[vk::location(8)]] float4 inInstanceMatrix3 : INSTANCEMATRIX3;
[[vk::location(9)]] float4 inInstanceColor : INSTANCECOLOR; // Per-instance color

// Uniform buffer objects
cbuffer UniformBufferObject : register(b0) {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

cbuffer MaterialUBO : register(b1) {
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float3 emissiveFactor;
    float alphaCutoff;
    int alphaMode; // 0: OPAQUE, 1: MASK, 2: BLEND
    int doubleSided;
};

// Push constants for dynamic data
[[vk::push_constant]] cbuffer PushConstants {
    float4x4 model;
    float4 instanceColor;
    float metallicFactor;
    float roughnessFactor;
    int useVertexColors;
    int hasSkinning;
};

// Outputs to fragment shader
struct VertexOutput {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
    [[vk::location(1)]] float3 normal : TEXCOORD1;
    [[vk::location(2)]] float2 texCoord : TEXCOORD2;
    [[vk::location(3)]] float3 tangent : TEXCOORD3;
    [[vk::location(4)]] float3 bitangent : TEXCOORD4;
    [[vk::location(5)]] float4 color : TEXCOORD5;
    [[vk::location(6)]] float3 viewDir : TEXCOORD6;
    [[vk::location(7)]] nointerpolation int materialID : TEXCOORD7;
    float4 position : SV_POSITION;
};

// Skinning data (if available)
#ifdef HAS_SKINNING
cbuffer SkinningUBO : register(b2) {
    float4x4 jointMatrices[256];
};

[[vk::location(10)]] int4 inJointIndices : JOINTINDICES;
[[vk::location(11)]] float4 inJointWeights : JOINTWEIGHTS;
#endif

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    // Calculate final model matrix
    float4x4 modelMatrix = model;
    
    // Apply instancing if available
    float4x4 instanceMatrix = float4x4(
        inInstanceMatrix0,
        inInstanceMatrix1,
        inInstanceMatrix2,
        inInstanceMatrix3
    );
    modelMatrix = mul(modelMatrix, instanceMatrix);
    
    // Calculate world position
    float4 worldPos = mul(modelMatrix, float4(inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    
    // Calculate normal matrix for proper normal transformation
    float3x3 normalMatrix = transpose(inverse(modelMatrix));
    
    // Apply skinning if available
#ifdef HAS_SKINNING
    if (hasSkinning > 0) {
        float4x4 skinMatrix = float4x4(0.0);
        for (int i = 0; i < 4; i++) {
            if (inJointWeights[i] > 0.0) {
                int jointIndex = inJointIndices[i];
                skinMatrix += jointMatrices[jointIndex] * inJointWeights[i];
            }
        }
        
        worldPos = mul(skinMatrix, float4(inPosition, 1.0));
        output.worldPos = worldPos.xyz;
        
        // Transform normal with skinning
        float3x3 skinNormalMatrix = transpose(inverse(skinMatrix));
        output.normal = normalize(mul(skinNormalMatrix, inNormal));
        
        // Transform tangent and bitangent with skinning
        output.tangent = normalize(mul(skinNormalMatrix, inTangent));
        output.bitangent = normalize(cross(output.normal, output.tangent));
    } else {
#endif
        // Transform normal, tangent, and bitangent
        output.normal = normalize(mul(normalMatrix, inNormal));
        output.tangent = normalize(mul(normalMatrix, inTangent));
        output.bitangent = normalize(cross(output.normal, output.tangent));
#ifdef HAS_SKINNING
    }
#endif
    
    // Pass through texture coordinates
    output.texCoord = inTexCoord;
    
    // Calculate view direction
    output.viewDir = normalize(cameraPosition - output.worldPos);
    
    // Calculate color
    float4 vertexColor = float4(1.0);
    if (useVertexColors > 0) {
        vertexColor = inColor;
    }
    
    // Combine vertex color with instance color and material base color
    float4 instanceColor = inInstanceColor * instanceColor;
    output.color = vertexColor * instanceColor * baseColorFactor;
    
    // Set material ID (can be used for material variation)
    output.materialID = 0; // Can be set based on material index
    
    // Calculate final position
    output.position = mul(projection, mul(view, worldPos));
    
    // Apply depth bias for shadow mapping if needed
#ifdef SHADOW_PASS
    const float depthBias = 0.00001;
    output.position.z -= depthBias;
#endif
    
    return output;
}
