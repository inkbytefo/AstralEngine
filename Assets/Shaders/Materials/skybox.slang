// Skybox Vertex Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Skybox vertex shader using SLANG

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 texCoord : TEXCOORD1;
    uint materialID : TEXCOORD2;
};

struct UniformBufferObject {
    float4x4 projection;
    float4x4 view;
    float3 cameraPosition;
    float time;
};

struct PushConstants {
    float4x4 model;
    float intensity;
    int useHDR;
    int rotationEnabled;
};

[shader("vertex")]
VSOutput main(VSInput input,
              UniformBufferObject ubo : register(b0),
              PushConstants pushConstants : register(b1))
{
    VSOutput output;
    
    // Remove translation from view matrix for skybox
    float4x4 viewNoTranslation = float4x4(
        float4(ubo.view[0].xyz, 0.0),
        float4(ubo.view[1].xyz, 0.0),
        float4(ubo.view[2].xyz, 0.0),
        float4(0.0, 0.0, 0.0, 1.0)
    );
    
    // Apply rotation if enabled
    float4x4 modelMatrix = pushConstants.model;
    if (pushConstants.rotationEnabled > 0) {
        float angle = ubo.time * 0.1; // Slow rotation
        float cosAngle = cos(angle);
        float sinAngle = sin(angle);
        float4x4 rotation = float4x4(
            float4(cosAngle, 0.0, sinAngle, 0.0),
            float4(0.0, 1.0, 0.0, 0.0),
            float4(-sinAngle, 0.0, cosAngle, 0.0),
            float4(0.0, 0.0, 0.0, 1.0)
        );
        modelMatrix = mul(rotation, modelMatrix);
    }
    
    // Calculate world position (skybox is always at camera position)
    float4 worldPos = mul(modelMatrix, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;
    
    // Pass texture coordinates (same as position for cube mapping)
    output.texCoord = input.position;
    
    // Set material ID
    output.materialID = 0; // Skybox material ID
    
    // Calculate final position
    output.position = mul(ubo.projection, mul(viewNoTranslation, worldPos));
    
    // Ensure skybox is always at the far plane
    output.position.z = output.position.w;
    
    return output;
}
