// AstralEngine - Vertex Attribute Definitions for Slang
// Defines standardized vertex input structures.

// Standard vertex attributes fed from the vertex buffer.
// Use this as the input for a vertex shader.
struct VertexInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float2 texCoord     : TEXCOORD0;
    float3 tangent      : TANGENT;
    float4 color        : COLOR0;
};

// Attributes for instanced rendering.
// These are appended to the VertexInput struct in the shader signature.
struct InstanceInput
{
    float4 row0 : INSTANCEMATRIX0;
    float4 row1 : INSTANCEMATRIX1;
    float4 row2 : INSTANCEMATRIX2;
    float4 row3 : INSTANCEMATRIX3;
};

// Attributes for skinned meshes.
struct SkinningInput
{
    int4 jointIndices   : JOINTS0;
    float4 jointWeights : WEIGHTS0;
};

// Helper to reconstruct the instance matrix.
float4x4 GetInstanceMatrix(InstanceInput input)
{
    return float4x4(input.row0, input.row1, input.row2, input.row3);
}
