#pragma once
// Loads assets by path, asynchronously, with hot reload. Assets are identified by AssetId — a stable
// hash of the path relative to the asset root — so ids are the same on every machine and run.
//
//     AssetManager assets({.root = "content", .renderer = &renderer, .jobs = &jobs});
//     AssetId robot = assets.load_model("robots/robot.glb");   // returns at once
//     ... every frame: assets.update();                         // finishes loads, hot-reloads
//     if (const Model* m = assets.model(robot)) { ... m->nodes ... }

#include <astral/assets/model.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/core/reflect.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// Stable identity of an asset: hash of its normalised path relative to the asset root.
struct AssetId {
    std::uint64_t value = 0;
    [[nodiscard]] bool valid() const noexcept { return value != 0; }
    bool operator==(const AssetId&) const = default;
    /// "Robots\\Robot.glb" and "robots/robot.glb" are the same asset (case and separators ignored).
    static AssetId from_path(std::string_view relative_path) noexcept;
};

enum class AssetState : std::uint8_t { Unknown, Loading, Ready, Failed };

/// A model ready to draw: renderer resources for each piece, and the node tree.
struct Model {
    struct Piece {
        MeshHandle mesh;
        MaterialHandle material;
    };
    struct Node {
        std::string name;
        Vec3 translation{};
        Quat rotation{};
        Vec3 scale{1, 1, 1};
        std::vector<Piece> pieces;
        std::vector<std::uint32_t> children;
    };
    std::vector<Node> nodes;
    std::vector<std::uint32_t> roots;
    /// Increments when a reload changed the node tree (entities spawned from the old one are stale).
    /// Reloads that keep the structure update meshes/materials/textures in place: version unchanged.
    std::uint32_t version = 1;
};

struct AssetManagerDesc {
    std::filesystem::path root = ".";
    Renderer* renderer = nullptr; ///< Null = headless: models load (data only) but have no GPU resources.
    JobSystem* jobs = nullptr;    ///< Null = load synchronously inside update().
    Logger* logger = nullptr;
    bool hot_reload = true;
    float reload_poll_seconds = 0.5f;
};

class AssetManager {
public:
    explicit AssetManager(const AssetManagerDesc& desc);
    ~AssetManager(); ///< Waits for loads in flight; destroys the renderer resources it created.
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /// Starts loading (if not loaded or loading already) and returns the id at once.
    AssetId load_model(std::string_view relative_path);
    [[nodiscard]] AssetState state(AssetId id) const noexcept;
    /// Null until Ready.
    [[nodiscard]] const Model* model(AssetId id) const noexcept;
    /// The load error of a Failed asset.
    [[nodiscard]] std::string_view error(AssetId id) const noexcept;
    [[nodiscard]] std::string_view path(AssetId id) const noexcept;

    /// Main thread, once per frame: finishes completed loads (GPU uploads) and checks watched files.
    void update();
    /// Blocks until nothing is loading (tests, loading screens).
    void wait_all();
    /// Number of reloads applied so far (hot reload).
    [[nodiscard]] std::uint32_t reloads() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Component: "this entity shows this model". The App loads it and spawns one child entity per node
/// piece under the entity (respawning if a reload changed the node tree). Serializable.
struct ModelRef {
    std::string path; ///< Relative to the asset root.
};

} // namespace astral

ASTRAL_REFLECT(astral::ModelRef, path)
