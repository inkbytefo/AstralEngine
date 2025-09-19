// shadow.slang
// Vertex shader for rendering to a depth-only shadow map.

struct VertexInput {
    float3 position : POSITION;
};

struct PushConstants {
    float4x4 lightSpaceMatrix;
    float4x4 modelMatrix;
};

[vk::push_constant]
PushConstants pushConstants;

float4 vs_main(VertexInput input) : SV_Position {
    // Transform vertex position to light's clip space.
    return mul(pushConstants.lightSpaceMatrix, mul(pushConstants.modelMatrix, float4(input.position, 1.0)));
}

// No fragment shader is needed as we are only writing to the depth buffer.
void fs_main() {}
