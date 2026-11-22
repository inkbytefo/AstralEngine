#pragma once

#include <astral/app/components.hpp>
#include <astral/app/input_actions.hpp>
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

/// When a system runs inside a frame:
/// input → PreUpdate → Game::on_update → Update → physics (FixedUpdate systems + step, 0..n
/// times at a fixed rate) → PostUpdate → transforms → render. Commands are applied after each.
enum class Phase : std::uint8_t { PreUpdate, Update, FixedUpdate, PostUpdate };

/// Game logic that runs every frame. Read and write components through ctx.world.
/// Inside update(), `added<T>()`/`changed<T>()` query filters mean "since this system last ran".
class System {
public:
    virtual ~System() = default;
    virtual void update(Context& ctx) = 0;
    /// Its type name ("PlayerControl"): how the profiler and debug UI show it.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    friend class App;
    friend class Context;
    Tick last_run_ = 0;
    std::string name_;
};

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
    /// Loads a sound by asset path once (from the pack or the asset folder) and returns it; invalid
    /// (and logged once) if it cannot be read or decoded. ctx.audio->play(ctx.sound("sfx/hit.wav")).
    SoundHandle sound(std::string_view path);
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
    /// The built-in font (Roboto Medium, Turkish letters included). Always available.
    Font* font = nullptr;
    /// Size of the image being rendered, pixels (the window's, or AppDesc width x height).
    [[nodiscard]] Vec2 screen_size() const;
    /// What the App draws into `debug` by itself every frame.
    struct DebugShow {
        bool physics = false; ///< RigidBody shapes (green: dynamic, blue: kinematic, grey: static) and characters.
        bool lights = false;  ///< Point light ranges, rect light panels.
        bool fields = false;  ///< WindZone boxes with their direction, WaterBody boxes.
        bool world_cells = false; ///< Loaded map cells (squares on the ground) and the load range.
    } debug_show;
    /// The engine's debug UI (F1): performance, entities, inspector, render settings. Needs a renderer.
    bool show_debug_ui = false;
    /// A material whose shader is `shader_type sky` (ctx.load_material): replaces the background.
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
    /// Named input actions ("move", "fire"): fill with load_actions(); scripts read them as astral.input.
    InputActions actions;
    /// The Luau host (ADR-0017). Always available; scripts run in the phases of the frame, after the C++
    /// systems of the same phase.
    ScriptHost* scripts = nullptr;
    /// Loads (or reloads) a script file relative to the asset root; it is reloaded when the file
    /// changes (development builds). Returns the problems as "file:line: message" (empty = fine; they are
    /// logged too). A failing reload keeps the previous version running.
    std::vector<std::string> load_script(std::string_view path);
    /// Which feature owns a component or event (filled by FeatureSet::install; shown in API.md).
    std::map<std::string, std::string> api_owners;
    /// Writes astral.d.luau (Luau type definitions) and API.md (reference) for everything registered now:
    /// call after the features are installed. False (logged) if a file cannot be written.
    bool write_script_api(std::string_view folder);
    /// Loads an .actions file (see input_actions.hpp) into `actions`; reloaded when edited. False (logged)
    /// if it cannot be read or is invalid.
    bool load_actions(std::string_view path);

    template <class S, class... Args> S& add_system(Phase phase, Args&&... args) {
        auto system = std::make_unique<S>(std::forward<Args>(args)...);
        S& ref = *system;
        register_system(phase, std::move(system));
        return ref;
    }
    /// Render resources. Safe in headless mode: they return an invalid handle (nothing is drawn).
    MeshHandle create_mesh(const MeshData& data);
    MaterialHandle create_material(const MaterialDesc& desc);
    /// False in headless mode or for a dead handle.
    bool update_material(MaterialHandle material, const MaterialDesc& desc);

    /// A material from a .material file (ADR-0022): its shader (.ashader) and values, with the parent chain
    /// applied. Loaded on first use and cached; the handle stays valid while the file is edited (hot reload
    /// updates it in place; a version that does not compile is logged and the last good one stays). Invalid
    /// (logged) if it cannot be built, and in headless mode. Draw with MeshRenderer, or give a
    /// PrimitiveMesh a material_file.
    MaterialHandle load_material(std::string_view path);
    /// Why the custom shaders could not be compiled into the ray-traced shaders ("" = fine).
    [[nodiscard]] std::string_view shader_error() const noexcept;
    /// Appends a scene file (relative to the asset root) to the world. Its "sun" and "environment",
    /// if present, replace ctx.sun / ctx.environment. Problems are logged and returned.
    LoadResult load_scene(std::string_view path);
    /// Writes the world as a scene file (with ctx.sun / ctx.environment). False if it cannot be written.
    bool save_scene(std::string_view path);
    /// Opens a map (.world, ADR-0021) in place of the current one: its always-loaded entities spawn now,
    /// its cells stream in and out around StreamingSource entities (else the active camera), and the
    /// floating origin follows the first source. Works with source maps (development) and cooked ones.
    /// False (logged) if the map cannot be read.
    bool open_world(std::string_view path);
    /// Removes the open map's entities and forgets it.
    void close_world();
    /// Blocks until every cell in range is loaded and spawned (loading screens, tests).
    void finish_world_loading();
    /// The open map's data layers (set("night", true)); null without an open map.
    [[nodiscard]] MapLayers* world_layers() noexcept;
    /// Cell counts of the open map; null without one.
    [[nodiscard]] const StreamingStats* world_streaming() const noexcept;
    /// World position of local zero. Transform positions are relative to it; OriginShifted events tell
    /// when it moves (only while a map is open).
    [[nodiscard]] WorldPosition world_origin() const noexcept;

    /// Creates an instance of a prefab now: its parts exist when this returns. Components of the
    /// prefab's root other than Transform come from the prefab.
    Entity spawn_prefab(std::string_view path, const Transform& transform = {});

    /// Contacts that began or ended during this frame's physics steps.
    [[nodiscard]] std::span<const ContactEvent> contacts() const noexcept;

    /// Ends the run loop after the current frame.
    void quit() noexcept { quit_requested_ = true; }
    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }

private:
    friend class App;
    Context(World& w, const Input& i, const Time& t, Logger& l, JobSystem& j, class App& app)
        : world(w), input(i), time(t), log(l), jobs(j), app_(app) {}
    void register_system(Phase phase, std::unique_ptr<System> system);
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
    struct Impl;
    static void run_phase(std::vector<std::unique_ptr<System>>& systems, Context& ctx);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
