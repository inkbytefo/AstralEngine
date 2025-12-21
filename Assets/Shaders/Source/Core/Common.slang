// Common.slang
// Core definitions and constants for Astral Engine

#ifndef ASTRAL_COMMON_SLANG
#define ASTRAL_COMMON_SLANG

static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;
static const float HALF_PI = 1.57079632679;

// Standard Vertex Input for 3D meshes
struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 color : COLOR0;
};

// Standard Vertex Output for rasterization
struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

#endif
