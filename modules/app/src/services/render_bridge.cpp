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
#include <bit>
#include <cassert>
#include <utility>

namespace astral {

namespace {

/// Internal: on a ModelRef entity, which model (and version) its pieces were spawned from.
struct SpawnedModel {
    AssetId id;
    std::uint32_t version = 0;
    std::vector<ModelMaterialOverride> material_overrides;
};
/// Internal: an entity generated from a model; destroyed with its owner, never saved.
struct ModelPiece {
    Entity owner;
};
/// Internal: this entity's MeshRenderer was made from its PrimitiveMesh.
struct PrimitiveDrawn {};

std::uint32_t float_bits(float v) noexcept {
    if (v == 0.0f) v = 0.0f; // normalize -0.0f to +0.0f
    return std::bit_cast<std::uint32_t>(v);
}

std::uint64_t fnv1a_mix(std::uint64_t h, std::uint32_t word) noexcept {
    h ^= word;
    h *= 1099511628211ull;
    return h;
}

std::uint64_t hash_material_extras(const MaterialDesc& d) noexcept {
    std::uint64_t extra = 14695981039346656037ull;
    extra = fnv1a_mix(extra, static_cast<std::uint32_t>(d.params.size()));
    for (float p : d.params) extra = fnv1a_mix(extra, float_bits(p));
    extra = fnv1a_mix(extra, static_cast<std::uint32_t>(d.textures.size()));
    for (const auto& pt : d.textures) {
        extra = fnv1a_mix(extra, pt.offset);
        extra = fnv1a_mix(extra, pt.texture.index);
        extra = fnv1a_mix(extra, pt.texture.generation);
        extra = fnv1a_mix(extra, pt.normal ? 1u : 0u);
    }
    return extra;
}

PrimitiveMaterialKey make_primitive_material_key(const MaterialDesc& d) noexcept {
    PrimitiveMaterialKey key{};
    std::size_t idx = 0;
    auto push_u32 = [&](std::uint32_t w) { assert(idx < key.words.size()); key.words[idx++] = w; };
    auto push_f32 = [&](float f) { push_u32(float_bits(f)); };
    auto push_vec3 = [&](Vec3 v) {
        push_f32(v.x);
        push_f32(v.y);
        push_f32(v.z);
    };
    auto push_vec4 = [&](Vec4 v) {
        push_f32(v.x);
        push_f32(v.y);
        push_f32(v.z);
        push_f32(v.w);
    };
    auto push_tex = [&](TextureHandle t) {
        push_u32(t.index);
        push_u32(t.generation);
    };

    push_vec4(d.base_color);
    push_f32(d.roughness);
    push_f32(d.metallic);
    push_vec3(d.emissive);
    push_tex(d.base_color_texture);
    push_tex(d.metallic_roughness_texture);
    push_tex(d.normal_texture);
    push_tex(d.emissive_texture);
    push_f32(d.normal_scale);
    push_u32(static_cast<std::uint32_t>(d.shading_model));
    push_f32(d.clearcoat.strength);
    push_f32(d.clearcoat.roughness);
    push_vec3(d.sheen.color);
    push_f32(d.sheen.roughness);
    push_f32(d.anisotropy.strength);
    push_f32(d.anisotropy.rotation);
    push_f32(d.transmission.strength);
    push_f32(d.transmission.ior);
    push_f32(d.transmission.thickness);
    push_vec3(d.transmission.attenuation_color);
    push_f32(d.transmission.attenuation_distance);
    push_f32(d.subsurface.strength);
    push_vec3(d.subsurface.color);
    push_f32(d.subsurface.radius);
    push_tex(d.detail.albedo_texture);
    push_tex(d.detail.normal_texture);
    push_tex(d.detail.height_texture);
    push_f32(d.detail.uv_scale);
    push_f32(d.detail.normal_scale);
    push_f32(d.detail.parallax_scale);
    push_u32(static_cast<std::uint32_t>(d.mapping.projection));
    push_f32(d.mapping.uv_scale.x);
    push_f32(d.mapping.uv_scale.y);
    push_f32(d.mapping.uv_offset.x);
    push_f32(d.mapping.uv_offset.y);
    push_f32(d.mapping.uv_rotation);
    push_f32(d.mapping.tile_size_m);
    push_f32(d.mapping.blend_sharpness);
    push_u32(d.mapping.world_space ? 1u : 0u);
    push_f32(d.wetness);
    push_u32(d.shader.index);
    push_u32(d.shader.generation);

    const std::uint64_t extra = hash_material_extras(d);
    push_u32(static_cast<std::uint32_t>(extra & 0xFFFFFFFFu));
    push_u32(static_cast<std::uint32_t>(extra >> 32));
    return key;
}

} // namespace

std::size_t PrimitiveMaterialKeyHash::operator()(const PrimitiveMaterialKey& k) const noexcept {
    std::uint64_t h = 14695981039346656037ull;
    for (std::uint32_t w : k.words) h = fnv1a_mix(h, w);
    return static_cast<std::size_t>(h);
}

Vec3 RenderBridge::camera_position() const {
    if (viewport_.has_camera_override) return viewport_.camera_override.position;
    World& active = viewport_.alt_world ? *viewport_.alt_world : world_;
    return find_active_camera(active, 1.0f).position;
}

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

RenderBridge::~RenderBridge() {
    if (renderer_) {
        if (viewport_.target.valid()) renderer_->destroy(viewport_.target);
        for (const auto& [key, handle] : primitive_materials_) {
            if (handle.valid()) renderer_->destroy(handle);
        }
        primitive_materials_.clear();
        if (builtin_meshes_.cube.valid()) renderer_->destroy(builtin_meshes_.cube);
        if (builtin_meshes_.sphere.valid()) renderer_->destroy(builtin_meshes_.sphere);
        if (builtin_meshes_.plane.valid()) renderer_->destroy(builtin_meshes_.plane);
    }
    material_library_.reset();
    if (device_) {
        if (offscreen_target_.valid()) device_->destroy(offscreen_target_);
        if (offscreen_readback_.valid()) device_->destroy(offscreen_readback_);
    }
}

void RenderBridge::set_size(std::uint32_t width, std::uint32_t height) { viewport_.requested = {width, height}; }

void RenderBridge::set_camera_override(const CameraData* camera) noexcept {
    if (camera) {
        viewport_.has_camera_override = true;
        viewport_.camera_override = *camera;
    } else {
        viewport_.has_camera_override = false;
    }
}

void RenderBridge::ensure_viewport_target() {
    if (!renderer_) return;
    const gpu::Extent2D req = viewport_.requested;
    if (req.width == 0 || req.height == 0) {
        if (viewport_.target.valid()) {
            renderer_->destroy(viewport_.target);
            viewport_.target = {};
            viewport_.actual = {};
        }
        return;
    }
    if (!viewport_.target.valid()) {
        viewport_.target = renderer_->create_render_target(req.width, req.height);
        if (viewport_.target.valid()) viewport_.actual = req;
    } else if (viewport_.actual.width != req.width || viewport_.actual.height != req.height) {
        if (renderer_->resize_render_target(viewport_.target, req.width, req.height)) {
            viewport_.actual = req;
            renderer_->reset_history();
        }
    }
}

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
    shift_render_history(world_, offset);
    if (renderer_) renderer_->reset_history();
}

