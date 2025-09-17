// Triangle Fragment Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Basic fragment shader for triangle rendering using SLANG

struct PSInput {
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 texCoord : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target0;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

Texture2D texSampler : register(t1);
SamplerState samplerState : register(s1);

[shader("pixel")]
PSOutput main(PSInput input, UniformBufferObject ubo : register(b0))
{
    PSOutput output;
    
    // Sample texture
    float4 texColor = texSampler.Sample(samplerState, input.texCoord);
    
    // Check if texture is valid (alpha very low)
    if (texColor.a < 0.1) {
        // Fallback to vertex color (white)
        output.color = float4(input.color, 1.0);
    } else {
        // Use texture with vertex color modulation
        output.color = float4(texColor.rgb * input.color, texColor.a);
    }
    
    return output;
}
