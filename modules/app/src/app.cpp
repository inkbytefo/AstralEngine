#include <astral/app/app.hpp>

#include "graphics_backend.hpp"
#include "services/atmosphere.hpp"
#include "services/audio_bridge.hpp"
#include "services/dev_tools.hpp"
#include "services/frame_loop.hpp"
#include "services/physics_bridge.hpp"
#include "services/render_bridge.hpp"
#include "services/script_bridge.hpp"
#include "services/system_scheduler.hpp"
#include "services/terrain_bridge.hpp"
#include "services/vfx_runtime.hpp"
#include "services/world_bridge.hpp"

#include <astral/platform/paths.hpp>
#include <astral/platform/window.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>
#include <typeinfo>
#include <utility>

namespace astral {

namespace {

AppDesc sanitize_desc(AppDesc desc) {
    if (desc.width == 0) desc.width = 1280;
    if (desc.height == 0) desc.height = 720;
    if (!std::isfinite(desc.physics_rate) || desc.physics_rate <= 0.0f) desc.physics_rate = 60.0f;
    else desc.physics_rate = std::max(desc.physics_rate, 1.0f);
    if (!std::isfinite(desc.fixed_timestep) || desc.fixed_timestep < 0.0f) desc.fixed_timestep = 0.0f;
    if (!std::isfinite(desc.origin_shift_distance) || desc.origin_shift_distance <= 0.0f) desc.origin_shift_distance = 2000.0f;
    if (!std::isfinite(desc.world_stream_budget_ms) || desc.world_stream_budget_ms < 0.0f) desc.world_stream_budget_ms = 1.0f;
    return desc;
}

void register_app_types(Context& ctx) {
    TypeRegistry& types = ctx.types;
    register_world_types(types);
    types.add<Camera>();
    types.add<Atmosphere>();
    register_vfx_types(types, ctx.events);
    types.add<PointLight>();
    types.add<RectLight>();
    types.add<Sdf>();
    types.add<ModelRef>();
    types.add<PrimitiveMesh>();
    types.add<TerrainRef>();
    types.exclude<TerrainTileDraw>();
    types.add<RigidBody>();
    types.add<CharacterController>();
    types.add<AudioSource>();
    types.add<AudioListener>();
    types.add<StreamingSource>();
    types.add<Joint>();
    types.add<JointMotor>();
    types.add<WindZone>();
    types.add<WaterBody>();
    types.add<Buoyant>();
    types.add<AirDrag>();
    types.add<Airfoil>();
    ctx.events.add<Contact>();
    ctx.events.add<TriggerEntered>();
    ctx.events.add<TriggerExited>();
    ctx.events.add<JointBroken>();
    ctx.events.add<JointDetached>();
    RenderBridge::register_types(types);
}

std::filesystem::path resolve_pack_path(std::string_view pack_view, Logger& log) {
    if (pack_view.empty()) return {};
    std::filesystem::path pack(pack_view);
    if (pack.is_relative()) pack = executable_directory() / pack;
    std::error_code ec;
    if (std::filesystem::is_regular_file(pack, ec)) {
        log.info("App: assets from {}", pack.string());
        return pack;
    }
    return {}; // development: loose files
}

std::unique_ptr<PrefabLibrary> make_prefab_library(TypeRegistry& types, AssetManager& assets) {
    return std::make_unique<PrefabLibrary>(types, [&assets](std::string_view path) {
        const AssetId id = assets.load_document(path);
        if (const Document* d = assets.document(id)) return SourceDocument{.json = &d->json, .version = d->version, .error = {}};
        return SourceDocument{.json = nullptr, .version = 0, .error = std::string(assets.error(id))};
    });
}

/// Context-bound adapters for grouped services whose operations combine Context fields or multiple bridges.
struct ContextServices {
    class Scenes final : public SceneService {
    public:
        Scenes(WorldBridge& bridge, AtmosphereController& atmosphere, Context& ctx)
            : bridge_(bridge), atmosphere_(atmosphere), ctx_(ctx) {}
        LoadResult load_scene(std::string_view path) override {
            DirectionalLight sun;
            Environment environment;
            atmosphere_.authored_lighting(ctx_, sun, environment);
            auto result = bridge_.load_scene(path, sun, environment);
            if (result.errors.empty() || !result.entities.empty()) atmosphere_.set_authored_lighting(ctx_, sun, environment);
            return result;
        }
        bool save_scene(std::string_view path) override {
            DirectionalLight sun;
            Environment environment;
            atmosphere_.authored_lighting(ctx_, sun, environment);
            return bridge_.save_scene(path, sun, environment);
        }
        Entity spawn_prefab(std::string_view path, const Transform& transform) override {
            return bridge_.spawn_prefab(path, transform);
        }