void RenderBridge::sync_primitives() { sync_primitives_in(world_, true); }

void RenderBridge::sync_primitives_in(World& w, bool incremental) {
    sync_scratch_.gone.clear();
    w.query<const PrimitiveDrawn>().without<PrimitiveMesh>().each([&](Entity e, const PrimitiveDrawn&) {
        sync_scratch_.gone.push_back(e);
    });
    for (Entity e : sync_scratch_.gone) {
        w.remove<PrimitiveDrawn>(e);
        w.remove<MeshRenderer>(e);
    }

    sync_scratch_.updates.clear();
    auto collect = [&](Entity e, const PrimitiveMesh& p) {
        const MeshHandle mesh = p.shape == Primitive::Sphere ? builtin_meshes_.sphere
                                : p.shape == Primitive::Plane ? builtin_meshes_.plane
                                                              : builtin_meshes_.cube;
        MaterialHandle material;
        if (renderer_ && !p.material_file.empty()) {
            material = material_library_ ? material_library_->get(p.material_file) : MaterialHandle{};
        } else if (renderer_) {
            const PrimitiveMaterialKey key = make_primitive_material_key(p.material);
            auto [it, inserted] = primitive_materials_.try_emplace(key);
            if (inserted) it->second = renderer_->create_material(p.material);
            material = it->second;
        }
        const auto* current = w.get<const MeshRenderer>(e);
        if (current && current->mesh == mesh && current->material == material && w.has<PrimitiveDrawn>(e)) return;
        sync_scratch_.updates.emplace_back(e, MeshRenderer{mesh, material});
    };

    if (incremental) {
        w.set_change_baseline(primitives_seen_);
        w.query<const PrimitiveMesh>().changed<PrimitiveMesh>().each(collect);
    } else {
        w.each<const PrimitiveMesh>(collect);
    }

    for (const auto& [e, r] : sync_scratch_.updates) {
        w.add(e, r);
        w.add(e, PrimitiveDrawn{});
    }

    if (incremental) {
        primitives_seen_ = w.tick();
        w.advance_tick();
    }
}

