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

class RenderBridge final : public MaterialService, public ViewportService {
public:
    RenderBridge(World& world, AssetManager& assets, Logger& log, gpu::Device* device, gpu::Swapchain* swapchain,
                 Window* window, Renderer* renderer, const RenderBridgeConfig& config);
    ~RenderBridge() override;
    RenderBridge(const RenderBridge&) = delete;
    RenderBridge& operator=(const RenderBridge&) = delete;

    static void register_types(TypeRegistry& types);

    [[nodiscard]] BuiltinMeshes builtin_meshes() const noexcept { return builtin_meshes_; }
    [[nodiscard]] MaterialLibrary* material_library() noexcept { return material_library_.get(); }
    [[nodiscard]] gpu::Extent2D extent() const;
    [[nodiscard]] Vec2 screen_size() const;
    [[nodiscard]] Vec3 camera_position() const;
    [[nodiscard]] bool read_frame(std::vector<std::uint8_t>& rgba) const;

    MeshHandle create_mesh(const MeshData& data) override;
    MaterialHandle create_material(const MaterialDesc& desc) override;
    bool update_material(MaterialHandle material, const MaterialDesc& desc) override;
    MaterialHandle load_material(std::string_view path) override;
    [[nodiscard]] std::string_view shader_error() const noexcept override;

    void set_size(std::uint32_t width, std::uint32_t height) override;
    [[nodiscard]] TextureHandle texture() const noexcept override { return viewport_.target; }
    [[nodiscard]] gpu::Extent2D size() const noexcept override { return viewport_.actual; }
    void set_render_world(World* world) noexcept override { viewport_.alt_world = world; }
    [[nodiscard]] World* render_world() const noexcept override { return viewport_.alt_world; }
    void set_camera_override(const CameraData* camera) noexcept override;
    void set_clear_color(Vec4 rgba) noexcept override { viewport_.clear_color = rgba; }

    void update_materials(double elapsed);
    void sync_models();
    void sync_primitives();
    void shift_origin(Vec3 offset);
    void render_frame(const RenderFrameInput& input);

private:
    struct ViewportState {
        TextureHandle target{};
        gpu::Extent2D requested{};
        gpu::Extent2D actual{};
        World* alt_world = nullptr;
        bool has_camera_override = false;
        CameraData camera_override{};
        Vec4 clear_color{0.06f, 0.06f, 0.07f, 1.0f};
    };

    struct FrameScratch {
        std::vector<MeshInstance> draw_items;
        std::vector<std::pair<Entity, Mat4>> new_histories;
        std::vector<std::pair<std::int32_t, SdfShape>> sdf_sorted;
        std::vector<SdfShape> sdf_shapes;
        std::vector<PointLight> point_lights;
        std::vector<RectLight> rect_lights;
        std::vector<Decal> decals;
    };

    void ensure_viewport_target();
    void sync_primitives_in(World& w, bool incremental);
    void spawn_model(Entity owner, const Model& model);
    void extract_meshes(World& w);
    void extract_sdfs(World& w);
    void extract_lights(World& w);

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
    ViewportState viewport_{};
    FrameScratch scratch_;
};

} // namespace astral
