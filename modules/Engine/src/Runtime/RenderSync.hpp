#pragma once

#include "Astral/Engine/Assets/AssetManager.hpp"
#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Render/Renderer.hpp"

#include <unordered_map>
#include <vector>

namespace Astral {

// Mirrors ECS components into the renderer's World. Only what changed is pushed:
// SDF shapes in particular are compared, because any update re-bakes their region.
class RenderSync {
public:
    RenderSync(Render::Renderer& renderer, AssetManager& assets);
    ~RenderSync();

    void Sync(Scene& scene);  // call after the scene's world transforms are up to date
    void Clear();             // removes everything this object created in the World

private:
    struct MeshEntry {
        std::string mesh;
        std::vector<Render::InstanceHandle> instances;
        Render::MaterialHandle overrideMaterial;
        MaterialParams lastMaterial;
        bool hasOverride = false;
        bool seen = false;
    };
    struct SdfEntry {
        Render::SdfShapeHandle handle;
        Render::SdfShapeDesc last;
        bool seen = false;
    };
    struct LightEntry {
        Render::LightHandle handle;
        Render::LightDesc last;
        bool seen = false;
    };

    void SyncMeshes(Scene& scene);
    void SyncSdfShapes(Scene& scene);
    void SyncLights(Scene& scene);
    void DestroyMesh(MeshEntry& entry);

    Render::Renderer& m_Renderer;
    AssetManager& m_Assets;
    std::unordered_map<uint64_t, MeshEntry> m_Meshes;   // keyed by Entity::Raw()
    std::unordered_map<uint64_t, SdfEntry> m_Sdfs;
    std::unordered_map<uint64_t, LightEntry> m_Lights;
    Render::EnvironmentDesc m_LastEnvironment{};
    bool m_EnvironmentSet = false;
};

} // namespace Astral
