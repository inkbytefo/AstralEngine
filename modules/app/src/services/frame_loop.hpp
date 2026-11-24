#pragma once
// Internal to Astral::App: owns the ordered frame pipeline, system phase storage, and frame clock.

#include "atmosphere.hpp"
#include "vfx_runtime.hpp"

#include <astral/app/app.hpp>
#include <astral/core/clock.hpp>

#include <array>
#include <memory>
#include <vector>

namespace astral {

class AssetManager;
class AudioBridge;
class DevTools;
class Input;
class Logger;
class PhysicsBridge;
class RenderBridge;
class ScriptBridge;
class World;
class WorldBridge;

class FrameLoop {
public:
    using PhaseRunner = void (*)(std::vector<std::unique_ptr<System>>&, Context&);

    struct Services {
        World& world;
        Context& context;
        const Input& raw_input;
        Input& game_input;
        Time& time;
        AssetManager& assets;
        Logger& log;
        DevTools& dev_tools;
        WorldBridge& world_bridge;
        RenderBridge& render_bridge;
        PhysicsBridge& physics_bridge;
        AudioBridge& audio_bridge;
        ScriptBridge& script_bridge;
        PhaseRunner run_phase;
        float fixed_timestep = 0.0f;
        bool parallel_systems = true;
    };

    explicit FrameLoop(const Services& services);
    ~FrameLoop() = default;
    FrameLoop(const FrameLoop&) = delete;
    FrameLoop& operator=(const FrameLoop&) = delete;

    void add_system(Phase phase, std::unique_ptr<System> system);
    void clear_systems();
    void set_parallel_systems(bool enabled) noexcept { parallel_systems_ = enabled; }
    [[nodiscard]] bool parallel_systems() const noexcept { return parallel_systems_; }

    bool start(Game& game);
    [[nodiscard]] float next_delta();
    void run_frame(Game& game, float delta);
    void finish_world_loading();
    AtmosphereController& atmosphere() noexcept { return atmosphere_; }

private:
    static void run_fixed_update_cb(void* user);
    void route_input();
    void update_world(bool finish);
    void update_assets_and_scenes();
    void run_scripted_phase(Phase phase, ScriptPhase script_phase, const char* scope_name);
    void run_game_update(Game& game);
    void run_physics(float delta);
    void run_fixed_update();
    void update_transforms_and_primitives();
    void update_audio();
    void render_frame(Game& game, float delta);
    void advance_frame_state(float delta);

    World& world_;
    Context& context_;
    const Input& raw_input_;
    Input& game_input_;
    Time& time_;
    AssetManager& assets_;
    Logger& log_;
    DevTools& dev_tools_;
    WorldBridge& world_bridge_;
    RenderBridge& render_bridge_;
    AtmosphereController atmosphere_;
    VfxRuntime vfx_;
    PhysicsBridge& physics_bridge_;
    AudioBridge& audio_bridge_;
    ScriptBridge& script_bridge_;
    PhaseRunner run_phase_;
    float fixed_timestep_;
    bool parallel_systems_ = true;
    Clock clock_;
    std::array<std::vector<std::unique_ptr<System>>, 4> systems_;
    bool started_ = false;
};

} // namespace astral
