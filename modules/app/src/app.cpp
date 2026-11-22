#include <astral/app/app.hpp>

#include "services/audio_bridge.hpp"
#include "services/dev_tools.hpp"
#include "services/frame_loop.hpp"
#include "services/physics_bridge.hpp"
#include "services/render_bridge.hpp"
#include "services/script_bridge.hpp"
#include "services/world_bridge.hpp"

#include <astral/gpu/device.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/paths.hpp>
#include <astral/platform/window.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <typeinfo>
#include <utility>

namespace astral {

namespace {

void register_app_types(Context& ctx) {
    TypeRegistry& types = ctx.types;
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
    ctx.events.add<Contact>();
    ctx.events.add<TriggerEntered>();
    ctx.events.add<TriggerExited>();
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

/// Owns the display and GPU stack in strict reverse-teardown order (renderer -> swapchain -> device -> window).
struct GraphicsBackend {
    std::unique_ptr<Window> window;
    gpu::SurfaceSource surface;
    std::unique_ptr<gpu::Device> device;
    std::unique_ptr<gpu::Swapchain> swapchain;
    std::unique_ptr<Renderer> renderer;

    void init(const AppDesc& desc, Logger& log) {
        if (!desc.headless && desc.offscreen) {
            device = gpu::Device::create({.app_name = desc.title, .validation = desc.validation, .logger = &log});
            if (device) renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, desc.render);
            else log.error("App: no usable GPU for offscreen rendering; running without rendering");
        } else if (!desc.headless) {
            window = Window::create({.title = desc.title, .width = desc.width, .height = desc.height});
            if (!window) {
                log.error("App: could not create a window; running headless");
                return;
            }
            surface = window->surface_source();
            device = gpu::Device::create({.app_name = desc.title, .validation = desc.validation, .surface = &surface, .logger = &log});
            if (!device) {
                log.error("App: no usable GPU; running without rendering");
                return;
            }
            swapchain = gpu::Swapchain::create(*device, surface, {.vsync = desc.vsync});
            if (swapchain) renderer = std::make_unique<Renderer>(*device, swapchain->format(), desc.render);
        }
    }

    void shutdown() {
        renderer.reset();
        swapchain.reset();
        device.reset();
        window.reset();
    }
};

/// Context-bound adapters for grouped services whose operations combine Context fields or multiple bridges.
struct ContextServices {
    class Scenes final : public SceneService {
    public:
        Scenes(WorldBridge& bridge, DirectionalLight& sun, Environment& environment)
            : bridge_(bridge), sun_(sun), environment_(environment) {}
        LoadResult load_scene(std::string_view path) override { return bridge_.load_scene(path, sun_, environment_); }
        bool save_scene(std::string_view path) override { return bridge_.save_scene(path, sun_, environment_); }
        Entity spawn_prefab(std::string_view path, const Transform& transform) override {
            return bridge_.spawn_prefab(path, transform);
        }

    private:
        WorldBridge& bridge_;
        DirectionalLight& sun_;
        Environment& environment_;
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

    ContextServices(WorldBridge& world_bridge, FrameLoop& frame_loop, ScriptBridge& script_bridge, Context& ctx)
        : scenes(world_bridge, ctx.sun, ctx.environment),
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
    Input game_input; ///< What the game sees: input minus what the debug UI took.
    Time time;

    GraphicsBackend graphics;
    std::unique_ptr<AssetManager> assets;
    std::unique_ptr<PrefabLibrary> prefabs;
    std::unique_ptr<Context> context;

    std::unique_ptr<DevTools> dev_tools;
    std::unique_ptr<AudioBridge> audio_bridge;
    std::unique_ptr<PhysicsBridge> physics_bridge;
    std::unique_ptr<RenderBridge> render_bridge;
    std::unique_ptr<WorldBridge> world_bridge;
    std::unique_ptr<ScriptBridge> script_bridge;
    std::unique_ptr<FrameLoop> frame_loop;
    std::unique_ptr<ContextServices> services;

