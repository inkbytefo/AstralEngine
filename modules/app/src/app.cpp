#include <astral/app/app.hpp>

#include <astral/core/clock.hpp>
#include <astral/gpu/device.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/window.hpp>
#include <astral/world/transform.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <vector>

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
} // namespace

struct App::Impl {
    AppDesc desc;
    Logger log;
    JobSystem jobs;
    World world;
    Input input;
    Time time;
    Clock clock;

    std::unique_ptr<Window> window;
    gpu::SurfaceSource surface;
    std::unique_ptr<gpu::Device> device;
    std::unique_ptr<gpu::Swapchain> swapchain;
    std::unique_ptr<Renderer> renderer;

    std::unique_ptr<PhysicsWorld> physics;
    std::unique_ptr<AssetManager> assets;
    std::unique_ptr<PrefabLibrary> prefabs;
    Tick primitives_seen = 0;
    std::unordered_map<std::string, MaterialHandle> primitive_materials; // shared by identical descriptions
    gpu::TextureHandle offscreen_target;
    gpu::BufferHandle offscreen_readback;
    float physics_accumulator = 0.0f;
    std::vector<ContactEvent> frame_contacts;
    struct BodyLink {
        Entity entity;
        BodyHandle body;
        Vec3 previous_position{}; // kinematic bodies: pose at the start of the frame
        Quat previous_rotation{};
    };
    struct CharacterLink {
        Entity entity;
        CharacterHandle character;
    };
    std::vector<BodyLink> body_links;
    std::vector<CharacterLink> character_links;

    std::unique_ptr<Context> context;
    std::array<std::vector<std::unique_ptr<System>>, 4> systems;
    std::vector<MeshInstance> draw_items; // scratch, reused every frame
    std::vector<std::pair<Entity, Mat4>> new_histories;
    std::vector<std::pair<std::int32_t, SdfShape>> sdf_sorted;
    std::vector<SdfShape> sdf_shapes;
    std::vector<PointLight> point_lights;
    std::vector<RectLight> rect_lights;

    bool started = false;
    bool in_fixed_update = false;
    void sync_physics_bodies();
    void sync_models();
    void spawn_model(Entity owner, const Model& model);
    void sync_primitives();
    void log_problems(std::string_view what, const std::vector<std::string>& problems);
    void step_physics(Game& game, float delta);

    explicit Impl(const AppDesc& d) : desc(d), jobs(d.worker_threads) {}
    bool start(Game& game);
    void frame(Game& game, float delta);
};

void Context::register_system(Phase phase, std::unique_ptr<System> system) {
    app_.impl_->systems[static_cast<std::size_t>(phase)].push_back(std::move(system));
}

