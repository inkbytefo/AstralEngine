// Triangle Vertex Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Basic vertex shader for triangle rendering using SLANG

struct VSInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 texCoord : TEXCOORD0;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[shader("vertex")]
VSOutput main(VSInput input, UniformBufferObject ubo : register(b0))
{
    VSOutput output;
    
    // Calculate position
    output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
    
    // Pass through color (white for now)
    output.color = float3(1.0, 1.0, 1.0);
    
    // Pass through texture coordinates
    output.texCoord = input.texCoord;
    
    return output;
}
