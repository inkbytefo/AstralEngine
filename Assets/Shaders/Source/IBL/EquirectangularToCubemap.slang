import Core.Common;

struct PushConstants {
    float4x4 view;
    float4x4 projection;
};

[vk::push_constant]
PushConstants pc;

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 localPos : POSITION0;
};

[shader("vertex")]
VSOutput vert(VSInput input) {
    VSOutput output;
    output.localPos = input.position;
    // We want the cube to surround the camera, so we don't apply world transform
    output.position = mul(pc.projection, mul(pc.view, float4(input.position, 1.0)));
    return output;
}

[vk::binding(0, 0)]
Sampler2D equirectangularMap;

float2 SampleEquirectangular(float3 v) {
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= float2(0.1591, 0.3183); // 1 / (2*PI), 1 / PI
    uv += 0.5;
    return uv;
}

[shader("fragment")]
float4 frag(VSOutput input) : SV_Target {
    float2 uv = SampleEquirectangular(normalize(input.localPos));
    float3 color = equirectangularMap.Sample(uv).rgb;
    return float4(color, 1.0);
}
