#include <astral/app/app.hpp>

#include "debug_ui.hpp"
#include "json_asset.hpp"
#include "map_runtime.hpp"
#include "material_library.hpp"

#include <astral/core/clock.hpp>
#include <astral/gpu/device.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/paths.hpp>
#include <astral/platform/window.hpp>
#include <astral/world/transform.hpp>

#include <array>
#include <cctype>
#include <iterator>
#include <optional>
#include <typeinfo>
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
/// Internal: the voice an AudioSource plays.
struct AudioVoice {
    VoiceHandle voice;
    std::string sound;
};
} // namespace

namespace {
/// What scripts see of the input: the named actions.
class ActionInput final : public ScriptInput {
public:
    explicit ActionInput(const InputActions& a) : actions_(a) {}
    bool down(std::string_view a) const override { return actions_.down(a); }
    bool pressed(std::string_view a) const override { return actions_.pressed(a); }
    bool released(std::string_view a) const override { return actions_.released(a); }
    float value(std::string_view a) const override { return actions_.value(a); }
    Vec2 axis(std::string_view a) const override { return actions_.axis(a); }

private:
    const InputActions& actions_;
};

/// What scripts may do with physics: the App's bodies and fields behind a narrow interface.
class AppScriptPhysics final : public ScriptPhysics {
public:
    AppScriptPhysics(World& world, PhysicsWorld& physics, const FieldWorld& fields, const Time& time)
        : world_(world), physics_(physics), fields_(fields), time_(time) {}

    std::optional<ScriptHit> raycast(Vec3 origin, Vec3 direction, float max_distance, std::uint32_t layers, Entity ignore) const override {
        const auto hit = physics_.raycast(origin, direction, max_distance, {.layers = layers, .ignore = body_of(ignore)});
        return hit ? std::optional<ScriptHit>(to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface)) : std::nullopt;
    }
    std::optional<ScriptHit> spherecast(Vec3 origin, float radius, Vec3 displacement, std::uint32_t layers, Entity ignore) const override {
        const auto hit = physics_.shape_cast(ShapeDesc::sphere(radius), origin, {}, displacement, {.layers = layers, .ignore = body_of(ignore)});
        return hit ? std::optional<ScriptHit>(to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface)) : std::nullopt;
    }
    std::vector<Entity> overlap_sphere(Vec3 center, float radius, std::uint32_t layers) const override {
        return entities(physics_.overlap(ShapeDesc::sphere(radius), center, {}, {.layers = layers}));
    }
    std::vector<Entity> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation, std::uint32_t layers) const override {
        return entities(physics_.overlap(ShapeDesc::box(half_extents), center, rotation, {.layers = layers}));
    }
    void impulse(Entity body, Vec3 v) override { physics_.add_impulse(body_of(body), v); }
    void force(Entity body, Vec3 v) override { physics_.add_force(body_of(body), v); }
    void set_velocity(Entity body, Vec3 v) override { physics_.set_linear_velocity(body_of(body), v); }
    Vec3 velocity(Entity body) const override { return physics_.linear_velocity(body_of(body)); }
    Vec3 wind(Vec3 position) const override { return fields_.wind_at(position, time_.elapsed); }
    ScriptWater water(Vec3 position) const override {
        const WaterSample s = fields_.water_at(position, time_.elapsed);
        return {s.inside, s.surface_height, s.depth, s.flow, s.density};
    }

private:
    BodyHandle body_of(Entity e) const {
        const RigidBody* rb = e.valid() ? world_.get<const RigidBody>(e) : nullptr;
        return rb ? rb->handle : BodyHandle{};
    }
    static ScriptHit to_script(std::uint64_t user_data, Vec3 position, Vec3 normal, float distance, int surface) {
        return {entity_from_user_data(user_data), position, normal, distance, surface};
    }
    static std::vector<Entity> entities(const std::vector<OverlapHit>& hits) {
        std::vector<Entity> out;
        for (const OverlapHit& h : hits) out.push_back(entity_from_user_data(h.user_data));
        return out;
    }
    World& world_;
    PhysicsWorld& physics_;
    const FieldWorld& fields_;
    const Time& time_;
};

/// Save slots as files: `<folder>/<slot>.json`. Slot names are letters, digits, '_' and '-' only.
class FileStorage final : public ScriptStorage {
public:
    explicit FileStorage(std::filesystem::path folder) : folder_(std::move(folder)) {}
    bool write(std::string_view slot, std::string_view text) override {
        if (!valid(slot)) return false;
        std::error_code ec;
        std::filesystem::create_directories(folder_, ec);
        std::ofstream out(folder_ / (std::string(slot) + ".json"), std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }
    std::optional<std::string> read(std::string_view slot) const override {
        if (!valid(slot)) return std::nullopt;
        std::ifstream in(folder_ / (std::string(slot) + ".json"), std::ios::binary);
        if (!in) return std::nullopt;
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

private:
    static bool valid(std::string_view slot) {
        if (slot.empty()) return false;
        for (char c : slot) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
        }
        return true;
    }
    std::filesystem::path folder_;
};
} // namespace

