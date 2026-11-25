#pragma once
// Internal to Astral::App: orchestrates the per-frame execution pipeline and frame clock.

#include <astral/app/system.hpp>
#include <astral/core/clock.hpp>
#include <astral/core/math.hpp>
#include <astral/script/script.hpp>

#include <vector>

namespace astral {

class AssetManager;
class AtmosphereController;
class AudioBridge;
class Context;
class DevTools;
class Game;
class Input;
class Logger;
class PhysicsBridge;
class RenderBridge;
class ScriptBridge;
class SystemScheduler;
class TerrainBridge;
class VfxRuntime;
class World;
class WorldBridge;
struct Time;

class FrameLoop {
public:
    struct Services {
        World& world;
        Context& context;
        const Input& raw_input;
        Input& game_input;
        Time& time;
        AssetManager& assets;
        Logger& log;
        SystemScheduler& scheduler;
        DevTools& dev_tools;
        WorldBridge& world_bridge;
        RenderBridge& render_bridge;
        AtmosphereController& atmosphere;
        VfxRuntime& vfx;
        PhysicsBridge& physics_bridge;
        TerrainBridge& terrain_bridge;
        AudioBridge& audio_bridge;
        ScriptBridge& script_bridge;
        float fixed_timestep = 0.0f;
    };

    explicit FrameLoop(const Services& services);
    ~FrameLoop() = default;
    FrameLoop(const FrameLoop&) = delete;
    FrameLoop& operator=(const FrameLoop&) = delete;

    bool start(Game& game);
    [[nodiscard]] float next_delta();
    void run_frame(Game& game, float delta);
    void finish_world_loading();

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
    SystemScheduler& scheduler_;
    DevTools& dev_tools_;
    WorldBridge& world_bridge_;
    RenderBridge& render_bridge_;
    AtmosphereController& atmosphere_;
    VfxRuntime& vfx_;
    PhysicsBridge& physics_bridge_;
    TerrainBridge& terrain_bridge_;
    AudioBridge& audio_bridge_;
    ScriptBridge& script_bridge_;
    float fixed_timestep_;
    Clock clock_;
    std::vector<Vec3> streaming_sources_;
    bool started_ = false;
};

} // namespace astral
