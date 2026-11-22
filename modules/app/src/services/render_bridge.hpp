#pragma once
// Internal to Astral::App: bridges ECS render components, models, primitives, materials and offscreen capture to Renderer.

#include <astral/app/app.hpp>
#include <astral/gpu/types.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astral {

class AssetManager;
class Logger;
class MaterialLibrary;
struct Model;
class Renderer;
class TypeRegistry;
class Window;
class World;
namespace gpu {
class Device;
class Swapchain;
} // namespace gpu

struct RenderBridgeConfig {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool offscreen = false;
    std::filesystem::path asset_root;
    bool watch_materials = false;
};

struct RenderFrameInput {
    const DirectionalLight& sun;
    const Environment& environment;
    std::span<const DebugLine> debug_lines;
    std::span<const UiLayer> ui_layers;
    float time = 0.0f;
    MaterialHandle sky{};
    std::span<const MaterialHandle> post;
};

class RenderBridge {
public:
    RenderBridge(World& world, AssetManager& assets, Logger& log, gpu::Device* device, gpu::Swapchain* swapchain,
                 Window* window, Renderer* renderer, const RenderBridgeConfig& config);
    ~RenderBridge();
    RenderBridge(const RenderBridge&) = delete;
    RenderBridge& operator=(const RenderBridge&) = delete;

    static void register_types(TypeRegistry& types);

    [[nodiscard]] BuiltinMeshes builtin_meshes() const noexcept { return builtin_meshes_; }
    [[nodiscard]] MaterialLibrary* material_library() noexcept { return material_library_.get(); }
    [[nodiscard]] gpu::Extent2D extent() const;
    [[nodiscard]] Vec2 screen_size() const;
    [[nodiscard]] bool read_frame(std::vector<std::uint8_t>& rgba) const;

    MeshHandle create_mesh(const MeshData& data);
    MaterialHandle create_material(const MaterialDesc& desc);
    bool update_material(MaterialHandle material, const MaterialDesc& desc);
    MaterialHandle load_material(std::string_view path);
    [[nodiscard]] std::string_view shader_error() const noexcept;

    void update_materials(double elapsed);
    void sync_models();
    void sync_primitives();
    void shift_origin(Vec3 offset);
    void render_frame(const RenderFrameInput& input);

private:
    struct FrameScratch {
        std::vector<MeshInstance> draw_items;
        std::vector<std::pair<Entity, Mat4>> new_histories;
        std::vector<std::pair<std::int32_t, SdfShape>> sdf_sorted;
        std::vector<SdfShape> sdf_shapes;
        std::vector<PointLight> point_lights;
        std::vector<RectLight> rect_lights;
    };

    void spawn_model(Entity owner, const Model& model);
    void extract_meshes();
    void extract_sdfs();
    void extract_lights();

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    gpu::Device* device_;
    gpu::Swapchain* swapchain_;
    Window* window_;
    Renderer* renderer_;
    std::uint32_t width_;
    std::uint32_t height_;
    BuiltinMeshes builtin_meshes_{};
    gpu::TextureHandle offscreen_target_{};
    gpu::BufferHandle offscreen_readback_{};
    std::unique_ptr<MaterialLibrary> material_library_;
    Tick primitives_seen_ = 0;
    std::unordered_map<std::string, MaterialHandle> primitive_materials_;
    FrameScratch scratch_;
};

} // namespace astral
