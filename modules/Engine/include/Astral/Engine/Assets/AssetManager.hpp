#pragma once

#include "Astral/Render/Renderer.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Astral {

struct ModelPrimitive {
    Render::MeshHandle mesh;
    Render::MaterialHandle material;
    glm::mat4 localTransform{1.0f}; // relative to the model root (glTF node hierarchy, flattened)
};

struct Model {
    std::string path;
    std::vector<ModelPrimitive> primitives;
    glm::vec3 boundsMin{0.0f}, boundsMax{0.0f};
};

// Loads and caches GPU assets. Paths are relative to the asset root unless absolute.
//   "builtin:cube" / "builtin:sphere" / "builtin:plane"  procedural meshes
//   "models/ship.glb", "models/tree.gltf"                  glTF 2.0
class AssetManager {
public:
    AssetManager(Render::Renderer& renderer, std::filesystem::path root);
    ~AssetManager();

    // nullptr if the asset cannot be loaded (the reason is logged once).
    const Model* GetModel(const std::string& path);
    Render::TextureHandle GetTexture(const std::string& path, bool srgb);

    const std::filesystem::path& Root() const { return m_Root; }
    std::filesystem::path Resolve(const std::string& path) const;
    void Clear();

private:
    bool LoadGltf(const std::filesystem::path& file, Model& model);
    Model MakeBuiltin(const std::string& name);

    Render::Renderer& m_Renderer;
    std::filesystem::path m_Root;
    std::unordered_map<std::string, Model> m_Models;
    std::unordered_map<std::string, Render::TextureHandle> m_Textures;
    std::unordered_map<std::string, bool> m_Failed;
};

} // namespace Astral
