#ifndef SDF_SCENE_GLSL
#define SDF_SCENE_GLSL

#extension GL_GOOGLE_include_directive : enable
#include "Astral/Geometry/SDFKernel.inl"

#define MAX_EDITS 256

struct SDFPrimitiveRecord {
    mat4 invTransform;          // 64 bytes - transforms world ray/point to canonical local space
    vec4 dimensions;            // 16 bytes - shape parameters (radius, extents, etc.)
    vec4 albedoRoughness;       // 16 bytes - rgb: albedo, w: roughness
    vec4 metallicParams;        // 16 bytes - x: metallic, y: blendFactor, z: conservativeScale, w: isDynamic
    uint primitiveType;         // 4 bytes
    uint operation;             // 4 bytes
    uint csgOrder;              // 4 bytes
    uint surfaceId;             // 4 bytes
};

// Evaluates a single primitive record in world space using unified SDFKernel
float evaluatePrimitiveDistance(vec3 worldPos, SDFPrimitiveRecord rec) {
    vec4 lp4 = rec.invTransform * vec4(worldPos, 1.0);
    vec3 lp = lp4.xyz;
    float localDist = evalPrimitiveDistance(lp, rec.primitiveType, rec.dimensions);
    return localDist * rec.metallicParams.z; // Conservative distance scale
}

#endif // SDF_SCENE_GLSL
