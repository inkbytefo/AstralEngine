
struct VertexInput
{
    float2 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

struct UBO
{
    float4x4 model;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UBO> ubo;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 pos = float4(input.position, 0.0, 1.0);
    // Rotate around Z axis (2D rotation)
    output.position = mul(ubo.model, pos);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