    private:
        WorldBridge& bridge_;
        AtmosphereController& atmosphere_;
        Context& ctx_;
    };

    class Maps final : public MapService {
    public:
        Maps(WorldBridge& bridge, FrameLoop& loop) : bridge_(bridge), loop_(loop) {}
        bool open_world(std::string_view path) override { return bridge_.open_world(path); }
        void close_world() override { bridge_.close_world(); }
        void finish_world_loading() override { loop_.finish_world_loading(); }
        MapLayers* world_layers() noexcept override { return bridge_.layers(); }
        const StreamingStats* world_streaming() const noexcept override { return bridge_.streaming(); }
        WorldPosition world_origin() const noexcept override { return bridge_.origin(); }
        MapEntityLookup resolve_map_entity(std::uint64_t id) const override { return bridge_.resolve_map_entity(id); }

    private:
        WorldBridge& bridge_;
        FrameLoop& loop_;
    };

    class Scripts final : public ScriptService {
    public:
        Scripts(ScriptBridge& bridge, const std::map<std::string, std::string>& api_owners)
            : bridge_(bridge), api_owners_(api_owners) {}
        std::vector<std::string> load_script(std::string_view path) override { return bridge_.load_script(path); }
        bool write_script_api(std::string_view folder) override { return bridge_.write_script_api(folder, api_owners_); }
        bool load_actions(std::string_view path) override { return bridge_.load_actions(path); }

    private:
        ScriptBridge& bridge_;
        const std::map<std::string, std::string>& api_owners_;
    };

    ContextServices(WorldBridge& world_bridge, AtmosphereController& atmosphere, FrameLoop& frame_loop,
                    ScriptBridge& script_bridge, Context& ctx)
        : scenes(world_bridge, atmosphere, ctx),
          maps(world_bridge, frame_loop),
          scripts(script_bridge, ctx.api_owners) {}

    Scenes scenes;
    Maps maps;
    Scripts scripts;
};

} // namespace

struct App::Impl {
    Logger log;
    JobSystem jobs;
    World world;
    Input input;      ///< What the window reports.
    std::vector<InputScriptEvent> input_script; ///< AppDesc::input_script, sorted by frame.
    std::size_t input_script_next = 0;
    Input game_input; ///< What the game sees: input minus what the debug UI took.
    Time time;

    GraphicsBackend graphics;
    std::unique_ptr<AssetManager> assets;
    std::unique_ptr<PrefabLibrary> prefabs;
    std::unique_ptr<Context> context;

    std::unique_ptr<SystemScheduler> scheduler;
    std::unique_ptr<DevTools> dev_tools;
    std::unique_ptr<AudioBridge> audio_bridge;
    std::unique_ptr<PhysicsBridge> physics_bridge;
    std::unique_ptr<RenderBridge> render_bridge;
    std::unique_ptr<AtmosphereController> atmosphere;
    std::unique_ptr<VfxRuntime> vfx;
    std::unique_ptr<TerrainBridge> terrain_bridge;
    std::unique_ptr<WorldBridge> world_bridge;
    std::unique_ptr<ScriptBridge> script_bridge;
    std::unique_ptr<FrameLoop> frame_loop;

    /// Before each frame: a new input frame when no window polls, then this frame's scripted keys.
    void feed_input_script() {
        if (!graphics.window()) input.begin_frame();
        while (input_script_next < input_script.size() && input_script[input_script_next].frame <= time.frame) {
            const InputScriptEvent& e = input_script[input_script_next++];
            if (e.frame == time.frame) input.set_key(e.key, e.down);
        }
    }
    std::unique_ptr<ContextServices> services;

    explicit Impl(unsigned worker_threads) : jobs(worker_threads) {}

