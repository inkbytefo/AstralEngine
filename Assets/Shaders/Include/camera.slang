// AstralEngine - Camera Data Definition for Slang
// Defines the data structure for the main camera constant buffer.

// Contains all camera-related data passed from the CPU.
// This struct is intended to be used within a ConstantBuffer.
struct CameraData
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float3 position;
    float nearPlane;
    float farPlane;
};