    explicit Impl(unsigned worker_threads) : jobs(worker_threads) {}
};

App::App(const AppDesc& desc) : impl_(std::make_unique<Impl>(desc.worker_threads)) {
    auto& m = *impl_;
    m.log.add_sink(std::make_shared<ConsoleSink>());
    m.context.reset(new Context(m.world, m.game_input, m.time, m.log, m.jobs, *this));
    register_app_types(*m.context);
    m.time.fixed_delta = 1.0f / std::max(desc.physics_rate, 1.0f);

    m.graphics.init(desc, m.log);
    m.context->window = m.graphics.window.get();
    m.context->device = m.graphics.device.get();
    m.context->renderer = m.graphics.renderer.get();

    const std::filesystem::path asset_root(desc.asset_root);
    const std::filesystem::path pack = resolve_pack_path(desc.pack, m.log);
    m.assets = std::make_unique<AssetManager>(AssetManagerDesc{.root = asset_root,
                                                               .renderer = m.graphics.renderer.get(),
                                                               .jobs = &m.jobs,
                                                               .logger = &m.log,
                                                               .hot_reload = desc.hot_reload,
                                                               .reload_poll_seconds = 0.5f,
                                                               .pack = pack});
    m.context->assets = m.assets.get();
    m.prefabs = make_prefab_library(m.context->types, *m.assets);
    m.context->prefabs = m.prefabs.get();

    m.dev_tools = std::make_unique<DevTools>(m.world, m.graphics.window.get(), m.graphics.renderer.get(), desc.debug_ui);
    m.context->profiler = &m.dev_tools->profiler();
    m.context->font = m.dev_tools->font();

    const bool audio_device = desc.audio && !desc.headless && !desc.offscreen; // captures and tests mix silently
    m.audio_bridge = std::make_unique<AudioBridge>(m.world, *m.assets, m.log, audio_device);
    m.context->audio = m.audio_bridge->engine();

    m.physics_bridge = std::make_unique<PhysicsBridge>(
        m.world, m.context->fields, m.time, m.context->profiler, m.log,
        PhysicsBridgeConfig{.max_steps = desc.max_physics_steps, .interpolation = desc.physics_interpolation});
    m.context->physics = m.physics_bridge->world();

    m.render_bridge = std::make_unique<RenderBridge>(
        m.world, *m.assets, m.log, m.graphics.device.get(), m.graphics.swapchain.get(), m.graphics.window.get(),
        m.graphics.renderer.get(),
        RenderBridgeConfig{.width = desc.width,
                           .height = desc.height,
                           .offscreen = !desc.headless && desc.offscreen,
                           .asset_root = asset_root,
                           .watch_materials = desc.hot_reload && pack.empty()});
    m.context->meshes = m.render_bridge->builtin_meshes();

    m.world_bridge = std::make_unique<WorldBridge>(m.world, *m.prefabs, *m.assets, m.jobs, m.log, asset_root,
                                                   desc.origin_shift_distance, desc.world_stream_budget_ms);

    m.script_bridge = std::make_unique<ScriptBridge>(
        m.world, *m.assets, m.context->types, m.context->events, m.context->actions, m.time, m.context->profiler, m.log,
        m.physics_bridge->script_physics(), std::filesystem::path(std::string(desc.save_folder)),
        desc.hot_reload && desc.pack.empty());
    m.context->scripts = m.script_bridge->host();

    m.frame_loop = std::make_unique<FrameLoop>(FrameLoop::Services{
        .world = m.world,
        .context = *m.context,
        .raw_input = m.input,
        .game_input = m.game_input,
        .time = m.time,
        .assets = *m.assets,
        .log = m.log,
        .dev_tools = *m.dev_tools,
        .world_bridge = *m.world_bridge,
        .render_bridge = *m.render_bridge,
        .physics_bridge = *m.physics_bridge,
        .audio_bridge = *m.audio_bridge,
        .script_bridge = *m.script_bridge,
        .run_phase = &App::run_phase,
        .fixed_timestep = desc.fixed_timestep,
    });
    m.services = std::make_unique<ContextServices>(*m.world_bridge, *m.frame_loop, *m.script_bridge, *m.context);
}

App::~App() {
    auto& m = *impl_;
    if (m.graphics.device) m.graphics.device->wait_idle();
    // Systems may release GPU resources in their destructors: they go first, while the
    // renderer and device still exist. Then the services, device last, window after it.
    m.frame_loop->clear_systems();
    m.services.reset();
    m.frame_loop.reset();
    m.context->scripts = nullptr;
    m.script_bridge.reset();
    m.world_bridge.reset();  // waits for cell reads in flight
    m.render_bridge.reset(); // materials and shaders belong to the renderer
    m.context->physics = nullptr;
    m.physics_bridge.reset();
    m.context->font = nullptr;
    m.context->profiler = nullptr;
    m.dev_tools.reset(); // debug_ui owns renderer textures; font before renderer
    m.context->audio = nullptr;
    m.audio_bridge.reset();
    m.context->prefabs = nullptr;
    m.prefabs.reset();
    m.context->assets = nullptr;
    m.assets.reset();    // owns renderer resources: before the renderer
    m.graphics.shutdown(); // renderer, swapchain, device, window
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

void App::run_frames(Game& game, std::uint32_t frames) {
    auto& m = *impl_;
    if (!m.frame_loop->start(game)) return;
    for (std::uint32_t i = 0; i < frames && !m.context->quit_requested(); ++i) {
        if (m.graphics.window && !m.graphics.window->poll(m.input)) break;
        m.frame_loop->run_frame(game, m.frame_loop->next_delta());
    }
    game.on_shutdown(*m.context);
    if (m.graphics.device) m.graphics.device->wait_idle();
}

int App::run(Game& game) {
    auto& m = *impl_;
    if (!m.frame_loop->start(game)) return 1;
    while (!m.context->quit_requested()) {
        if (m.graphics.window && !m.graphics.window->poll(m.input)) break;
        if (m.graphics.window && m.graphics.window->minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // don't spin while hidden
            continue;
        }
        m.frame_loop->run_frame(game, m.frame_loop->next_delta());
    }
    game.on_shutdown(*m.context);
    if (m.graphics.device) m.graphics.device->wait_idle();
    return 0;
}

Context& App::context() noexcept { return *impl_->context; }
World& App::world() noexcept { return impl_->world; }
const Time& App::time() const noexcept { return impl_->time; }
bool App::read_frame(std::vector<std::uint8_t>& rgba) const { return impl_->render_bridge->read_frame(rgba); }

// --- Context grouped service accessors ---------------------------------------------------------

void Context::register_system(Phase phase, std::unique_ptr<System> system) {
    system->name_ = demangle(typeid(*system).name());
    app_.impl_->frame_loop->add_system(phase, std::move(system));
}

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

} // namespace astral
