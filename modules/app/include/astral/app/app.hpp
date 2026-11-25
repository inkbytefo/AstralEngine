#pragma once

#include <astral/app/audio_service.hpp>
#include <astral/app/components.hpp>
#include <astral/app/input_actions.hpp>
#include <astral/app/map_service.hpp>
#include <astral/app/material_service.hpp>
#include <astral/app/physics_service.hpp>
#include <astral/app/scene_service.hpp>
#include <astral/app/script_service.hpp>
#include <astral/app/system.hpp>
#include <astral/app/viewport_service.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/platform/input.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/render/renderer.hpp>
#include <astral/render/text.hpp>
#include <astral/script/api.hpp>
#include <astral/script/script.hpp>
#include <astral/world/map_state.hpp>
#include <astral/world/origin.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/streaming.hpp>
#include <astral/world/serialize.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <concepts>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <string_view>
#include <type_traits>
#include <utility>

namespace astral {

class Window;
namespace gpu {
class Device;
}
class Context;

/// Your game. Override what you need; spawn entities and add systems in on_start().
class Game {
public:
    virtual ~Game() = default;
    virtual void on_start(Context&) {}
    virtual void on_update(Context&) {}
    virtual void on_shutdown(Context&) {}
    /// While the debug UI is open (F1): add your own panels with Dear ImGui (#include <imgui.h>).
    virtual void on_debug_ui(Context&) {}
};

/// Ready-made meshes so a game can draw something without loading files.
struct BuiltinMeshes {
    MeshHandle cube;
    MeshHandle sphere;
    MeshHandle plane;
};

/// Everything a game or system can reach. Window/renderer/device are null in headless mode.
class Context {
public:
    World& world;
    const Input& input;
    const Time& time;
    Logger& log;
    JobSystem& jobs;
    Window* window = nullptr;
    gpu::Device* device = nullptr;
    Renderer* renderer = nullptr;
    PhysicsWorld* physics = nullptr; ///< Always available (also headless).
    /// Always available. Paths are relative to AppDesc::asset_root. Headless: model data, no GPU resources.
    AssetManager* assets = nullptr;
    /// Sound output (a silent mixer when headless or without a device). Always available.
    AudioEngine* audio = nullptr;
    /// Prefab templates (from asset documents). Always available.
    PrefabLibrary* prefabs = nullptr;
    /// CPU frame profiler: the App times its phases and every system; add your own scopes with
    /// ASTRAL_PROFILE_SCOPE(ctx.profiler, "name"). GPU pass times: renderer->gpu_timings().
    Profiler* profiler = nullptr;
    BuiltinMeshes meshes{};
    /// Components that scenes, prefabs and save_world/load_world know: the engine's own (Name,
    /// Transform, Parent, Camera, PointLight, RectLight, Sdf, PrimitiveMesh, ModelRef) are registered;
    /// add your ASTRAL_REFLECT'ed ones with types.add<T>().
    TypeRegistry types;
    /// Debug lines for this frame (or longer: DebugStyle::seconds); drawn over the image.
    DebugDraw debug;
    /// The game's 2D overlay for this frame: text, rectangles, images (pixels, origin top-left).
    /// Cleared after every frame; drawn over the 3D image, under the debug UI.
    UiCanvas hud;
    /// The built-in font (Roboto Medium, Turkish letters included). Always available; read-only.
    const Font* font = nullptr;
    /// Size of the image being rendered, pixels (the window's, or AppDesc width x height).
    [[nodiscard]] Vec2 screen_size() const;
    /// What the App draws into `debug` by itself every frame.
    struct DebugShow {
        bool physics = false;        ///< RigidBody shapes (green: dynamic, blue: kinematic, grey: static), joints, and characters.
        bool physics_joints = false; ///< Joint anchors, axes, limits, motors, and broken indicators.
        bool lights = false;         ///< Point light ranges, rect light panels.
        bool fields = false;         ///< WindZone boxes with their direction, WaterBody boxes.
        bool world_cells = false;    ///< Loaded map cells (squares on the ground) and the load range.
    } debug_show;
    /// The engine's debug UI (F1): performance, entities, inspector, render settings. Needs a renderer.
    bool show_debug_ui = false;
    /// A material whose shader is `shader_type sky` (ctx.materials().load_material): replaces the background.
    MaterialHandle sky{};
    /// Materials with `shader_type post` shaders, applied in order after tonemapping.
    std::vector<MaterialHandle> post;
    DirectionalLight sun{};   ///< The sun (direction, color, shadows).
    Environment environment{}; ///< Sky/ground ambient and exposure.