App::App(const AppDesc& desc) : impl_(std::make_unique<Impl>(desc)) {
    auto& m = *impl_;
    m.log.add_sink(std::make_shared<ConsoleSink>());
    m.context.reset(new Context(m.world, m.input, m.time, m.log, m.jobs, *this));
    TypeRegistry& types = m.context->types;
    register_world_types(types);
    types.add<Camera>();
    types.add<PointLight>();
    types.add<RectLight>();
    types.add<Sdf>();
    types.add<ModelRef>();
    types.add<PrimitiveMesh>();
    types.exclude<ModelPiece>();
    m.physics = std::make_unique<PhysicsWorld>(PhysicsDesc{.logger = &m.log});
    m.context->physics = m.physics.get();
    m.time.fixed_delta = 1.0f / std::max(desc.physics_rate, 1.0f);

    if (!desc.headless && desc.offscreen) {
        m.device = gpu::Device::create({.app_name = desc.title, .validation = desc.validation, .logger = &m.log});
        if (m.device) {
            m.renderer = std::make_unique<Renderer>(*m.device, gpu::Format::RGBA8Unorm, m.desc.render);
            m.context->meshes = {m.renderer->create_mesh(make_cube()), m.renderer->create_mesh(make_sphere(64, 32)),
                                 m.renderer->create_mesh(make_plane(1.0f))};
            m.offscreen_target = m.device->create_texture({.width = desc.width, .height = desc.height,
                                                           .format = gpu::Format::RGBA8Unorm,
                                                           .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
            m.offscreen_readback = m.device->create_buffer({.size = std::uint64_t{desc.width} * desc.height * 4,
                                                            .usage = gpu::BufferUsage::TransferDst,
                                                            .memory = gpu::MemoryUsage::GpuToCpu});
        } else {
            m.log.error("App: no usable GPU for offscreen rendering; running without rendering");
        }
    } else if (!desc.headless) {
        m.window = Window::create({.title = desc.title, .width = desc.width, .height = desc.height});
        if (!m.window) {
            m.log.error("App: could not create a window; running headless");
        } else {
            m.surface = m.window->surface_source();
            m.device = gpu::Device::create({.app_name = desc.title, .validation = desc.validation,
                                            .surface = &m.surface, .logger = &m.log});
            if (m.device) {
                m.swapchain = gpu::Swapchain::create(*m.device, m.surface, {.vsync = desc.vsync});
                if (m.swapchain) {
                    m.renderer = std::make_unique<Renderer>(*m.device, m.swapchain->format(), m.desc.render);
                    m.context->meshes = {m.renderer->create_mesh(make_cube()),
                                         m.renderer->create_mesh(make_sphere(32, 16)),
                                         m.renderer->create_mesh(make_plane(1.0f))};
                }
            } else {
                m.log.error("App: no usable GPU; running without rendering");
            }
        }
    }
    m.context->window = m.window.get();
    m.context->device = m.device.get();
    m.context->renderer = m.renderer.get();
    m.assets = std::make_unique<AssetManager>(AssetManagerDesc{.root = std::filesystem::path(desc.asset_root),
                                                               .renderer = m.renderer.get(), .jobs = &m.jobs,
                                                               .logger = &m.log, .hot_reload = desc.hot_reload});
    m.context->assets = m.assets.get();
    m.prefabs = std::make_unique<PrefabLibrary>(types, [&m](std::string_view path) {
        const AssetId id = m.assets->load_document(path);
        if (const Document* d = m.assets->document(id)) return SourceDocument{.json = &d->json, .version = d->version, .error = {}};
        return SourceDocument{.json = nullptr, .version = 0, .error = std::string(m.assets->error(id))};
    });
    m.context->prefabs = m.prefabs.get();
}

App::~App() {
    auto& m = *impl_;
    if (m.device) m.device->wait_idle();
    // Systems may release GPU resources in their destructors: they go first, while the
    // renderer and device still exist. Then the services, device last, window after it.
    for (auto& phase : m.systems) phase.clear();
    m.context->physics = nullptr;
    m.physics.reset();
    m.context->prefabs = nullptr;
    m.prefabs.reset();
    m.context->assets = nullptr;
    m.assets.reset(); // owns renderer resources: before the renderer
    m.renderer.reset();
    m.swapchain.reset();
    m.device.reset();
}

Context& App::context() noexcept { return *impl_->context; }

MeshHandle Context::create_mesh(const MeshData& data) { return renderer ? renderer->create_mesh(data) : MeshHandle{}; }

MaterialHandle Context::create_material(const MaterialDesc& desc) {
    return renderer ? renderer->create_material(desc) : MaterialHandle{};
}

bool Context::update_material(MaterialHandle material, const MaterialDesc& desc) {
    return renderer && renderer->update_material(material, desc);
}

std::span<const ContactEvent> Context::contacts() const noexcept {
    // Inside FixedUpdate: the previous physics step (possibly last frame's final step);
    // elsewhere: every step of this frame.
    const auto& m = *app_.impl_;
    return m.in_fixed_update ? m.physics->contacts() : std::span<const ContactEvent>(m.frame_contacts);
}
World& App::world() noexcept { return impl_->world; }

bool App::read_frame(std::vector<std::uint8_t>& rgba) const {
    const auto& m = *impl_;
    if (!m.offscreen_readback.valid()) return false;
    rgba.resize(std::size_t{m.desc.width} * m.desc.height * 4);
    return m.device->read_buffer(m.offscreen_readback, rgba.data(), rgba.size());
}

void App::Impl::log_problems(std::string_view what, const std::vector<std::string>& problems) {
    for (const std::string& p : problems) log.log(LogLevel::Error, std::string(what) + ": " + p);
}

LoadResult Context::load_scene(std::string_view path) {
    auto& m = *app_.impl_;
    const AssetId id = assets->load_document(path);
    const Document* doc = assets->document(id);
    if (!doc) {
        LoadResult failed;
        failed.errors.push_back(std::string(assets->error(id)));
        m.log_problems("load_scene", failed.errors);
        return failed;
    }
    LoadResult result = astral::load_scene(world, *prefabs, doc->json);
    LoadContext settings;
    settings.errors = &result.errors;
    if (const Json* j = doc->json.find("sun")) {
        settings.path = "sun";
        from_json(*j, sun, settings);
    }
    if (const Json* j = doc->json.find("environment")) {
        settings.path = "environment";
        from_json(*j, environment, settings);
    }
    m.log_problems(std::string("load_scene ") + std::string(path), result.errors);
    return result;
}

bool Context::save_scene(std::string_view path) {
    auto& m = *app_.impl_;
    Json doc = astral::save_scene(world, *prefabs);
    doc["sun"] = to_json(sun, {});
    doc["environment"] = to_json(environment, {});
    const std::filesystem::path file = std::filesystem::path(m.desc.asset_root) / path;
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    out << doc.dump(2) << '\n';
    if (!out) {
        log.log(LogLevel::Error, "save_scene: cannot write " + file.string());
        return false;
    }
    return true;
}

Entity Context::spawn_prefab(std::string_view path, const Transform& transform) {
    const Entity e = world.create(transform, PrefabInstance{std::string(path)});
    app_.impl_->log_problems("spawn_prefab", instantiate_prefab(world, *prefabs, e).errors);
    return e;
}

/// PrimitiveMesh -> MeshRenderer for new and edited components; drops the renderer when it is removed.
void App::Impl::sync_primitives() {
    std::vector<Entity> gone;
    world.query<const PrimitiveDrawn>().without<PrimitiveMesh>().each([&](Entity e, const PrimitiveDrawn&) { gone.push_back(e); });
    for (Entity e : gone) {
        world.remove<PrimitiveDrawn>(e);
        world.remove<MeshRenderer>(e);
    }
    world.set_change_baseline(primitives_seen);
    std::vector<std::pair<Entity, MeshRenderer>> updates;
    world.query<const PrimitiveMesh>().changed<PrimitiveMesh>().each([&](Entity e, const PrimitiveMesh& p) {
        const BuiltinMeshes& meshes = context->meshes;
        const MeshHandle mesh = p.shape == Primitive::Sphere ? meshes.sphere : p.shape == Primitive::Plane ? meshes.plane : meshes.cube;
        MaterialHandle material;
        if (renderer) {
            const MaterialDesc& d = p.material;
            std::string key = to_json(d, {}).dump();
            for (TextureHandle t : {d.base_color_texture, d.metallic_roughness_texture, d.normal_texture, d.emissive_texture}) {
                key += "/" + std::to_string(t.index) + "." + std::to_string(t.generation);
            }
            auto [it, inserted] = primitive_materials.try_emplace(key);
            if (inserted) it->second = renderer->create_material(d);
            material = it->second;
        }
        updates.emplace_back(e, MeshRenderer{mesh, material});
    });
    for (const auto& [e, r] : updates) {
        world.add(e, r);
        world.add(e, PrimitiveDrawn{});
    }
    primitives_seen = world.tick();
    world.advance_tick();
}
const Time& App::time() const noexcept { return impl_->time; }

namespace {

} // namespace

void App::Impl::spawn_model(Entity owner, const Model& model) {
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
        const Entity e = world.create(Transform{n.translation, n.rotation, n.scale}, Parent{item.parent}, ModelPiece{owner});
        if (n.pieces.size() == 1) {
            world.add(e, MeshRenderer{n.pieces[0].mesh, n.pieces[0].material});
        } else {
            for (const Model::Piece& p : n.pieces) {
                world.create(Transform{}, Parent{e}, ModelPiece{owner}, MeshRenderer{p.mesh, p.material});
            }
        }
        for (auto c = n.children.rbegin(); c != n.children.rend(); ++c) stack.push_back({*c, e});
    }
}

void App::Impl::sync_models() {
    // Pieces whose owner is gone (destroyed, or its ModelRef removed / changed) are cleared first.
    std::vector<Entity> stale;
    world.each<const ModelPiece>([&](Entity e, const ModelPiece& p) {
        const SpawnedModel* s = world.get<const SpawnedModel>(p.owner);
        if (!s || !world.has<ModelRef>(p.owner)) stale.push_back(e);
    });
    std::vector<std::pair<Entity, AssetId>> todo;
    world.each<const ModelRef>([&](Entity e, const ModelRef& ref) {
        const AssetId id = assets->load_model(ref.path); // idempotent
        const Model* model = assets->model(id);
        const SpawnedModel* s = world.get<const SpawnedModel>(e);
        if (model && (!s || s->id != id || s->version != model->version)) todo.emplace_back(e, id);
    });
    // Owners whose ModelRef was removed keep a SpawnedModel marker: drop it (their pieces are stale above).
    std::vector<Entity> orphans;
    world.query<const SpawnedModel>().without<ModelRef>().each([&](Entity e, const SpawnedModel&) { orphans.push_back(e); });
    for (Entity e : orphans) world.remove<SpawnedModel>(e);
    for (const auto& [owner, id] : todo) {
        world.each<const ModelPiece>([&](Entity e, const ModelPiece& p) {
            if (p.owner == owner) stale.push_back(e);
        });
    }
    std::sort(stale.begin(), stale.end(), [](Entity a, Entity b) { return a.index < b.index; });
    stale.erase(std::unique(stale.begin(), stale.end()), stale.end());
    for (Entity e : stale) world.destroy(e);
    for (const auto& [owner, id] : todo) {
        const Model* model = assets->model(id);
        spawn_model(owner, *model);
        world.add(owner, SpawnedModel{id, model->version});
    }
}

/// Each system sees changes made since it last ran (its own writes are stamped with the tick it
/// ran at, so it does not see them next time).
void App::run_phase(std::vector<std::unique_ptr<System>>& systems, Context& ctx) {
    for (auto& s : systems) {
        ctx.world.set_change_baseline(s->last_run_);
        s->update(ctx);
        s->last_run_ = ctx.world.tick();
        ctx.world.advance_tick();
    }
}

namespace {

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

bool App::Impl::start(Game& game) {
    if (started) {
        log.error("App: run()/run_frames() may only be called once per App");
        return false;
    }
    started = true;
    game.on_start(*context);
    world.apply_commands();
    clock.tick();
    return true;
}

void App::run_frames(Game& game, std::uint32_t frames) {
    auto& m = *impl_;
    if (!m.start(game)) return;
    for (std::uint32_t i = 0; i < frames && !m.context->quit_requested(); ++i) {
        if (m.window && !m.window->poll(m.input)) break;
        const float dt = m.desc.fixed_timestep > 0 ? m.desc.fixed_timestep : m.clock.tick();
        m.frame(game, dt);
    }
    game.on_shutdown(*m.context);
    if (m.device) m.device->wait_idle();
}

int App::run(Game& game) {
    auto& m = *impl_;
    if (!m.start(game)) return 1;
    while (!m.context->quit_requested()) {
        if (m.window && !m.window->poll(m.input)) break;
        if (m.window && m.window->minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // don't spin while hidden
            continue;
        }
        const float dt = m.desc.fixed_timestep > 0 ? m.desc.fixed_timestep : m.clock.tick();
        m.frame(game, dt);
    }
    game.on_shutdown(*m.context);
    if (m.device) m.device->wait_idle();
    return 0;
}

/// Creates bodies/characters for new components, destroys those whose entity or component is gone.
void App::Impl::sync_physics_bodies() {
    std::erase_if(body_links, [&](const BodyLink& link) {
        const RigidBody* rb = world.get<RigidBody>(link.entity);
        if (rb && rb->handle == link.body) return false;
        physics->destroy(link.body);
        return true;
    });
    std::erase_if(character_links, [&](const CharacterLink& link) {
        const CharacterController* cc = world.get<CharacterController>(link.entity);
        if (cc && cc->handle == link.character) return false;
        physics->destroy(link.character);
        return true;
    });
    // A component copied from another entity carries that entity's handle: only a body whose
    // user data names *this* entity counts as its own.
    world.each<RigidBody, const Transform>([&](Entity e, RigidBody& rb, const Transform& t) {
        if (physics->valid(rb.handle) && physics->user_data(rb.handle) == to_user_data(e)) return;
        BodyDesc body_desc = rb.desc;
        body_desc.position = t.position;
        body_desc.rotation = t.rotation;
        body_desc.user_data = to_user_data(e);
        rb.handle = physics->create_body(body_desc);
        if (rb.handle.valid()) body_links.push_back({e, rb.handle, t.position, t.rotation});
    });
    world.each<CharacterController, const Transform>([&](Entity e, CharacterController& cc, const Transform& t) {
        if (physics->valid(cc.handle) && physics->user_data(cc.handle) == to_user_data(e)) return;
        CharacterDesc character_desc = cc.desc;
        character_desc.position = t.position;
        character_desc.user_data = to_user_data(e);
        cc.handle = physics->create_character(character_desc);
        if (cc.handle.valid()) character_links.push_back({e, cc.handle});
    });
}

/// Fixed-rate physics: FixedUpdate systems, kinematic targets, characters, step; then the
/// simulated poses go back into Transforms.
void App::Impl::step_physics(Game&, float delta) {
    Context& ctx = *context;
    frame_contacts.clear();
    sync_physics_bodies();
    const float fixed = time.fixed_delta;
    const std::uint32_t max_steps = std::max(desc.max_physics_steps, 1u);
    physics_accumulator += delta;
    // Kinematic targets are spread over this frame's steps (a 30 FPS frame = 2 even moves,
    // not one jump and one stop).
    const auto planned = static_cast<std::uint32_t>(std::min<float>(physics_accumulator / fixed, static_cast<float>(max_steps)));
    std::uint32_t steps = 0;
    while (physics_accumulator >= fixed && steps < max_steps) {
        in_fixed_update = true;
        run_phase(systems[static_cast<std::size_t>(Phase::FixedUpdate)], ctx);
        in_fixed_update = false;
        world.apply_commands();
        sync_physics_bodies(); // FixedUpdate systems may have spawned or removed bodies
        const float alpha = planned > 0 ? std::min(1.0f, static_cast<float>(steps + 1) / static_cast<float>(planned)) : 1.0f;
        for (const BodyLink& link : body_links) {
            const RigidBody* rb = world.get<RigidBody>(link.entity);
            const Transform* t = world.get<Transform>(link.entity);
            if (!rb || !t || rb->desc.motion != MotionType::Kinematic) continue;
            const Vec3 target = lerp(link.previous_position, t->position, alpha);
            const Quat from = link.previous_rotation, to = t->rotation;
            const float sign = (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w) < 0 ? -1.0f : 1.0f;
            const Quat rotation = normalize(Quat{from.x + (to.x * sign - from.x) * alpha, from.y + (to.y * sign - from.y) * alpha,
                                                 from.z + (to.z * sign - from.z) * alpha, from.w + (to.w * sign - from.w) * alpha});
            physics->move_kinematic(rb->handle, target, rotation, fixed);
        }
        world.each<CharacterController>([&](Entity e, CharacterController& cc) {
            if (physics->user_data(cc.handle) != to_user_data(e)) return;
            physics->update_character(cc.handle, cc.move, cc.jump, fixed, cc.jump_speed);
            cc.jump = false; // one jump per press
        });
        physics->step(fixed);
        frame_contacts.insert(frame_contacts.end(), physics->contacts().begin(), physics->contacts().end());
        physics_accumulator -= fixed;
        ++steps;
    }
    if (steps == max_steps) physics_accumulator = std::min(physics_accumulator, fixed); // drop the backlog
    if (steps > 0) {
        for (BodyLink& link : body_links) {
            if (const Transform* t = world.get<Transform>(link.entity)) {
                link.previous_position = t->position;
                link.previous_rotation = t->rotation;
            }
        }
    }

    world.each<const RigidBody, Transform>([&](const RigidBody& rb, Transform& t) {
        if (rb.desc.motion != MotionType::Dynamic || !physics->valid(rb.handle)) return;
        t.position = physics->position(rb.handle);
        t.rotation = physics->rotation(rb.handle);
    });
    world.each<CharacterController, Transform>([&](Entity e, CharacterController& cc, Transform& t) {
        if (!physics->valid(cc.handle) || physics->user_data(cc.handle) != to_user_data(e)) return;
        const CharacterState state = physics->character_state(cc.handle);
        t.position = state.position;
        cc.on_ground = state.on_ground;
        cc.velocity = state.velocity;
    });
}

void App::Impl::frame(Game& game, float delta) {
    world.update_events(); // events sent last frame are still readable this frame, then dropped
    assets->update();      // finished loads, hot reloads
    prefabs->refresh();    // templates whose documents reloaded are rebuilt on next use
    {
        std::vector<std::string> problems;
        sync_prefabs(world, *prefabs, &problems); // instances spawned by code / prefabs that changed
        log_problems("prefab", problems);
    }
    sync_models();         // ModelRef entities get (or refresh) their pieces before any system runs
    Impl& m = *this;
    Context& ctx = *m.context;
    m.time.delta = delta;

    run_phase(m.systems[static_cast<std::size_t>(Phase::PreUpdate)], ctx);
    m.world.apply_commands();
    game.on_update(ctx);
    m.world.apply_commands();
    run_phase(m.systems[static_cast<std::size_t>(Phase::Update)], ctx);
    m.world.apply_commands();
    m.step_physics(game, delta);
    m.world.apply_commands();
    run_phase(m.systems[static_cast<std::size_t>(Phase::PostUpdate)], ctx);
    m.world.apply_commands();
    update_transforms(m.world);
    m.sync_primitives();

    const bool offscreen = m.offscreen_target.valid();
    if (m.renderer && (offscreen || (m.swapchain && !(m.window && m.window->minimized())))) {
        gpu::TextureHandle image = offscreen ? m.offscreen_target : m.swapchain->acquire();
        if (image.valid()) {
            // Extract: one MeshInstance per entity, with last frame's transform for motion vectors.
            m.draw_items.clear();
            m.new_histories.clear();
            m.world.each<const MeshRenderer, const WorldTransform>(
                [&](Entity e, const MeshRenderer& r, const WorldTransform& w) {
                    MeshInstance inst{.mesh = r.mesh, .material = r.material, .transform = w.matrix};
                    if (RenderHistory* h = m.world.get<RenderHistory>(e)) {
                        inst.previous_transform = h->previous;
                        h->previous = w.matrix;
                    } else {
                        m.new_histories.emplace_back(e, w.matrix);
                    }
                    m.draw_items.push_back(inst);
                });
            for (const auto& [e, matrix] : m.new_histories) m.world.add(e, RenderHistory{matrix});
            m.sdf_sorted.clear();
            m.world.each<const Sdf>([&](Entity e, const Sdf& s) {
                SdfShape shape = s.shape;
                if (const WorldTransform* w = m.world.get<WorldTransform>(e)) {
                    shape.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                    shape.rotation = rotation_of(w->matrix);
                }
                m.sdf_sorted.emplace_back(s.order, shape);
            });
            std::stable_sort(m.sdf_sorted.begin(), m.sdf_sorted.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            m.sdf_shapes.clear();
            for (const auto& [order, shape] : m.sdf_sorted) m.sdf_shapes.push_back(shape);
            m.point_lights.clear();
            m.world.each<const PointLight>([&](Entity e, const PointLight& l) {
                PointLight light = l;
                if (const WorldTransform* w = m.world.get<WorldTransform>(e)) {
                    light.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                }
                m.point_lights.push_back(light);
            });
            m.rect_lights.clear();
            m.world.each<const RectLight>([&](Entity e, const RectLight& l) {
                RectLight light = l;
                if (const WorldTransform* w = m.world.get<WorldTransform>(e)) {
                    // Position from the entity; the panel's axes turn and scale with it.
                    light.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                    const Vec4 r = w->matrix * Vec4{l.right.x, l.right.y, l.right.z, 0};
                    const Vec4 u = w->matrix * Vec4{l.up.x, l.up.y, l.up.z, 0};
                    light.right = {r.x, r.y, r.z};
                    light.up = {u.x, u.y, u.z};
                }
                m.rect_lights.push_back(light);
            });
            const gpu::Extent2D extent = offscreen ? gpu::Extent2D{m.desc.width, m.desc.height} : m.swapchain->extent();
            const float aspect = extent.height ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
            const CameraData camera = find_camera(m.world, aspect);
            auto cmd = m.device->begin_commands();
            m.renderer->render(cmd, image, camera,
                               {.meshes = m.draw_items, .sdf = m.sdf_shapes, .lights = m.point_lights,
                                .rect_lights = m.rect_lights, .sun = ctx.sun, .environment = ctx.environment});
            if (offscreen) {
                cmd.transition(image, gpu::ResourceState::TransferSrc);
                cmd.copy_texture_to_buffer(image, m.offscreen_readback);
                m.device->wait(m.device->submit(cmd));
            } else {
                cmd.transition(image, gpu::ResourceState::Present);
                m.swapchain->present(cmd);
            }
        }
    }

    m.time.elapsed += delta;
    ++m.time.frame;
}

} // namespace astral