struct App::Impl {
    AppDesc desc;
    Logger log;
    JobSystem jobs;
    World world;
    Input input;      ///< What the window reports.
    Input game_input; ///< What the game sees: input minus what the debug UI took.
    std::unique_ptr<DebugUi> debug_ui;
    std::unique_ptr<Font> font;
    std::array<UiLayer, 2> ui_layers{}; // HUD, debug UI
    bool restore_mouse_capture = false;
    bool text_input = false;
    UiLayer ui_layer{};
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
    std::unique_ptr<AudioEngine> audio;
    std::unordered_map<std::string, SoundHandle> sounds; // by asset path
    void sync_audio();
    Profiler profiler;
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
        Vec3 step_position{};     // physics_interpolation: simulated pose before the last step
        Quat step_rotation{};
    };
    struct CharacterLink {
        Entity entity;
        CharacterHandle character;
    };
    std::vector<BodyLink> body_links;
    std::vector<CharacterLink> character_links;
    struct JointLink {
        Entity entity;
        JointHandle joint;
    };
    std::vector<JointLink> joint_links;

    std::unique_ptr<ScriptHost> scripts;
    std::unique_ptr<ScriptInput> script_input;
    std::unique_ptr<ScriptStorage> script_storage;
    std::unique_ptr<ScriptPhysics> script_physics;
    struct Watched {
        std::uint64_t hash = 0;
        bool is_actions = false;
    };
    std::unordered_map<std::string, Watched> watched_files; ///< Scripts and action files, for hot reload.
    double next_watch = 0.0;
    void reload_changed_scripts();
    void run_scripts(ScriptPhase phase);
    std::unique_ptr<MaterialLibrary> material_library; ///< .material / .ashader files (needs a renderer)
    std::unique_ptr<MapRuntime> map; ///< Open map (ctx.open_world); null until the first one.
    std::filesystem::path asset_root; ///< Copied at construction (AppDesc holds views).
    FloatingOrigin origin;
    std::vector<StreamingPoint> stream_points; // scratch
    /// Streaming sources in local coordinates (StreamingSource entities, else the active camera).
    std::vector<Vec3> streaming_sources();
    void update_world(bool finish);

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
    void update_fields();
    void apply_field_forces(float dt);
    void emit_contact_events();
    void sync_models();
    void spawn_model(Entity owner, const Model& model);
    void sync_primitives();
    void log_problems(std::string_view what, const std::vector<std::string>& problems);
    void step_physics(Game& game, float delta);
    void end_frame_profile();
    void route_input();
    std::span<const UiLayer> overlay_layers();
    void build_debug_ui(Game& game, float delta);
    void draw_debug_overlays();

    explicit Impl(const AppDesc& d)
        : desc(d), jobs(d.worker_threads), asset_root(std::filesystem::path(d.asset_root)), origin(d.origin_shift_distance) {}
    bool start(Game& game);
    void frame(Game& game, float delta);
};

void Context::register_system(Phase phase, std::unique_ptr<System> system) {
    system->name_ = demangle(typeid(*system).name());
    app_.impl_->systems[static_cast<std::size_t>(phase)].push_back(std::move(system));
}

App::App(const AppDesc& desc) : impl_(std::make_unique<Impl>(desc)) {
    auto& m = *impl_;
    m.log.add_sink(std::make_shared<ConsoleSink>());
    m.context.reset(new Context(m.world, m.game_input, m.time, m.log, m.jobs, *this));
    TypeRegistry& types = m.context->types;
    register_world_types(types);
    types.add<Camera>();
    types.add<PointLight>();
    types.add<RectLight>();
    types.add<Sdf>();
    types.add<ModelRef>();
    types.add<PrimitiveMesh>();
    types.add<AudioSource>();
    types.add<AudioListener>();
    types.add<StreamingSource>();
    types.add<Joint>();
    types.add<WindZone>();
    types.add<WaterBody>();
    types.add<Buoyant>();
    types.add<AirDrag>();
    types.add<Airfoil>();
    m.context->events.add<Contact>();
    m.context->events.add<TriggerEntered>();
    m.context->events.add<TriggerExited>();
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
    std::filesystem::path pack;
    if (!desc.pack.empty()) {
        pack = std::filesystem::path(desc.pack);
        if (pack.is_relative()) pack = executable_directory() / pack;
        std::error_code ec;
        if (std::filesystem::is_regular_file(pack, ec)) m.log.info("App: assets from {}", pack.string());
        else pack.clear(); // development: loose files
    }
    m.assets = std::make_unique<AssetManager>(AssetManagerDesc{.root = std::filesystem::path(desc.asset_root),
                                                               .renderer = m.renderer.get(), .jobs = &m.jobs,
                                                               .logger = &m.log, .hot_reload = desc.hot_reload,
                                                               .reload_poll_seconds = 0.5f, .pack = pack});
    m.context->assets = m.assets.get();
    if (m.renderer) {
        m.material_library = std::make_unique<MaterialLibrary>(*m.renderer, *m.assets, m.asset_root,
                                                                desc.hot_reload && pack.empty(), m.log);
    }
    m.prefabs = std::make_unique<PrefabLibrary>(types, [&m](std::string_view path) {
        const AssetId id = m.assets->load_document(path);
        if (const Document* d = m.assets->document(id)) return SourceDocument{.json = &d->json, .version = d->version, .error = {}};
        return SourceDocument{.json = nullptr, .version = 0, .error = std::string(m.assets->error(id))};
    });
    m.context->prefabs = m.prefabs.get();
    m.context->profiler = &m.profiler;
    {
        std::string error;
        const bool device = desc.audio && !desc.headless && !desc.offscreen; // captures and tests mix silently
        if (device) m.audio = AudioEngine::create({.logger = &m.log}, &error);
        if (!m.audio) {
            if (device) m.log.log(LogLevel::Warn, error + "; sound is mixed silently");
            m.audio = AudioEngine::create({.null_device = true, .logger = &m.log}, &error);
        }
        m.context->audio = m.audio.get();
    }
    if (m.renderer && desc.debug_ui) m.debug_ui = std::make_unique<DebugUi>(*m.renderer);
    m.font = Font::create(default_font_ttf());
    if (m.font && m.renderer) m.font->upload(*m.renderer);
    m.context->font = m.font.get();
    m.script_input = std::make_unique<ActionInput>(m.context->actions);
    m.script_storage = std::make_unique<FileStorage>(std::filesystem::path(std::string(desc.save_folder)));
    m.script_physics = std::make_unique<AppScriptPhysics>(m.world, *m.physics, m.context->fields, m.time);
    m.scripts = std::make_unique<ScriptHost>(ScriptEnv{m.world, m.context->types, m.context->events, m.log, m.script_input.get(), m.script_storage.get(), m.script_physics.get()});
    m.context->scripts = m.scripts.get();
}

