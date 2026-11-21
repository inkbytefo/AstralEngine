#pragma once

#include "Astral/Render/RenderTypes.hpp"

#include <memory>
#include <optional>

namespace Astral::Render {

class Renderer;

struct SdfRayHit {
    float distance = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    uint32_t objectId = 0;
};

// Everything that is drawn: mesh instances, SDF shapes, lights and the environment.
// The world is plain CPU data; the renderer uploads what changed when a view is rendered.
// Obtain it from Renderer::GetWorld().
class World {
public:
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Mesh instances ------------------------------------------------------
    InstanceHandle CreateInstance(MeshHandle mesh, MaterialHandle material, const glm::mat4& transform,
                                  uint32_t objectId = 0);
    void SetInstanceTransform(InstanceHandle instance, const glm::mat4& transform);
    void SetInstanceMaterial(InstanceHandle instance, MaterialHandle material);
    void SetInstanceVisible(InstanceHandle instance, bool visible);
    void SetInstanceCastsShadows(InstanceHandle instance, bool castsShadows);
    void DestroyInstance(InstanceHandle instance);

    // --- SDF shapes ----------------------------------------------------------
    SdfShapeHandle CreateSdfShape(const SdfShapeDesc& desc);
    void UpdateSdfShape(SdfShapeHandle shape, const SdfShapeDesc& desc);
    void DestroySdfShape(SdfShapeHandle shape);
    const SdfShapeDesc* GetSdfShape(SdfShapeHandle shape) const;

    // CPU evaluation of the combined SDF (same math as the GPU). Used for gameplay
    // queries and physics; does not need the GPU.
    float SdfDistance(const glm::vec3& point) const;
    std::optional<SdfRayHit> RaycastSdf(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;

    // Bounds of the region baked for rendering; shapes outside it are clipped.
    glm::vec3 SdfVolumeMin() const;
    glm::vec3 SdfVolumeMax() const;

    // --- Lights and environment ----------------------------------------------
    LightHandle CreateLight(const LightDesc& desc);
    void UpdateLight(LightHandle light, const LightDesc& desc);
    void DestroyLight(LightHandle light);

    void SetEnvironment(const EnvironmentDesc& environment);
    const EnvironmentDesc& GetEnvironment() const;

    uint32_t InstanceCount() const;
    uint32_t SdfShapeCount() const;
    uint32_t LightCount() const;

    struct Impl;
    Impl& GetImpl() { return *m_Impl; }
    const Impl& GetImpl() const { return *m_Impl; }

private:
    friend class Renderer;
    explicit World(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Astral::Render
