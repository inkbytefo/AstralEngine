// lighting_structs.slang
// Contains data structures for lighting calculations, shared between C++ and shaders.

#ifndef LIGHTING_STRUCTS_SLANG
#define LIGHTING_STRUCTS_SLANG

// Must match LightComponent::LightType in C++
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2

struct GPULight {
    float4x4 lightSpaceMatrix; // For shadow mapping

    float3 position;    // For Point/Spot lights (world space)
    int type;           // 0: Dir, 1: Point, 2: Spot

    float3 direction;   // For Directional/Spot lights (world space)
    float range;        // For Point/Spot lights

    float3 color;
    float intensity;

    // Attenuation for Point/Spot
    float constant;
    float linear;
    float quadratic;
    
    // Spot light parameters (cosines of angles)
    float innerConeCos;
    float outerConeCos;

    float castsShadows; // bool as float
    float _padding;
};

#endif // LIGHTING_STRUCTS_SLANG