App::~App() {
    auto& m = *impl_;
    if (m.device) m.device->wait_idle();
    // Systems may release GPU resources in their destructors: they go first, while the
    // renderer and device still exist. Then the services, device last, window after it.
    for (auto& phase : m.systems) phase.clear();
    m.context->scripts = nullptr;
    m.scripts.reset();
    m.map.reset(); // waits for cell reads in flight
    m.material_library.reset(); // materials and shaders belong to the renderer
    m.context->physics = nullptr;
    m.physics.reset();
    m.debug_ui.reset(); // owns renderer textures
    m.context->audio = nullptr;
    m.audio.reset();
    m.context->font = nullptr;
    m.font.reset();
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
    const std::filesystem::path file = m.asset_root / path;
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
        if (renderer && !p.material_file.empty()) {
            material = material_library ? material_library->get(p.material_file) : MaterialHandle{}; // invalid: default material
        } else if (renderer) {
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
        ASTRAL_PROFILE_SCOPE(ctx.profiler, s->name_.c_str());
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
        m.end_frame_profile();
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
        m.end_frame_profile();
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
        if (rb.handle.valid()) body_links.push_back({e, rb.handle, t.position, t.rotation, t.position, t.rotation});
    });
    // Joints: once both bodies exist. A joint whose entity or bodies are gone goes away (the physics world also drops the
    // joints of destroyed bodies).
    std::erase_if(joint_links, [&](const JointLink& link) {
        const Joint* j = world.get<const Joint>(link.entity);
        if (j && j->handle == link.joint && physics->valid(link.joint)) return false;
        physics->destroy(link.joint);
        return true;
    });
    world.each<Joint>([&](Entity e, Joint& j) {
        const bool ours = std::any_of(joint_links.begin(), joint_links.end(), [&](const JointLink& l) { return l.entity == e && l.joint == j.handle; });
        if (ours) return;
        const RigidBody* ra = j.a.valid() ? world.get<const RigidBody>(j.a) : nullptr;
        const RigidBody* rb = j.b.valid() ? world.get<const RigidBody>(j.b) : nullptr;
        if (!rb || !physics->valid(rb->handle) || (j.a.valid() && (!ra || !physics->valid(ra->handle)))) return; // wait for the bodies
        j.handle = physics->create_joint({.type = j.type, .body_a = ra ? ra->handle : BodyHandle{}, .body_b = rb->handle, .anchor = j.anchor,
                                          .anchor_b = j.anchor_b, .axis = j.axis, .limited = j.limited, .min_limit = j.min_limit,
                                          .max_limit = j.max_limit, .spring_frequency = j.spring_frequency, .spring_damping = j.spring_damping,
                                          .max_friction = j.max_friction, .collide_connected = j.collide_connected});
        if (j.handle.valid()) joint_links.push_back({e, j.handle});
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

/// Rebuilds the fields from WindZone / WaterBody entities (their position is the box centre).
void App::Impl::update_fields() {
    FieldWorld& fields = context->fields;
    fields.clear_volumes();
    fields.set_global_wind({});
    const auto centre = [&](Entity e) {
        if (const WorldTransform* w = world.get<const WorldTransform>(e)) return Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        const Transform* t = world.get<const Transform>(e);
        return t ? t->position : Vec3{};
    };
    world.each<const WindZone>([&](Entity e, const WindZone& z) {
        const WindDesc wind{.velocity = z.velocity, .gust_strength = z.gust_strength, .gust_frequency = z.gust_frequency,
                            .turbulence = z.turbulence, .turbulence_scale = z.turbulence_scale, .seed = z.seed};
        if (z.global) fields.set_global_wind(wind);
        else fields.add_wind_volume({.center = centre(e), .half_extents = z.half_extents, .wind = wind, .falloff = z.falloff, .replace = z.replace});
    });
    world.each<const WaterBody>([&](Entity e, const WaterBody& w) {
        fields.add_water_volume({.center = centre(e), .half_extents = w.half_extents, .density = w.density, .flow = w.flow,
                                 .wave_height = w.wave_height, .wave_length = w.wave_length, .wave_speed = w.wave_speed});
    });
}

/// AirDrag, Buoyant and Airfoil: forces from the fields on the bodies that have them.
void App::Impl::apply_field_forces(float dt) {
    const FieldWorld& fields = context->fields;
    const double now = time.elapsed;
    const float gravity = length(physics->gravity());
    const auto body_of = [&](const RigidBody& rb) { return rb.desc.motion == MotionType::Dynamic && physics->valid(rb.handle); };
    // Water first: how much of each body is under, so air drag can fade out.
    world.each<const RigidBody, const Buoyant>([&](const RigidBody& rb, const Buoyant& b) {
        if (!body_of(rb)) return;
        const Vec3 p = physics->position(rb.handle);
        const Vec3 v = physics->linear_velocity(rb.handle);
        const float height = std::max(b.height, 0.01f);
        const WaterSample water = fields.water_at({p.x, p.y - 0.5f * height, p.z}, now); // at the bottom of the body
        if (!water.inside) return;
        const float fraction = std::clamp(water.depth / height, 0.0f, 1.0f);
        const float mass = physics->mass(rb.handle);
        const float volume = b.volume > 0.0f ? b.volume : mass / 500.0f;
        const Vec3 up = gravity > 0.0f ? normalize(Vec3{0, 1, 0}) : Vec3{0, 1, 0};
        physics->add_force(rb.handle, up * (water.density * gravity * volume * fraction));
        const Vec3 relative = water.flow - v;
        physics->add_force(rb.handle, relative * (0.5f * water.density * b.drag * b.area * length(relative) * fraction));
        const Vec3 spin = physics->angular_velocity(rb.handle);
        physics->set_angular_velocity(rb.handle, spin * std::max(0.0f, 1.0f - b.angular_drag * fraction * dt));
    });
    world.each<const RigidBody, const AirDrag>([&](Entity e, const RigidBody& rb, const AirDrag& d) {
        if (!body_of(rb)) return;
        const Vec3 p = physics->position(rb.handle);
        float in_air = 1.0f;
        if (const Buoyant* b = world.get<const Buoyant>(e)) {
            const WaterSample water = fields.water_at({p.x, p.y - 0.5f * std::max(b->height, 0.01f), p.z}, now);
            if (water.inside) in_air = 1.0f - std::clamp(water.depth / std::max(b->height, 0.01f), 0.0f, 1.0f);
        }
        const Vec3 relative = fields.wind_at(p, now) - physics->linear_velocity(rb.handle);
        physics->add_force(rb.handle, relative * (0.5f * air_density * d.drag * d.area * length(relative) * in_air));
    });
    world.each<const RigidBody, const Airfoil>([&](const RigidBody& rb, const Airfoil& wing) {
        if (!body_of(rb)) return;
        const Vec3 p = physics->position(rb.handle);
        const Quat q = physics->rotation(rb.handle);
        const Vec3 flow = fields.wind_at(p, now) - physics->linear_velocity(rb.handle); // air moving past the wing
        const float speed = length(flow);
        if (speed < 0.1f) return;
        const Vec3 forward = q * normalize(wing.forward), up = q * normalize(wing.up);
        const float alpha = std::atan2(dot(flow, up), -dot(flow, forward)); // + when the air meets the underside (it moves towards the lift side)
        const float stall = std::max(wing.stall_angle, 0.01f);
        float coefficient = wing.lift_slope * std::clamp(alpha, -stall, stall);
        if (std::abs(alpha) > stall) coefficient *= std::clamp(1.0f - (std::abs(alpha) - stall) / stall, 0.0f, 1.0f);
        const Vec3 direction = flow / speed;
        const Vec3 lift_axis = normalize(up - direction * dot(up, direction)); // perpendicular to the flow, on the wing's lift side
        physics->add_force(rb.handle, lift_axis * (0.5f * air_density * speed * speed * wing.area * coefficient));
    });
}

/// Physics contacts of this frame as events: collisions and triggers.
void App::Impl::emit_contact_events() {
    for (const ContactEvent& c : frame_contacts) {
        const Entity a = c.user_data_a ? entity_from_user_data(c.user_data_a) : Entity{};
        const Entity b = c.user_data_b ? entity_from_user_data(c.user_data_b) : Entity{};
        const bool begin = c.type == ContactEvent::Type::Begin;
        if (c.sensor) {
            const auto is_sensor = [&](Entity e) {
                const RigidBody* rb = e.valid() ? world.get<const RigidBody>(e) : nullptr;
                return rb && rb->desc.sensor;
            };
            const bool a_is = is_sensor(a);
            const Entity trigger = a_is ? a : b, other = a_is ? b : a;
            if (!trigger.valid() || !other.valid()) continue;
            if (begin) world.send(TriggerEntered{trigger, other});
            else world.send(TriggerExited{trigger, other});
            continue;
        }
        world.send(Contact{begin ? ContactKind::Begin : ContactKind::End, a, b, c.point, c.normal, c.surface_a, c.surface_b});
    }
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
        update_fields();
        in_fixed_update = true;
        {
            ASTRAL_PROFILE_SCOPE(&profiler, "FixedUpdate");
            run_phase(systems[static_cast<std::size_t>(Phase::FixedUpdate)], ctx);
            run_scripts(ScriptPhase::FixedUpdate);
        }
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
        if (desc.physics_interpolation) {
            for (BodyLink& link : body_links) { // the pose this step starts from
                link.step_position = physics->position(link.body);
                link.step_rotation = physics->rotation(link.body);
            }
        }
        apply_field_forces(fixed);
        {
            ASTRAL_PROFILE_SCOPE(&profiler, "physics.step");
            physics->step(fixed);
        }
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

    emit_contact_events();
    const float blend = desc.physics_interpolation ? std::clamp(physics_accumulator / fixed, 0.0f, 1.0f) : 1.0f;
    world.each<const RigidBody, Transform>([&](const RigidBody& rb, Transform& t) {
        if (rb.desc.motion != MotionType::Dynamic || !physics->valid(rb.handle)) return;
        t.position = physics->position(rb.handle);
        t.rotation = physics->rotation(rb.handle);
    });
    if (desc.physics_interpolation) {
        for (const BodyLink& link : body_links) { // between the last two simulated poses
            Transform* t = world.get<Transform>(link.entity);
            const RigidBody* rb = world.get<RigidBody>(link.entity);
            if (!t || !rb || rb->desc.motion != MotionType::Dynamic || !physics->valid(rb->handle)) continue;
            const Quat from = link.step_rotation, to = t->rotation;
            const float sign = (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w) < 0 ? -1.0f : 1.0f;
            t->position = lerp(link.step_position, t->position, blend);
            t->rotation = normalize(Quat{from.x + (to.x * sign - from.x) * blend, from.y + (to.y * sign - from.y) * blend,
                                         from.z + (to.z * sign - from.z) * blend, from.w + (to.w * sign - from.w) * blend});
        }
    }
    world.each<CharacterController, Transform>([&](Entity e, CharacterController& cc, Transform& t) {
        if (!physics->valid(cc.handle) || physics->user_data(cc.handle) != to_user_data(e)) return;
        const CharacterState state = physics->character_state(cc.handle);
        t.position = state.position;
        cc.on_ground = state.on_ground;
        cc.velocity = state.velocity;
    });
}

void App::Impl::frame(Game& game, float delta) {
    profiler.begin_frame();
    Profiler* prof = &profiler;
    route_input();
    context->actions.update(game_input);
    world.update_events(); // events sent last frame are still readable this frame, then dropped
    {
        ASTRAL_PROFILE_SCOPE(prof, "world streaming");
        update_world(false);
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "assets");
        assets->update();      // finished loads, hot reloads
        if (material_library) material_library->update(time.elapsed); // edited .material / .ashader files
        reload_changed_scripts();
        prefabs->refresh();    // templates whose documents reloaded are rebuilt on next use
        std::vector<std::string> problems;
        sync_prefabs(world, *prefabs, &problems); // instances spawned by code / prefabs that changed
        log_problems("prefab", problems);
        sync_models();         // ModelRef entities get (or refresh) their pieces before any system runs
    }
    Impl& m = *this;
    Context& ctx = *m.context;
    m.time.delta = delta;

    {
        ASTRAL_PROFILE_SCOPE(prof, "PreUpdate");
        run_phase(m.systems[static_cast<std::size_t>(Phase::PreUpdate)], ctx);
        m.run_scripts(ScriptPhase::PreUpdate);
        m.world.apply_commands();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "Game::on_update");
        game.on_update(ctx);
        m.world.apply_commands();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "Update");
        run_phase(m.systems[static_cast<std::size_t>(Phase::Update)], ctx);
        m.run_scripts(ScriptPhase::Update);
        m.world.apply_commands();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "physics");
        m.step_physics(game, delta);
        m.world.apply_commands();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "PostUpdate");
        run_phase(m.systems[static_cast<std::size_t>(Phase::PostUpdate)], ctx);
        m.run_scripts(ScriptPhase::PostUpdate);
        m.world.apply_commands();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "transforms");
        update_transforms(m.world);
        m.sync_primitives();
    }
    {
        ASTRAL_PROFILE_SCOPE(prof, "audio");
        m.sync_audio();
    }
    ASTRAL_PROFILE_SCOPE(prof, "render");
    m.draw_debug_overlays();
    m.build_debug_ui(game, delta);

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
                                .rect_lights = m.rect_lights, .sun = ctx.sun, .environment = ctx.environment,
                                .debug_lines = ctx.debug.lines(),
                                .ui = m.overlay_layers(),
                                .time = static_cast<float>(m.time.elapsed),
                                .sky = ctx.sky,
                                .post = ctx.post});
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

    ctx.debug.advance(delta); // this frame's shapes go; timed ones count down
    ctx.hud.clear();
    m.time.elapsed += delta;
    ++m.time.frame;
}