    /// Wind and water now (ADR-0029): rebuilt from WindZone / WaterBody entities at the start of every fixed step. Sample it
    /// from systems: ctx.fields.wind_at(position, ctx.time.elapsed), ctx.fields.water_at(...).
    FieldWorld fields;
    /// Events by name (scripts send and listen through it): add your ASTRAL_REFLECT'ed events with
    /// events.add<T>(); scripts can define their own with astral.define_event.
    EventRegistry events;
    /// Named input actions ("move", "fire"): fill with ctx.script_service().load_actions(); scripts read them as astral.input.
    InputActions actions;
    /// The Luau host (ADR-0017). Always available; scripts run in the phases of the frame, after the C++
    /// systems of the same phase.
    ScriptHost* scripts = nullptr;
    /// Which feature owns a component or event (filled by FeatureSet::install; shown in API.md).
    std::map<std::string, std::string> api_owners;

    template <class S, class... Args>
        requires std::derived_from<S, System>
    S& add_system(Phase phase, Args&&... args) {
        auto system = std::make_unique<S>(std::forward<Args>(args)...);
        S& ref = *system;
        register_system(phase, std::move(system));
        return ref;
    }

    /// Registers a lightweight function/lambda as a named System in `phase`.
    template <class Fn>
        requires std::invocable<Fn&, Context&> && (!std::derived_from<std::remove_cvref_t<Fn>, System>)
    System& add_system(Phase phase, std::string_view name, Fn&& fn, SystemAccess access = {}) {
        auto system = std::make_unique<FunctionSystem>(std::forward<Fn>(fn), std::move(access));
        System& ref = *system;
        register_named_system(phase, std::string(name), std::move(system));
        return ref;
    }

    /// Returns the first component of type `T` in `world` without marking it changed, or nullptr if none exists.
    template <class T> [[nodiscard]] const std::remove_const_t<T>* singleton() const {
        using C = std::remove_const_t<T>;
        const C* found = nullptr;
        world.each<const C>([&](const C& component) {
            if (!found) found = &component;
        });
        return found;
    }

    /// Returns a mutable pointer to the first component of type `T` in `world` (marks `T` changed), or nullptr.
    template <class T> [[nodiscard]] std::remove_const_t<T>* singleton_mut() {
        using C = std::remove_const_t<T>;
        Entity target{};
        world.each<const C>([&](Entity entity, const C&) {
            if (!target.valid()) target = entity;
        });
        return target.valid() ? world.get<C>(target) : nullptr;
    }

    /// Grouped engine services.
    [[nodiscard]] SceneService& scenes() noexcept;
    [[nodiscard]] const SceneService& scenes() const noexcept;
    [[nodiscard]] MapService& maps() noexcept;
    [[nodiscard]] const MapService& maps() const noexcept;
    [[nodiscard]] MaterialService& materials() noexcept;
    [[nodiscard]] const MaterialService& materials() const noexcept;
    [[nodiscard]] AudioService& audio_service() noexcept;
    [[nodiscard]] const AudioService& audio_service() const noexcept;
    [[nodiscard]] ScriptService& script_service() noexcept;
    [[nodiscard]] const ScriptService& script_service() const noexcept;
    [[nodiscard]] PhysicsService& physics_service() noexcept;
    [[nodiscard]] const PhysicsService& physics_service() const noexcept;
    [[nodiscard]] ViewportService& viewport() noexcept;
    [[nodiscard]] const ViewportService& viewport() const noexcept;

    /// Ends the run loop after the current frame.
    void quit() noexcept { quit_requested_ = true; }
    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }

