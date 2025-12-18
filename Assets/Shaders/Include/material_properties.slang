// AstralEngine - Material Data Structures for Slang
// Defines data structures for material properties and PBR parameters.

// Alpha rendering modes.
enum AlphaMode
{
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

// Material properties, typically loaded into a constant buffer.
struct MaterialData
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float alphaCutoff;
    int alphaMode; // Corresponds to AlphaMode enum
    bool doubleSided;
    
    // Bindless texture indices
    uint albedoIndex;
    uint normalIndex;
    uint metallicIndex;
    uint roughnessIndex;
    uint aoIndex;
    uint emissiveIndex;
};

// A struct to hold sampled and computed PBR surface parameters.
struct PBRSurface
{
    float3 albedo;
    float metallic;
    float roughness;
    float ao;
    float3 normal;
    float3 emissive;
    float alpha;
};