std::span<const UiLayer> App::Impl::overlay_layers() {
    std::size_t n = 0;
    if (!context->hud.empty()) ui_layers[n++] = context->hud.layer();
    if (!ui_layer.commands.empty()) ui_layers[n++] = ui_layer;
    return std::span<const UiLayer>(ui_layers.data(), n);
}

SoundHandle Context::sound(std::string_view path) {
    auto& m = *app_.impl_;
    auto [it, inserted] = m.sounds.try_emplace(std::string(path));
    if (!inserted) return it->second;
    std::string error;
    if (auto bytes = assets->read(path, &error)) {
        it->second = audio->load(*bytes, path);
        if (!it->second.valid()) log.log(LogLevel::Error, "sound " + std::string(path) + ": cannot decode (WAV, FLAC or MP3)");
    } else {
        log.log(LogLevel::Error, "sound: " + error);
    }
    return it->second;
}

/// AudioSource -> voices (started, moved with the entity, stopped); the listener follows its entity.
void App::Impl::sync_audio() {
    Context& ctx = *context;
    auto position = [&](Entity e) {
        const WorldTransform* w = world.get<WorldTransform>(e);
        return w ? Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z} : Vec3{};
    };
    // Listener: an AudioListener entity, else the first active camera.
    bool listener = false;
    auto listen_from = [&](const Mat4& m) {
        const Vec3 forward = normalize(Vec3{-m[2].x, -m[2].y, -m[2].z});
        const Vec3 up = normalize(Vec3{m[1].x, m[1].y, m[1].z});
        audio->set_listener({m[3].x, m[3].y, m[3].z}, forward, up);
        listener = true;
    };
    world.each<const AudioListener, const WorldTransform>([&](const AudioListener&, const WorldTransform& w) {
        if (!listener) listen_from(w.matrix);
    });
    world.each<const Camera, const WorldTransform>([&](const Camera& c, const WorldTransform& w) {
        if (!listener && c.active) listen_from(w.matrix);
    });

    // Removed sources stop.
    std::vector<Entity> gone;
    world.query<const AudioVoice>().without<AudioSource>().each([&](Entity e, const AudioVoice&) { gone.push_back(e); });
    for (Entity e : gone) {
        audio->stop(world.get<AudioVoice>(e)->voice, 0.05f);
        world.remove<AudioVoice>(e);
    }
    std::vector<std::pair<Entity, AudioVoice>> new_voices;
    world.each<AudioSource>([&](Entity e, AudioSource& src) {
        AudioVoice* v = world.get<AudioVoice>(e);
        const bool alive = v && audio->playing(v->voice);
        if (v && (v->sound != src.sound || !src.playing)) { // stopped, or another sound
            audio->stop(v->voice, 0.05f);
            if (!src.playing) return;
        }
        if (alive && v->sound == src.sound) { // keep it in step with the component
            audio->set_volume(v->voice, src.volume);
            audio->set_pitch(v->voice, src.pitch);
            if (src.spatial) audio->set_position(v->voice, position(e));
            return;
        }
        if (v && v->sound == src.sound && v->voice.valid() && !alive) { // a one-shot ended
            src.playing = false;
            v->voice = {};
            return;
        }
        if (!src.playing || src.sound.empty()) return;
        const SoundHandle sound = ctx.sound(src.sound);
        const VoiceHandle voice = audio->play(sound, {.bus = src.bus, .volume = src.volume, .pitch = src.pitch, .loop = src.loop,
                                                      .spatial = src.spatial, .position = position(e),
                                                      .min_distance = src.min_distance, .max_distance = src.max_distance});
        new_voices.emplace_back(e, AudioVoice{voice, src.sound});
    });
    for (auto& [e, v] : new_voices) world.add(e, std::move(v));
    audio->update();
}

