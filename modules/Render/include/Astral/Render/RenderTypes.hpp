#pragma once

#include "Astral/Core/Handle.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Astral::Render {

// ---------------------------------------------------------------------------
// Handles. All renderer objects are referenced by generational handles; a handle
// to a destroyed object is simply invalid (never a dangling pointer).
// ---------------------------------------------------------------------------
using MeshHandle     = Handle<struct MeshTag>;
using TextureHandle  = Handle<struct TextureTag>;
using MaterialHandle = Handle<struct MaterialTag>;
using InstanceHandle = Handle<struct InstanceTag>;
using SdfShapeHandle = Handle<struct SdfShapeTag>;
using LightHandle    = Handle<struct LightTag>;
using ViewHandle     = Handle<struct ViewTag>;

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f}; // xyz tangent, w handedness
    glm::vec2 uv{0.0f};
};

struct MeshDesc {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string debugName;
};

// ---------------------------------------------------------------------------
// Textures and materials
// ---------------------------------------------------------------------------
enum class TextureFormat : uint32_t {
    RGBA8_sRGB,   // colour data (base colour, emissive)
    RGBA8_Linear, // non-colour data (normals, metallic/roughness)
};

struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::RGBA8_sRGB;
    std::span<const uint8_t> pixels; // width * height * 4 bytes, tightly packed
    bool generateMips = true;
    std::string debugName;
};

// Metallic-roughness PBR (same model as glTF 2.0). Textures are optional; when set
// they are multiplied with the constant factors.
struct MaterialDesc {
    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emissive{0.0f};
    float normalScale = 1.0f;
    TextureHandle baseColorTexture;
    TextureHandle normalTexture;
    TextureHandle metallicRoughnessTexture; // glTF packing: G = roughness, B = metallic
    TextureHandle emissiveTexture;
};

// ---------------------------------------------------------------------------
// Signed distance field shapes
// ---------------------------------------------------------------------------
enum class SdfPrimitive : uint32_t {
    Sphere   = 0, // size.x = radius
    Box      = 1, // size.xyz = half extents, size.w = corner rounding
    Torus    = 2, // size.x = ring radius, size.y = tube radius
    Capsule  = 3, // size.x = radius, size.y = half segment length (along local Y)
    Cylinder = 4, // size.x = radius, size.y = half height, size.w = edge rounding
};
inline constexpr uint32_t kSdfPrimitiveCount = 5;

// How a shape combines with everything listed before it.
enum class SdfOperation : uint32_t {
    Union = 0,
    Subtract = 1,
    Intersect = 2,
    SmoothUnion = 3,
    SmoothSubtract = 4,
};
inline constexpr uint32_t kSdfOperationCount = 5;

struct SdfMaterial {
    glm::vec3 albedo{0.8f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    glm::vec3 emissive{0.0f};
};

struct SdfShapeDesc {
    SdfPrimitive primitive = SdfPrimitive::Sphere;
    SdfOperation operation = SdfOperation::Union;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 size{0.5f, 0.5f, 0.5f, 0.0f};
    float blend = 0.25f;  // smoothing radius for the smooth operations
    int32_t order = 0;    // evaluation order (lower first); ties keep creation order
    SdfMaterial material;
    uint32_t objectId = 0; // returned by picking; 0 = not pickable
};

// ---------------------------------------------------------------------------
// Lights and environment
// ---------------------------------------------------------------------------
enum class LightType : uint32_t { Directional = 0, Point = 1 };

struct LightDesc {
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;               // lux for directional, lumens-like scale for point
    glm::vec3 position{0.0f};             // point lights
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // directional: direction the light travels
    float range = 10.0f;                  // point lights: influence radius
    bool castShadows = true;              // directional only (point shadows: roadmap)
};

// Procedural sky. Drives the background, image-based lighting and the sun.
struct EnvironmentDesc {
    // Radiance of the sky dome. Keep it well below the sun (about 1/10 of its
    // irradiance) or shadows wash out.
    glm::vec3 zenithColor{0.12f, 0.24f, 0.55f};
    glm::vec3 horizonColor{0.40f, 0.50f, 0.65f};
    glm::vec3 groundColor{0.13f, 0.12f, 0.11f};
    float skyIntensity = 1.0f;
    float ambientIntensity = 1.0f;  // scales image-based lighting
    float exposure = 1.0f;          // EV multiplier applied before tonemapping
};

// ---------------------------------------------------------------------------
// Cameras and views
// ---------------------------------------------------------------------------
struct Camera {
    glm::vec3 position{0.0f, 1.5f, 5.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // looks down local -Z, +Y up
    float verticalFov = glm::radians(60.0f);
    float nearPlane = 0.05f;
    float farPlane = 500.0f;
};

enum class DebugView : uint32_t {
    None = 0,
    Albedo,
    Normals,
    Roughness,
    Metallic,
    Depth,
    Motion,
    ObjectId,
    Shadows,
    AmbientOcclusion,
    SdfBricks,
    SdfSteps,
};

struct ViewSettings {
    bool taa = true;
    bool shadows = true;
    bool sdfShadows = true;
    bool sdfAmbientOcclusion = true;
    float shadowDistance = 60.0f;   // directional shadow map coverage (metres)
    DebugView debugView = DebugView::None;
};

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------
struct PickResult {
    ViewHandle view;
    uint32_t x = 0, y = 0;
    uint32_t objectId = 0;      // 0 = nothing was hit
    glm::vec3 worldPosition{0.0f};
};

struct RenderStats {
    uint32_t meshInstances = 0;
    uint32_t sdfShapes = 0;
    uint32_t lights = 0;
    uint32_t sdfBricksUsed = 0;
    uint32_t sdfBrickCapacity = 0;
    uint32_t sdfBricksDropped = 0; // surface cells that did not fit in the atlas (last bake)
    uint32_t sdfLastBakeCells = 0;
    float gpuFrameMs = 0.0f;
};

} // namespace Astral::Render