    /// Enables or disables parallel execution of non-conflicting access-declaring systems (ADR-0032).
    void set_parallel_systems(bool enabled) noexcept;
    [[nodiscard]] bool parallel_systems() const noexcept;

private:
    friend class App;
    friend Entity play_vfx(Context& context, std::string_view asset, Vec3 position);
    Context(World& w, const Input& i, const Time& t, Logger& l, JobSystem& j, class App& app)
        : world(w), input(i), time(t), log(l), jobs(j), app_(app) {}
    void register_system(Phase phase, std::unique_ptr<System> system);
    void register_named_system(Phase phase, std::string name, std::unique_ptr<System> system);
    class App& app_;
    bool quit_requested_ = false;
};

struct AppDesc {
    std::string_view title = "Astral";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool vsync = true;
    bool headless = false;        ///< No window, no GPU: for servers and tests.
    bool validation = false;      ///< Vulkan validation layer (slow; for debugging).
    float fixed_timestep = 0.0f;  ///< > 0: every frame advances exactly this much time.
    unsigned worker_threads = 0;  ///< 0 = hardware threads - 1.
    float physics_rate = 60.0f;   ///< Fixed physics steps per second.
    std::uint32_t max_physics_steps = 4; ///< Per frame; slower frames drop time instead of stalling.
    RenderSettings render = {};   ///< Renderer quality/technique settings (ray tracing, GI, denoiser...).
    std::string_view asset_root = "assets"; ///< Folder that asset paths (ModelRef, ctx.assets) are relative to.
    /// A .apak to read assets from instead (shipped games; relative = next to the executable). Used
    /// when the file exists, so a development build without one reads asset_root with hot reload.
    /// astral_add_game() defines ASTRAL_GAME_ASSETS and ASTRAL_GAME_PACK for these two fields.
    std::string_view pack = {};
    bool hot_reload = true;       ///< Reload assets when their files change on disk.
    /// Render into an internal width x height image instead of a window (captures, image tests);
    /// read it with App::read_frame(). Ignored when headless.
    bool offscreen = false;
    /// F1 opens the debug UI (Context::show_debug_ui). Off: no ImGui at all.
    bool debug_ui = true;
    /// Draw built-in F1 debug panels and menu bar (false for standalone editors that own the whole ImGui dockspace).
    bool debug_ui_builtin_panels = true;
    /// Optional ImGui layout persistence file (.ini). Empty = no file written (default for games).
    std::string_view debug_ui_ini_path = {};
    /// Play sound on the default device (headless and offscreen always mix silently).
    bool audio = true;
    /// Open maps: the origin moves to the first streaming source when it is farther than this (metres).
    float origin_shift_distance = 2000.0f;
    /// Open maps: time per frame for spawning streamed cells (at least one cell per frame).
    float world_stream_budget_ms = 1.0f;
    /// Folder of save slots (astral.save from scripts): one `<slot>.json` each. Relative = the working folder.
    std::string_view save_folder = "saves";
    /// Smooth motion between physics steps: dynamic bodies are drawn between their last two simulated poses (up to one step behind)
    /// instead of jumping once per step. Matters when physics_rate is below the frame rate. Transform then lags the simulation a little.
    bool physics_interpolation = false;
    /// Run non-conflicting access-declaring C++ systems in parallel waves on the JobSystem (ADR-0032).
    bool parallel_systems = true;
};

/// Owns the main loop and every engine service; your Game plugs into it.
///
///     int main() {
///         astral::App app({.title = "Sword Arena"});
///         return app.run<ArenaGame>();
///     }
class App {
public:
    explicit App(const AppDesc& desc = {});
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// Runs until the window closes or ctx.quit(). Returns 0 on a clean exit.
    int run(Game& game);
    template <class G, class... Args> int run(Args&&... args) {
        static_assert(std::is_base_of_v<Game, G>, "G must derive from astral::Game");
        G game(std::forward<Args>(args)...);
        return run(game);
    }
    /// Runs exactly `frames` frames (start and shutdown included). For tests and tools.
    void run_frames(Game& game, std::uint32_t frames);

    [[nodiscard]] Context& context() noexcept;
    [[nodiscard]] World& world() noexcept;
    [[nodiscard]] const Time& time() const noexcept;
    /// Offscreen mode: the last rendered frame, RGBA8, width*height*4 bytes. False otherwise.
    bool read_frame(std::vector<std::uint8_t>& rgba) const;

private:
    friend class Context;
    friend Entity play_vfx(Context& context, std::string_view asset, Vec3 position);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
