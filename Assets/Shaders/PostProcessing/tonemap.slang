// Tonemapping Vertex Shader
// Author: inkbytefo
// Project: AstralEngine

#include "../Common/common.slang"

// Vertex shader inputs for full-screen quad
[[vk::location(0)]] float3 inPosition : POSITION;
[[vk::location(1)]] float2 inTexCoord : TEXCOORD0;

// Outputs to fragment shader
struct VertexOutput {
    [[vk::location(0)]] float2 texCoord : TEXCOORD0;
    [[vk::location(1)]] float3 viewRay : TEXCOORD1;
    float4 position : SV_POSITION;
};

// Uniform buffer objects
cbuffer UniformBufferObject : register(b0) {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

// Push constants
[[vk::push_constant]] cbuffer PushConstants {
    float2 texelSize;
    int useBloom;
    int useVignette;
    int useChromaticAberration;
};

[[shader("vertex")]]
VertexOutput main() {
    VertexOutput output;
    
    // Pass through texture coordinates
    output.texCoord = inTexCoord;
    
    // Calculate view ray for effects like depth of field
    // This creates rays from camera through each pixel
    float4 clipPos = float4(inPosition.xy, 1.0, 1.0);
    float4 viewPos = mul(inverse4x4(projection), clipPos);
    viewPos = float4(viewPos.xy, -1.0, 0.0); // Point towards negative Z
    
    // Transform to world space
    float4x4 invView = inverse4x4(view);
    float4 worldPos = mul(invView, viewPos);
    output.viewRay = normalize(worldPos.xyz);
    
    // Output position (full-screen quad)
    output.position = float4(inPosition.xy, 0.0, 1.0);
    
    return output;
}
