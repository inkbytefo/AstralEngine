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
    output.position = mul(pc.projection, mul(pc.view, float4(input.position, 1.0)));
    return output;
}

[vk::binding(0, 0)]
SamplerCube environmentMap;

[shader("fragment")]
float4 frag(VSOutput input) : SV_Target {
    float3 normal = normalize(input.localPos);
    float3 irradiance = float3(0.0);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < TWO_PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < HALF_PI; theta += sampleDelta) {
            // Spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // Tangent space to world space
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += environmentMap.Sample(sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / nrSamples);

    return float4(irradiance, 1.0);
}
