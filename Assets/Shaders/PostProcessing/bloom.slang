// Bloom Vertex Shader
// Author: inkbytefo
// Project: AstralEngine

// Vertex shader inputs for full-screen quad
[[vk::location(0)]] float3 inPosition : POSITION;
[[vk::location(1)]] float2 inTexCoord : TEXCOORD0;

// Outputs to fragment shader
struct VertexOutput {
    [[vk::location(0)]] float2 texCoord : TEXCOORD0;
    [[vk::location(1)]] float2 texCoords[5] : TEXCOORD1; // For sampling in bloom shader
    float4 position : SV_POSITION;
};

// Push constants
[[vk::push_constant]] cbuffer PushConstants {
    float2 texelSize;
    int bloomPass; // 0: bright pass, 1: horizontal blur, 2: vertical blur, 3: composite
};

VertexOutput main() {
    VertexOutput output;
    
    // Pass through texture coordinates
    output.texCoord = inTexCoord;
    
    // Calculate sampling coordinates for bloom
    // This creates a cross pattern for efficient sampling
    float2 texelSize = texelSize;
    
    // Center sample
    output.texCoords[0] = inTexCoord;
    
    // Cross samples for blur
    output.texCoords[1] = inTexCoord + float2(texelSize.x * 1.0, 0.0);
    output.texCoords[2] = inTexCoord + float2(texelSize.x * -1.0, 0.0);
    output.texCoords[3] = inTexCoord + float2(0.0, texelSize.y * 1.0);
    output.texCoords[4] = inTexCoord + float2(0.0, texelSize.y * -1.0);
    
    // Output position (full-screen quad)
    output.position = float4(inPosition.xy, 0.0, 1.0);
    
    return output;
}