std::vector<MaterialHandle> RenderBridge::resolve_model_overrides(const Model& model, const ModelRef& ref) {
    std::vector<MaterialHandle> overrides(model.material_slots.size());
    if (!material_library_) return overrides;
    for (const auto& value : ref.material_overrides) {
        if (value.material_file.empty()) continue;
        const auto warn = [&](std::string message) { log_.log(LogLevel::Warn, ref.path + ": " + message); };
        if (value.slot.empty() || std::count(model.material_slots.begin(), model.material_slots.end(), value.slot) != 1) {
            warn("no unique material slot named '" + value.slot + "'");
            continue;
        }
        if (std::count_if(ref.material_overrides.begin(), ref.material_overrides.end(),
                          [&](const auto& entry) { return entry.slot == value.slot; }) != 1) {
            warn("multiple overrides for material slot '" + value.slot + "'");
            continue;
        }
        const auto slot = std::find(model.material_slots.begin(), model.material_slots.end(), value.slot);
        overrides[static_cast<std::size_t>(slot - model.material_slots.begin())] = material_library_->get(value.material_file);
    }
    return overrides;
}

void RenderBridge::spawn_model(Entity owner, const Model& model, const ModelRef& ref) {
    const auto overrides = resolve_model_overrides(model, ref);
    const auto material_for = [&](const Model::Piece& piece) {
        if (piece.material_index >= 0 && static_cast<std::size_t>(piece.material_index) < overrides.size()) {
            const auto override = overrides[static_cast<std::size_t>(piece.material_index)];
            if (override.valid()) return override;
        }
        return piece.material;
    };
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
            world_.add(e, MeshRenderer{n.pieces[0].mesh, material_for(n.pieces[0])});
        } else {
            for (const Model::Piece& p : n.pieces) {
                world_.create(Transform{}, Parent{e}, ModelPiece{owner}, MeshRenderer{p.mesh, material_for(p)});
            }
        }
        for (auto c = n.children.rbegin(); c != n.children.rend(); ++c) stack.push_back({*c, e});
    }
}