Vec2 Context::screen_size() const {
    const auto& m = *app_.impl_;
    if (m.swapchain && !m.offscreen_target.valid()) {
        const gpu::Extent2D e = m.swapchain->extent();
        return {static_cast<float>(e.width), static_cast<float>(e.height)};
    }
    return {static_cast<float>(m.desc.width), static_cast<float>(m.desc.height)};
}

void App::Impl::route_input() {
    Context& ctx = *context;
    if (debug_ui && input.pressed(Key::F1)) {
        ctx.show_debug_ui = !ctx.show_debug_ui;
        if (window && ctx.show_debug_ui && window->mouse_captured()) { // the UI needs a cursor
            window->set_mouse_captured(false);
            restore_mouse_capture = true;
        } else if (window && !ctx.show_debug_ui && restore_mouse_capture) {
            window->set_mouse_captured(true);
            restore_mouse_capture = false;
        }
    }
    game_input = input;
    if (debug_ui && ctx.show_debug_ui) { // what the UI used last frame is not the game's
        if (debug_ui->wants_keyboard()) game_input.clear_keyboard();
        if (debug_ui->wants_mouse()) game_input.clear_mouse();
    }
}

void App::Impl::build_debug_ui(Game& game, float delta) {
    Context& ctx = *context;
    ui_layer = {};
    const bool visible = debug_ui && ctx.show_debug_ui;
    if (window && text_input != (visible && debug_ui->wants_text())) {
        text_input = !text_input;
        window->set_text_input(text_input);
    }
    if (!visible) return;
    ASTRAL_PROFILE_SCOPE(&profiler, "debug ui");
    const gpu::Extent2D extent = offscreen_target.valid() ? gpu::Extent2D{desc.width, desc.height}
                                 : swapchain                ? swapchain->extent()
                                                            : gpu::Extent2D{desc.width, desc.height};
    debug_ui->begin(input, delta, extent, window ? window->pixel_density() : 1.0f);
    debug_ui->draw_panels(ctx, material_library.get());
    game.on_debug_ui(ctx);
    debug_ui->end();
    ui_layer = debug_ui->layer();
}

