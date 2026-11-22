#include "render_bridge.hpp"

#include "../material_library.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/gpu/device.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/window.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>
#include <utility>

namespace astral {

namespace {

/// Internal component: the transform a MeshRenderer entity was drawn with last frame.
struct RenderHistory {
    Mat4 previous;
};
/// Internal: on a ModelRef entity, which model (and version) its pieces were spawned from.
struct SpawnedModel {
    AssetId id;
    std::uint32_t version = 0;
};
/// Internal: an entity generated from a model; destroyed with its owner, never saved.
struct ModelPiece {
    Entity owner;
};
/// Internal: this entity's MeshRenderer was made from its PrimitiveMesh.
struct PrimitiveDrawn {};

/// Builds the camera from the first active Camera entity (or a default one).
CameraData find_camera(World& world, float aspect) {
    CameraData data;
    data.position = {0, 2, 6};
    data.view = look_at(data.position, {0, 0, 0}, {0, 1, 0});
    data.projection = perspective(radians(70.0f), aspect, 0.1f, 1000.0f);
    bool found = false;
    world.each<const Camera, const WorldTransform>([&](const Camera& cam, const WorldTransform& w) {
        if (found || !cam.active) return;
        found = true;
        data.position = {w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
        data.view = inverse(w.matrix);
        data.projection = perspective(cam.fov_y, aspect, cam.near_plane, cam.far_plane);
    });
    return data;
}

} // namespace

void RenderBridge::register_types(TypeRegistry& types) { types.exclude<ModelPiece>(); }

RenderBridge::RenderBridge(World& world, AssetManager& assets, Logger& log, gpu::Device* device, gpu::Swapchain* swapchain,
                           Window* window, Renderer* renderer, const RenderBridgeConfig& config)
    : world_(world),
      assets_(assets),
      log_(log),
      device_(device),
      swapchain_(swapchain),
      window_(window),
      renderer_(renderer),
      width_(config.width),
      height_(config.height) {
    if (!renderer_) return;
    const MeshData sphere = config.offscreen ? make_sphere(64, 32) : make_sphere(32, 16);
    builtin_meshes_ = {renderer_->create_mesh(make_cube()), renderer_->create_mesh(sphere),
                       renderer_->create_mesh(make_plane(1.0f))};
    if (config.offscreen && device_) {
        offscreen_target_ = device_->create_texture({.width = width_,
                                                     .height = height_,
                                                     .format = gpu::Format::RGBA8Unorm,
                                                     .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        offscreen_readback_ = device_->create_buffer({.size = std::uint64_t{width_} * height_ * 4,
                                                      .usage = gpu::BufferUsage::TransferDst,
                                                      .memory = gpu::MemoryUsage::GpuToCpu});
    }
    material_library_ =
        std::make_unique<MaterialLibrary>(*renderer_, assets_, config.asset_root, config.watch_materials, log_);
}

RenderBridge::~RenderBridge() { material_library_.reset(); }

gpu::Extent2D RenderBridge::extent() const {
    if (swapchain_ && !offscreen_target_.valid()) return swapchain_->extent();
    return {width_, height_};
}

Vec2 RenderBridge::screen_size() const {
    const gpu::Extent2D e = extent();
    return {static_cast<float>(e.width), static_cast<float>(e.height)};
}

bool RenderBridge::read_frame(std::vector<std::uint8_t>& rgba) const {
    if (!offscreen_readback_.valid() || !device_) return false;
    rgba.resize(std::size_t{width_} * height_ * 4);
    return device_->read_buffer(offscreen_readback_, rgba.data(), rgba.size());
}

MeshHandle RenderBridge::create_mesh(const MeshData& data) { return renderer_ ? renderer_->create_mesh(data) : MeshHandle{}; }

MaterialHandle RenderBridge::create_material(const MaterialDesc& desc) {
    return renderer_ ? renderer_->create_material(desc) : MaterialHandle{};
}

bool RenderBridge::update_material(MaterialHandle material, const MaterialDesc& desc) {
    return renderer_ && renderer_->update_material(material, desc);
}

MaterialHandle RenderBridge::load_material(std::string_view path) {
    return material_library_ ? material_library_->get(path) : MaterialHandle{};
}

std::string_view RenderBridge::shader_error() const noexcept {
    return renderer_ ? renderer_->shader_error() : std::string_view{};
}

void RenderBridge::update_materials(double elapsed) {
    if (material_library_) material_library_->update(elapsed);
}

void RenderBridge::shift_origin(Vec3 offset) {
    world_.each<RenderHistory>([&](RenderHistory& h) {
        h.previous[3].x -= offset.x;
        h.previous[3].y -= offset.y;
        h.previous[3].z -= offset.z;
    });
    if (renderer_) renderer_->reset_history();
}

/// PrimitiveMesh -> MeshRenderer for new and edited components; drops the renderer when it is removed.
void RenderBridge::sync_primitives() {
    std::vector<Entity> gone;
    world_.query<const PrimitiveDrawn>().without<PrimitiveMesh>().each([&](Entity e, const PrimitiveDrawn&) {
        gone.push_back(e);
    });
    for (Entity e : gone) {
        world_.remove<PrimitiveDrawn>(e);
        world_.remove<MeshRenderer>(e);
    }
    world_.set_change_baseline(primitives_seen_);
    std::vector<std::pair<Entity, MeshRenderer>> updates;
    world_.query<const PrimitiveMesh>().changed<PrimitiveMesh>().each([&](Entity e, const PrimitiveMesh& p) {
        const MeshHandle mesh = p.shape == Primitive::Sphere ? builtin_meshes_.sphere
                                : p.shape == Primitive::Plane ? builtin_meshes_.plane
                                                              : builtin_meshes_.cube;
        MaterialHandle material;
        if (renderer_ && !p.material_file.empty()) {
            material = material_library_ ? material_library_->get(p.material_file) : MaterialHandle{}; // invalid: default material
        } else if (renderer_) {
            const MaterialDesc& d = p.material;
            std::string key = to_json(d, {}).dump();
            for (TextureHandle t : {d.base_color_texture, d.metallic_roughness_texture, d.normal_texture, d.emissive_texture}) {
                key += "/" + std::to_string(t.index) + "." + std::to_string(t.generation);
            }
            auto [it, inserted] = primitive_materials_.try_emplace(key);
            if (inserted) it->second = renderer_->create_material(d);
            material = it->second;
        }
        updates.emplace_back(e, MeshRenderer{mesh, material});
    });
    for (const auto& [e, r] : updates) {
        world_.add(e, r);
        world_.add(e, PrimitiveDrawn{});
    }
    primitives_seen_ = world_.tick();
    world_.advance_tick();
}

void RenderBridge::spawn_model(Entity owner, const Model& model) {
    // Depth-first over the node tree; each node is an entity parented to its parent node (roots to
    // the owner). A node with one piece draws it itself; several pieces get one child each.
    struct Item {
        std::uint32_t node;
        Entity parent;
    };
    std::vector<Item> stack;
    for (auto r = model.roots.rbegin(); r != model.roots.rend(); ++r) stack.push_back({*r, owner});
    while (!stack.empty()) {
        const Item item = stack.back();
        stack.pop_back();
        const Model::Node& n = model.nodes[item.node];
        const Entity e = world_.create(Transform{n.translation, n.rotation, n.scale}, Parent{item.parent}, ModelPiece{owner});
        if (n.pieces.size() == 1) {
            world_.add(e, MeshRenderer{n.pieces[0].mesh, n.pieces[0].material});
        } else {
            for (const Model::Piece& p : n.pieces) {
                world_.create(Transform{}, Parent{e}, ModelPiece{owner}, MeshRenderer{p.mesh, p.material});
            }
        }
        for (auto c = n.children.rbegin(); c != n.children.rend(); ++c) stack.push_back({*c, e});
    }
}

void RenderBridge::sync_models() {
    // Pieces whose owner is gone (destroyed, or its ModelRef removed / changed) are cleared first.
    std::vector<Entity> stale;
    world_.each<const ModelPiece>([&](Entity e, const ModelPiece& p) {
        const SpawnedModel* s = world_.get<const SpawnedModel>(p.owner);
        if (!s || !world_.has<ModelRef>(p.owner)) stale.push_back(e);
    });
    std::vector<std::pair<Entity, AssetId>> todo;
    world_.each<const ModelRef>([&](Entity e, const ModelRef& ref) {
        const AssetId id = assets_.load_model(ref.path); // idempotent
        const Model* model = assets_.model(id);
        const SpawnedModel* s = world_.get<const SpawnedModel>(e);
        if (model && (!s || s->id != id || s->version != model->version)) todo.emplace_back(e, id);
    });
    // Owners whose ModelRef was removed keep a SpawnedModel marker: drop it (their pieces are stale above).
    std::vector<Entity> orphans;
    world_.query<const SpawnedModel>().without<ModelRef>().each([&](Entity e, const SpawnedModel&) { orphans.push_back(e); });
    for (Entity e : orphans) world_.remove<SpawnedModel>(e);
    for (const auto& [owner, id] : todo) {
        world_.each<const ModelPiece>([&](Entity e, const ModelPiece& p) {
            if (p.owner == owner) stale.push_back(e);
        });
    }
    std::sort(stale.begin(), stale.end(), [](Entity a, Entity b) { return a.index < b.index; });
    stale.erase(std::unique(stale.begin(), stale.end()), stale.end());
    for (Entity e : stale) world_.destroy(e);
    for (const auto& [owner, id] : todo) {
        const Model* model = assets_.model(id);
        spawn_model(owner, *model);
        world_.add(owner, SpawnedModel{id, model->version});
    }
}

void RenderBridge::extract_meshes() {
    // Extract: one MeshInstance per entity, with last frame's transform for motion vectors.
    scratch_.draw_items.clear();
    scratch_.new_histories.clear();
    world_.each<const MeshRenderer, const WorldTransform>([&](Entity e, const MeshRenderer& r, const WorldTransform& w) {
        MeshInstance inst{.mesh = r.mesh, .material = r.material, .transform = w.matrix};
        if (RenderHistory* h = world_.get<RenderHistory>(e)) {
            inst.previous_transform = h->previous;
            h->previous = w.matrix;
        } else {
            scratch_.new_histories.emplace_back(e, w.matrix);
        }
        scratch_.draw_items.push_back(inst);
    });
    for (const auto& [e, matrix] : scratch_.new_histories) world_.add(e, RenderHistory{matrix});
}

void RenderBridge::extract_sdfs() {
    scratch_.sdf_sorted.clear();
    world_.each<const Sdf>([&](Entity e, const Sdf& s) {
        SdfShape shape = s.shape;
        if (const WorldTransform* w = world_.get<WorldTransform>(e)) {
            shape.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
            shape.rotation = rotation_of(w->matrix);
        }
        scratch_.sdf_sorted.emplace_back(s.order, shape);
    });
    std::stable_sort(scratch_.sdf_sorted.begin(), scratch_.sdf_sorted.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    scratch_.sdf_shapes.clear();
    for (const auto& [order, shape] : scratch_.sdf_sorted) scratch_.sdf_shapes.push_back(shape);
}

void RenderBridge::extract_lights() {
    scratch_.point_lights.clear();
    world_.each<const PointLight>([&](Entity e, const PointLight& l) {
        PointLight light = l;
        if (const WorldTransform* w = world_.get<WorldTransform>(e)) {
            light.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        }
        scratch_.point_lights.push_back(light);
    });
    scratch_.rect_lights.clear();
    world_.each<const RectLight>([&](Entity e, const RectLight& l) {
        RectLight light = l;
        if (const WorldTransform* w = world_.get<WorldTransform>(e)) {
            // Position from the entity; the panel's axes turn and scale with it.
            light.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
            const Vec4 r = w->matrix * Vec4{l.right.x, l.right.y, l.right.z, 0};
            const Vec4 u = w->matrix * Vec4{l.up.x, l.up.y, l.up.z, 0};
            light.right = {r.x, r.y, r.z};
            light.up = {u.x, u.y, u.z};
        }
        scratch_.rect_lights.push_back(light);
    });
}

void RenderBridge::render_frame(const RenderFrameInput& input) {
    const bool offscreen = offscreen_target_.valid();
    if (!renderer_ || (!offscreen && (!swapchain_ || (window_ && window_->minimized())))) return;
    gpu::TextureHandle image = offscreen ? offscreen_target_ : swapchain_->acquire();
    if (!image.valid()) return;

    extract_meshes();
    extract_sdfs();
    extract_lights();

    const gpu::Extent2D size = extent();
    const float aspect = size.height ? static_cast<float>(size.width) / static_cast<float>(size.height) : 1.0f;
    const CameraData camera = find_camera(world_, aspect);
    auto cmd = device_->begin_commands();
    renderer_->render(cmd, image, camera,
                      {.meshes = scratch_.draw_items,
                       .sdf = scratch_.sdf_shapes,
                       .lights = scratch_.point_lights,
                       .rect_lights = scratch_.rect_lights,
                       .sun = input.sun,
                       .environment = input.environment,
                       .debug_lines = input.debug_lines,
                       .ui = input.ui_layers,
                       .time = input.time,
                       .sky = input.sky,
                       .post = input.post});
    if (offscreen) {
        cmd.transition(image, gpu::ResourceState::TransferSrc);
        cmd.copy_texture_to_buffer(image, offscreen_readback_);
        device_->wait(device_->submit(cmd));
    } else {
        cmd.transition(image, gpu::ResourceState::Present);
        swapchain_->present(cmd);
    }
}

} // namespace astral