void RenderBridge::sync_models() {
    if (world_.count<ModelRef>() == 0 && world_.count<ModelPiece>() == 0 && world_.count<SpawnedModel>() == 0) return;

    sync_scratch_.todo_models.clear();
    sync_scratch_.todo_owners.clear();
    world_.each<const ModelRef>([&](Entity e, const ModelRef& ref) {
        const AssetId id = assets_.load_model(ref.path);
        const Model* model = assets_.model(id);
        const SpawnedModel* s = world_.get<const SpawnedModel>(e);
        if (model && (!s || s->id != id || s->version != model->version || s->material_overrides != ref.material_overrides)) {
            sync_scratch_.todo_models.emplace_back(e, id);
            sync_scratch_.todo_owners.insert(to_user_data(e));
        }
    });

    sync_scratch_.orphan_models.clear();
    world_.query<const SpawnedModel>().without<ModelRef>().each(
        [&](Entity e, const SpawnedModel&) { sync_scratch_.orphan_models.push_back(e); });
    for (Entity e : sync_scratch_.orphan_models) world_.remove<SpawnedModel>(e);

    sync_scratch_.stale_pieces.clear();
    world_.each<const ModelPiece>([&](Entity e, const ModelPiece& p) {
        const SpawnedModel* s = world_.get<const SpawnedModel>(p.owner);
        if (!s || !world_.has<ModelRef>(p.owner) || sync_scratch_.todo_owners.contains(to_user_data(p.owner))) {
            sync_scratch_.stale_pieces.push_back(e);
        }
    });

    for (Entity e : sync_scratch_.stale_pieces) {
        if (world_.alive(e)) world_.destroy(e);
    }

    for (const auto& [owner, id] : sync_scratch_.todo_models) {
        const Model* model = assets_.model(id);
        const ModelRef ref = *world_.get<const ModelRef>(owner);
        spawn_model(owner, *model, ref);
        world_.add(owner, SpawnedModel{id, model->version, ref.material_overrides});
    }
}

void RenderBridge::render_frame(const RenderFrameInput& input) {
    const bool offscreen = offscreen_target_.valid();
    if (!renderer_ || (!offscreen && (!swapchain_ || (window_ && window_->minimized())))) return;
    ensure_viewport_target();
    const gpu::TextureHandle image = offscreen ? offscreen_target_ : swapchain_->acquire();
    if (!image.valid()) return;

    World& active_world = viewport_.alt_world ? *viewport_.alt_world : world_;
    if (viewport_.alt_world) {
        update_transforms(active_world);
        sync_primitives_in(active_world, false);
    }
    extract_render_scene(active_world, scratch_);

    auto cmd = device_->begin_commands();
    const gpu::TextureHandle vp_native =
        viewport_.target.valid() ? renderer_->native_target(viewport_.target) : gpu::TextureHandle{};
    const gpu::Extent2D target_size = vp_native.valid() ? viewport_.actual : extent();
    const float aspect =
        target_size.height ? static_cast<float>(target_size.width) / static_cast<float>(target_size.height) : 1.0f;
    const CameraData camera =
        viewport_.has_camera_override ? viewport_.camera_override : find_active_camera(active_world, aspect);

    RenderScene scene{
        .meshes = scratch_.draw_items,
        .sdf = scratch_.sdf_shapes,
        .lights = scratch_.point_lights,
        .rect_lights = scratch_.rect_lights,
        .decals = scratch_.decals,
        .sun = input.sun,
        .environment = input.environment,
        .debug_lines = input.debug_lines,
        .ui = vp_native.valid() ? std::span<const UiLayer>{} : input.ui_layers,
        .time = input.time,
        .sky = input.sky,
        .post = input.post,
        .world_origin = input.world_origin,
        .smoke = scratch_.smoke_volumes,
        .particles = scratch_.particle_bursts,
    };

    if (vp_native.valid()) {
        renderer_->render(cmd, vp_native, camera, scene);
        cmd.transition(vp_native, gpu::ResourceState::ShaderRead);
        renderer_->render_ui(cmd, image, input.ui_layers, viewport_.clear_color);
    } else {
        renderer_->render(cmd, image, camera, scene);
    }

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