void App::Impl::draw_debug_overlays() {
    Context& ctx = *context;
    if (ctx.debug_show.world_cells && map && map->is_open()) {
        const MapDesc& md = map->map();
        const WorldPosition o = origin.origin();
        for (const CellKey& key : map->attached_cells()) {
            if (key.grid >= md.grids.size()) continue;
            const auto size = static_cast<float>(md.grids[key.grid].cell_size);
            const Vec3 a{static_cast<float>(cell_min_x(md, key) - o.x), 0.05f, static_cast<float>(cell_min_z(md, key) - o.z)};
            const Vec4 color = key.grid == 0 ? Vec4{1.0f, 0.8f, 0.2f, 1} : Vec4{0.4f, 0.8f, 1.0f, 1};
            ctx.debug.line(a, {a.x + size, a.y, a.z}, color);
            ctx.debug.line(a, {a.x, a.y, a.z + size}, color);
            ctx.debug.line({a.x + size, a.y, a.z}, {a.x + size, a.y, a.z + size}, color);
            ctx.debug.line({a.x, a.y, a.z + size}, {a.x + size, a.y, a.z + size}, color);
        }
        for (const Vec3& p : streaming_sources()) {
            if (!md.grids.empty()) ctx.debug.circle(p, {0, 1, 0}, static_cast<float>(md.grids[0].load_range), {0.2f, 1.0f, 0.4f, 1});
        }
    }
    if (ctx.debug_show.physics) {
        world.each<const RigidBody, const WorldTransform>([&](const RigidBody& rb, const WorldTransform& w) {
            const Vec3 p{w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
            const Quat q = rotation_of(w.matrix);
            const Vec4 color = rb.desc.motion == MotionType::Dynamic   ? Vec4{0.3f, 1.0f, 0.4f, 1}
                               : rb.desc.motion == MotionType::Kinematic ? Vec4{0.3f, 0.6f, 1.0f, 1}
                                                                        : Vec4{0.6f, 0.6f, 0.6f, 1};
            const auto draw = [&](auto&& self, const ShapeDesc& s, Vec3 pos, Quat rot) -> void {
                switch (s.type) {
                case ShapeType::Box: ctx.debug.box(pos, s.half_extents, rot, color); break;
                case ShapeType::Sphere: ctx.debug.sphere(pos, s.radius, color); break;
                case ShapeType::Capsule: ctx.debug.capsule(pos, rot, s.radius, s.half_height, color); break;
                case ShapeType::Cylinder: ctx.debug.cylinder(pos, rot, s.radius, s.half_height, color); break;
                case ShapeType::ConvexHull: { // its bounding box
                    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
                    for (const Vec3& v : s.points) {
                        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
                        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
                    }
                    ctx.debug.box(pos + rot * ((lo + hi) * 0.5f), (hi - lo) * 0.5f, rot, color);
                    break;
                }
                case ShapeType::Mesh: {
                    const std::size_t triangles = std::min<std::size_t>(s.indices.size() / 3, 4000);
                    for (std::size_t i = 0; i < triangles; ++i) {
                        const Vec3 a2 = pos + rot * s.vertices[s.indices[i * 3]], b2 = pos + rot * s.vertices[s.indices[i * 3 + 1]],
                                   c2 = pos + rot * s.vertices[s.indices[i * 3 + 2]];
                        ctx.debug.line(a2, b2, color);
                        ctx.debug.line(b2, c2, color);
                        ctx.debug.line(c2, a2, color);
                    }
                    break;
                }
                case ShapeType::HeightField: { // every 4th row and column
                    const float half = 0.5f * static_cast<float>(s.samples - 1) * s.cell_size;
                    const auto at = [&](std::uint32_t x, std::uint32_t z) {
                        return pos + rot * Vec3{static_cast<float>(x) * s.cell_size - half, s.heights[z * s.samples + x], static_cast<float>(z) * s.cell_size - half};
                    };
                    for (std::uint32_t z = 0; z < s.samples; z += 4) {
                        for (std::uint32_t x = 0; x + 1 < s.samples; ++x) ctx.debug.line(at(x, z), at(x + 1, z), color);
                    }
                    for (std::uint32_t x = 0; x < s.samples; x += 4) {
                        for (std::uint32_t z = 0; z + 1 < s.samples; ++z) ctx.debug.line(at(x, z), at(x, z + 1), color);
                    }
                    break;
                }
                case ShapeType::Compound:
                    for (const ShapeChild& child : s.children) self(self, child.shape, pos + rot * child.position, rot * child.rotation);
                    break;
                }
            };
            draw(draw, rb.desc.shape, p, q);
        });
        world.each<const CharacterController, const WorldTransform>([&](const CharacterController& cc, const WorldTransform& w) {
            const float half = cc.desc.height * 0.5f; // feet at the entity position
            ctx.debug.capsule({w.matrix[3].x, w.matrix[3].y + half, w.matrix[3].z}, Quat{}, cc.desc.radius,
                              std::max(0.0f, half - cc.desc.radius), cc.on_ground ? Vec4{1, 0.85f, 0.2f, 1} : Vec4{1, 0.4f, 0.2f, 1});
        });
    }
    if (ctx.debug_show.fields) {
        world.each<const WindZone, const WorldTransform>([&](const WindZone& z, const WorldTransform& w) {
            const Vec3 p{w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
            if (!z.global) ctx.debug.box(p, z.half_extents, Quat{}, {0.5f, 0.9f, 1.0f, 1});
            const float speed = length(z.velocity);
            if (speed > 0.0f) ctx.debug.arrow(p, p + normalize(z.velocity) * std::min(1.0f + speed * 0.3f, 6.0f), {0.5f, 0.9f, 1.0f, 1}, {.on_top = true});
        });
        world.each<const WaterBody, const WorldTransform>([&](const WaterBody& b, const WorldTransform& w) {
            ctx.debug.box({w.matrix[3].x, w.matrix[3].y, w.matrix[3].z}, b.half_extents, Quat{}, {0.2f, 0.4f, 1.0f, 1});
        });
    }
    if (ctx.debug_show.lights) {
        world.each<const PointLight>([&](Entity e, const PointLight& l) {
            Vec3 p = l.position;
            if (const WorldTransform* w = world.get<WorldTransform>(e)) p = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
            const Vec4 c{l.color.x, l.color.y, l.color.z, 1};
            ctx.debug.cross(p, 0.15f, c, {.on_top = true});
            ctx.debug.sphere(p, l.radius, {c.x, c.y, c.z, 0.35f});
        });
        world.each<const RectLight>([&](Entity e, const RectLight& l) {
            Vec3 p = l.position, r = l.right, u = l.up;
            if (const WorldTransform* w = world.get<WorldTransform>(e)) {
                p = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                const Vec4 rr = w->matrix * Vec4{r.x, r.y, r.z, 0}, uu = w->matrix * Vec4{u.x, u.y, u.z, 0};
                r = {rr.x, rr.y, rr.z};
                u = {uu.x, uu.y, uu.z};
            }
            const Vec4 c{l.color.x, l.color.y, l.color.z, 1};
            const Vec3 a = p - r - u, b = p + r - u, cc = p + r + u, d = p - r + u;
            ctx.debug.line(a, b, c, {.on_top = true});
            ctx.debug.line(b, cc, c, {.on_top = true});
            ctx.debug.line(cc, d, c, {.on_top = true});
            ctx.debug.line(d, a, c, {.on_top = true});
            ctx.debug.arrow(p, p + normalize(cross(r, u)) * 0.5f, c, {.on_top = true});
        });
    }
}

void App::Impl::end_frame_profile() { profiler.end_frame(); }

// --- Maps: streaming and floating origin (ADR-0021) --------------------------------------------

std::vector<Vec3> App::Impl::streaming_sources() {
    std::vector<Vec3> out;
    auto position_of = [&](Entity e) -> std::optional<Vec3> {
        // A top-level Transform is current even before this frame's transform update.
        if (!world.has<Parent>(e)) {
            if (const Transform* t = world.get<const Transform>(e)) return t->position;
        }
        if (const WorldTransform* w = world.get<const WorldTransform>(e)) return Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        return std::nullopt;
    };
    world.each<const StreamingSource>([&](Entity e, const StreamingSource& s) {
        if (!s.active) return;
        if (auto p = position_of(e)) out.push_back(*p);
    });
    if (out.empty()) {
        world.each<const Camera>([&](Entity e, const Camera& c) {
            if (!out.empty() || !c.active) return;
            if (auto p = position_of(e)) out.push_back(*p);
        });
    }
    return out;
}

void App::Impl::update_world(bool finish) {
    if (!map || !map->is_open()) return;
    std::vector<Vec3> sources = streaming_sources();
    if (!sources.empty()) {
        if (auto offset = origin.update(sources.front())) {
            const Vec3 off = *offset;
            shift_roots(world, off);
            physics->shift_origin(off);
            for (BodyLink& link : body_links) link.previous_position = link.previous_position - off;
            world.each<RenderHistory>([&](RenderHistory& h) {
                h.previous[3].x -= off.x;
                h.previous[3].y -= off.y;
                h.previous[3].z -= off.z;
            });
            if (renderer) renderer->reset_history();
            for (Vec3& p : sources) p = p - off;
            map->set_origin(origin.origin());
            world.send(OriginShifted{off, origin.origin()});
        }
    }
    stream_points.clear();
    for (const Vec3& p : sources) {
        const WorldPosition w = origin.to_world(p);
        stream_points.push_back({w.x, w.z});
    }
    if (finish) map->finish(stream_points);
    else map->update(stream_points, static_cast<double>(desc.world_stream_budget_ms) / 1000.0);
}

namespace {
std::uint64_t content_hash(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}
} // namespace

void App::Impl::run_scripts(ScriptPhase phase) {
    ASTRAL_PROFILE_SCOPE(&profiler, "scripts");
    scripts->run(phase, {.delta = time.delta, .fixed_delta = time.fixed_delta, .elapsed = time.elapsed});
}

std::vector<std::string> Context::load_script(std::string_view path) {
    auto& m = *app_.impl_;
    std::string error;
    const auto bytes = m.assets->read(path, &error);
    if (!bytes) {
        log.log(LogLevel::Error, "script " + std::string(path) + ": " + error);
        return {std::string(path) + ": " + error};
    }
    m.watched_files[std::string(path)] = {content_hash(*bytes), false};
    return m.scripts->load(path, std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

bool Context::write_script_api(std::string_view folder) {
    const ScriptApi api = generate_script_api(types, events, api_owners);
    std::error_code ec;
    const std::filesystem::path dir{std::string(folder)};
    std::filesystem::create_directories(dir, ec);
    for (const auto& [name, text] : {std::pair<const char*, const std::string&>{"astral.d.luau", api.luau_types}, {"API.md", api.markdown}}) {
        std::ofstream out(dir / name, std::ios::binary);
        out << text;
        if (!out) {
            log.log(LogLevel::Error, "script api: cannot write " + (dir / name).string());
            return false;
        }
    }
    return true;
}

bool Context::load_actions(std::string_view path) {
    auto& m = *app_.impl_;
    std::string error;
    const auto bytes = m.assets->read(path, &error);
    const auto fail = [&](const std::string& why) {
        log.log(LogLevel::Error, "actions " + std::string(path) + ": " + why);
        return false;
    };
    if (!bytes) return fail(error);
    const auto doc = parse_json_asset(*bytes, &error);
    if (!doc) return fail(error);
    m.watched_files[std::string(path)] = {content_hash(*bytes), true};
    return actions.load(*doc, &error) || fail(error);
}

void App::Impl::reload_changed_scripts() {
    if (!desc.hot_reload || !desc.pack.empty() || watched_files.empty() || time.elapsed < next_watch) return;
    next_watch = time.elapsed + 0.5;
    std::vector<std::pair<std::string, bool>> changed;
    for (auto& [path, w] : watched_files) {
        const auto bytes = assets->read(path);
        if (bytes && content_hash(*bytes) != w.hash) changed.emplace_back(path, w.is_actions);
    }
    for (const auto& [path, is_actions] : changed) {
        if (is_actions) context->load_actions(path);
        else context->load_script(path);
        log.info("reloaded {}", path);
    }
}

MaterialHandle Context::load_material(std::string_view path) {
    auto& library = app_.impl_->material_library;
    return library ? library->get(path) : MaterialHandle{};
}

std::string_view Context::shader_error() const noexcept {
    return renderer ? renderer->shader_error() : std::string_view{};
}

bool Context::open_world(std::string_view path) {
    auto& m = *app_.impl_;
    if (!m.map) m.map = std::make_unique<MapRuntime>(world, *prefabs, *assets, jobs, log);
    m.map->set_origin(m.origin.origin());
    return m.map->open(path, m.asset_root);
}

void Context::close_world() {
    if (auto& map = app_.impl_->map) map->close();
}

void Context::finish_world_loading() {
    app_.impl_->update_world(true);
}

MapLayers* Context::world_layers() noexcept {
    auto& map = app_.impl_->map;
    return map && map->is_open() ? map->layers() : nullptr;
}

const StreamingStats* Context::world_streaming() const noexcept {
    const auto& map = app_.impl_->map;
    return map && map->is_open() ? map->stats() : nullptr;
}

WorldPosition Context::world_origin() const noexcept {
    return app_.impl_->origin.origin();
}

} // namespace astral
