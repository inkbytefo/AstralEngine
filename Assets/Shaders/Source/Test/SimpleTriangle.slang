// SimpleTriangle.slang
// A simple test shader for verifying the render pipeline.

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

// Push Constants for lightweight data (Model Matrix)
struct PushConstants
{
    float4x4 modelMatrix;
};

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    
    // Apply model rotation
    // Note: We assume view and projection are identity/simple for this test
    float4 pos = float4(input.position, 1.0);
    output.position = mul(pc.modelMatrix, pos);
    
    // Pass color through
    output.color = input.color;
    
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
