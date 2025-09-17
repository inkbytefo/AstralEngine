// PBR structures for SLANG shaders
// Author: inkbytefo
// Project: AstralEngine
// Description: Physically Based Rendering structures

// Material structure for PBR
struct PBRMaterial
{
    float3 albedo;
    float metallic;
    float roughness;
    float ao;
    float3 normal;
    float3 emissive;
    float alpha;
};

// PBR surface properties
struct PBSurface
{
    float3 albedo;
    float3 normal;
    float3 viewDir;
    float3 F0;
    float roughness;
    float metallic;
    float ao;
    float3 emissive;
    float alpha;
};