    void init_runtime(const AppDesc& desc, const std::filesystem::path& asset_root, const std::filesystem::path& pack) {
        const bool audio_device = desc.audio && !desc.headless && !desc.offscreen; // captures and tests mix silently
        audio_bridge = std::make_unique<AudioBridge>(world, *assets, log, audio_device);
        context->audio = audio_bridge->engine();

        physics_bridge = std::make_unique<PhysicsBridge>(
            world, context->fields, time, context->profiler, log,
            PhysicsBridgeConfig{.max_steps = desc.max_physics_steps, .interpolation = desc.physics_interpolation, .jobs = &jobs});
        context->physics = physics_bridge->world();

        render_bridge = std::make_unique<RenderBridge>(
            world, *assets, log, graphics.device(), graphics.swapchain(), graphics.window(), graphics.renderer(),
            RenderBridgeConfig{.width = desc.width,
                               .height = desc.height,
                               .offscreen = !desc.headless && desc.offscreen,
                               .asset_root = asset_root,
                               .watch_materials = desc.hot_reload && pack.empty()});
        context->meshes = render_bridge->builtin_meshes();

        atmosphere = std::make_unique<AtmosphereController>();
        vfx = std::make_unique<VfxRuntime>();
        terrain_bridge = std::make_unique<TerrainBridge>(world, *assets, log, *render_bridge, graphics.renderer(),
                                                         *physics_bridge->world(), jobs, dev_tools->profiler());
        world_bridge = std::make_unique<WorldBridge>(world, *prefabs, *assets, jobs, log, asset_root,
                                                     desc.origin_shift_distance, desc.world_stream_budget_ms);

        script_bridge = std::make_unique<ScriptBridge>(
            world, *assets, context->types, context->events, context->actions, time, context->profiler, log,
            physics_bridge->script_physics(), std::filesystem::path(std::string(desc.save_folder)),
            desc.hot_reload && pack.empty());
        context->scripts = script_bridge->host();

        frame_loop = std::make_unique<FrameLoop>(FrameLoop::Services{
            .world = world,
            .context = *context,
            .raw_input = input,
            .game_input = game_input,
            .time = time,
            .assets = *assets,
            .log = log,
            .scheduler = *scheduler,
            .dev_tools = *dev_tools,
            .world_bridge = *world_bridge,
            .render_bridge = *render_bridge,
            .atmosphere = *atmosphere,
            .vfx = *vfx,
            .physics_bridge = *physics_bridge,
            .terrain_bridge = *terrain_bridge,
            .audio_bridge = *audio_bridge,
            .script_bridge = *script_bridge,
            .fixed_timestep = desc.fixed_timestep,
        });
        services = std::make_unique<ContextServices>(*world_bridge, *atmosphere, *frame_loop, *script_bridge, *context);
    }
};

App::App(const AppDesc& raw_desc) : impl_(std::make_unique<Impl>(raw_desc.worker_threads)) {
    const AppDesc desc = sanitize_desc(raw_desc);
    auto& m = *impl_;
    m.log.add_sink(std::make_shared<ConsoleSink>());
    m.context.reset(new Context(m.world, m.game_input, m.time, m.log, m.jobs, *this));
    register_app_types(*m.context);
    m.time.fixed_delta = 1.0f / desc.physics_rate;
    m.input_script.assign(desc.input_script.begin(), desc.input_script.end());
    std::stable_sort(m.input_script.begin(), m.input_script.end(),
                     [](const InputScriptEvent& a, const InputScriptEvent& b) { return a.frame < b.frame; });

    m.graphics.init(desc, m.log);
    m.context->window = m.graphics.window();
    m.context->device = m.graphics.device();
    m.context->renderer = m.graphics.renderer();

    const std::filesystem::path asset_root(desc.asset_root);
    const std::filesystem::path pack = resolve_pack_path(desc.pack, m.log);
    m.assets = std::make_unique<AssetManager>(AssetManagerDesc{.root = asset_root,
                                                               .renderer = m.graphics.renderer(),
                                                               .jobs = &m.jobs,
                                                               .logger = &m.log,
                                                               .hot_reload = desc.hot_reload,
                                                               .reload_poll_seconds = 0.5f,
                                                               .pack = pack});
    m.context->assets = m.assets.get();
    m.prefabs = make_prefab_library(m.context->types, *m.assets);
    m.context->prefabs = m.prefabs.get();

    m.scheduler = std::make_unique<SystemScheduler>(desc.parallel_systems);
    m.dev_tools = std::make_unique<DevTools>(m.world, m.graphics.window(), m.graphics.renderer(), desc.debug_ui,
                                             desc.debug_ui_builtin_panels, desc.debug_ui_ini_path);
    m.context->profiler = &m.dev_tools->profiler();
    m.context->font = m.dev_tools->font();
    m.init_runtime(desc, asset_root, pack);
}

App::~App() {
    auto& m = *impl_;
    m.graphics.wait_idle();
    // Systems may release GPU or service resources in destructors: destroy them first in phase order
    // while every service, renderer, and device still exists.
    if (m.scheduler) m.scheduler->clear();
    m.services.reset();
    m.frame_loop.reset();
    m.context->scripts = nullptr;
    m.script_bridge.reset();
    m.terrain_bridge.reset();
    m.vfx.reset();
    m.atmosphere.reset();
    m.world_bridge.reset();
    m.render_bridge.reset();
    m.context->physics = nullptr;
    m.physics_bridge.reset();
    m.context->font = nullptr;
    m.context->profiler = nullptr;
    m.dev_tools.reset();
    m.context->audio = nullptr;
    m.audio_bridge.reset();
    m.context->prefabs = nullptr;
    m.prefabs.reset();
    m.context->assets = nullptr;
    m.assets.reset();
    m.scheduler.reset();
    m.graphics.shutdown();
}

void App::run_frames(Game& game, std::uint32_t frames) {
    auto& m = *impl_;
    if (!m.frame_loop->start(game)) return;
    for (std::uint32_t i = 0; i < frames && !m.context->quit_requested(); ++i) {
        if (m.graphics.window() && !m.graphics.window()->poll(m.input)) break;
        m.feed_input_script();
        m.frame_loop->run_frame(game, m.frame_loop->next_delta());
    }
    game.on_shutdown(*m.context);
    m.graphics.wait_idle();
}

int App::run(Game& game) {
    auto& m = *impl_;
    if (!m.frame_loop->start(game)) return 1;
    while (!m.context->quit_requested()) {
        if (m.graphics.window() && !m.graphics.window()->poll(m.input)) break;
        if (m.graphics.window() && m.graphics.window()->minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // don't spin while hidden
            continue;
        }
        m.feed_input_script();
        m.frame_loop->run_frame(game, m.frame_loop->next_delta());
    }
    game.on_shutdown(*m.context);
    m.graphics.wait_idle();
    return 0;
}

Context& App::context() noexcept { return *impl_->context; }
World& App::world() noexcept { return impl_->world; }
const Time& App::time() const noexcept { return impl_->time; }
bool App::read_frame(std::vector<std::uint8_t>& rgba) const { return impl_->render_bridge->read_frame(rgba); }

// --- Context grouped service accessors & helpers -------------------------------------------------

Entity play_vfx(Context& context, std::string_view asset, Vec3 position) {
    return context.app_.impl_->vfx->play(context, asset, position);
}

void Context::register_system(Phase phase, std::unique_ptr<System> system) {
    if (!system) return;
    if (system->name_.empty()) system->name_ = demangle(typeid(*system).name());
    app_.impl_->scheduler->add(phase, std::move(system));
}

void Context::register_named_system(Phase phase, std::string name, std::unique_ptr<System> system) {
    if (!system) return;
    system->name_ = name.empty() ? std::string("FunctionSystem") : std::move(name);
    app_.impl_->scheduler->add(phase, std::move(system));
}

void Context::set_parallel_systems(bool enabled) noexcept { app_.impl_->scheduler->set_parallel(enabled); }
bool Context::parallel_systems() const noexcept { return app_.impl_->scheduler->parallel(); }

Vec2 Context::screen_size() const { return app_.impl_->render_bridge->screen_size(); }

SceneService& Context::scenes() noexcept { return app_.impl_->services->scenes; }
const SceneService& Context::scenes() const noexcept { return app_.impl_->services->scenes; }

MapService& Context::maps() noexcept { return app_.impl_->services->maps; }
const MapService& Context::maps() const noexcept { return app_.impl_->services->maps; }

MaterialService& Context::materials() noexcept { return *app_.impl_->render_bridge; }
const MaterialService& Context::materials() const noexcept { return *app_.impl_->render_bridge; }

AudioService& Context::audio_service() noexcept { return *app_.impl_->audio_bridge; }
const AudioService& Context::audio_service() const noexcept { return *app_.impl_->audio_bridge; }

ScriptService& Context::script_service() noexcept { return app_.impl_->services->scripts; }
const ScriptService& Context::script_service() const noexcept { return app_.impl_->services->scripts; }

PhysicsService& Context::physics_service() noexcept { return *app_.impl_->physics_bridge; }
const PhysicsService& Context::physics_service() const noexcept { return *app_.impl_->physics_bridge; }

ViewportService& Context::viewport() noexcept { return *app_.impl_->render_bridge; }
const ViewportService& Context::viewport() const noexcept { return *app_.impl_->render_bridge; }

} // namespace astral
