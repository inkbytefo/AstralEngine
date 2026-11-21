#pragma once

#include <astral/app/components.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/platform/input.hpp>
#include <astral/render/renderer.hpp>
#include <astral/world/serialize.hpp>
#include <astral/world/world.hpp>

#include <cstdint>
#include <memory>
#include <span>
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

private:
    friend class App;
    Tick last_run_ = 0;
};

/// Your game. Override what you need; spawn entities and add systems in on_start().
class Game {
public:
    virtual ~Game() = default;
    virtual void on_start(Context&) {}
    virtual void on_update(Context&) {}
    virtual void on_shutdown(Context&) {}
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
    BuiltinMeshes meshes{};
    /// Components that save_world/load_world know: the engine's own (Transform, Parent, Camera,
    /// PointLight, RectLight, Sdf) are registered; add your ASTRAL_REFLECT'ed ones with types.add<T>().
    TypeRegistry types;
    DirectionalLight sun{};   ///< The sun (direction, color, shadows).
    Environment environment{}; ///< Sky/ground ambient and exposure.

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

private:
    friend class Context;
    struct Impl;
    static void run_phase(std::vector<std::unique_ptr<System>>& systems, Context& ctx);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
