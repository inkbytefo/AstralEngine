#include "frame_loop.hpp"

#include "atmosphere.hpp"
#include "audio_bridge.hpp"
#include "dev_tools.hpp"
#include "physics_bridge.hpp"
#include "render_bridge.hpp"
#include "script_bridge.hpp"
#include "system_scheduler.hpp"
#include "terrain_bridge.hpp"
#include "vfx_runtime.hpp"
#include "world_bridge.hpp"

#include <astral/app/app.hpp>
#include <astral/app/gameplay_smoke.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

namespace astral {

FrameLoop::FrameLoop(const Services& s)
    : world_(s.world),
      context_(s.context),
      raw_input_(s.raw_input),
      game_input_(s.game_input),
      time_(s.time),
      assets_(s.assets),
      log_(s.log),
      scheduler_(s.scheduler),
      dev_tools_(s.dev_tools),
      world_bridge_(s.world_bridge),
      render_bridge_(s.render_bridge),
      atmosphere_(s.atmosphere),
      vfx_(s.vfx),
      physics_bridge_(s.physics_bridge),
      terrain_bridge_(s.terrain_bridge),
      audio_bridge_(s.audio_bridge),
      script_bridge_(s.script_bridge),
      fixed_timestep_(s.fixed_timestep) {}

bool FrameLoop::start(Game& game) {
    if (started_) {
        log_.error("App: run()/run_frames() may only be called once per App");
        return false;
    }
    started_ = true;
    game.on_start(context_);
    world_.apply_commands();
    clock_.tick();
    return true;
}

float FrameLoop::next_delta() { return fixed_timestep_ > 0.0f ? fixed_timestep_ : clock_.tick(); }

void FrameLoop::finish_world_loading() {
    update_world(true);
    world_bridge_.collect_streaming_sources(streaming_sources_);
    terrain_bridge_.sync(streaming_sources_, true);
}

void FrameLoop::run_frame(Game& game, float delta) {
    dev_tools_.begin_frame();
    route_input();
    world_.update_events(); // events sent last frame are still readable this frame, then dropped
    {
        ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "world streaming");
        update_world(false);
    }
    update_assets_and_scenes();
    time_.delta = delta;
    run_scripted_phase(Phase::PreUpdate, ScriptPhase::PreUpdate, "PreUpdate");
    run_game_update(game);
    run_scripted_phase(Phase::Update, ScriptPhase::Update, "Update");
    if (terrain_bridge_.streaming_enabled()) {
        world_bridge_.collect_streaming_sources(streaming_sources_);
        terrain_bridge_.sync(streaming_sources_);
    } else {
        terrain_bridge_.sync({});
    }
    run_physics(delta);
    run_scripted_phase(Phase::PostUpdate, ScriptPhase::PostUpdate, "PostUpdate");
    atmosphere_.update(context_, render_bridge_, delta);
    update_transforms_and_primitives();
    update_audio();
    render_frame(game, delta);
    advance_frame_state(delta);
    dev_tools_.end_frame();
}

void FrameLoop::route_input() {
    const InputCapture capture = dev_tools_.route_debug_input(raw_input_, context_.show_debug_ui);
    script_bridge_.route_input(raw_input_, game_input_, capture.keyboard, capture.mouse);
}

void FrameLoop::update_world(bool finish) {
    if (!world_bridge_.is_open() && !terrain_bridge_.streaming_enabled()) return;
    world_bridge_.collect_streaming_sources(streaming_sources_);
    if (const auto off = world_bridge_.begin_origin_shift(streaming_sources_)) {
        physics_bridge_.shift_origin(*off);
        render_bridge_.shift_origin(*off);
        world_bridge_.complete_origin_shift(*off, streaming_sources_);
    }
    world_bridge_.stream(streaming_sources_, finish);
}

void FrameLoop::update_assets_and_scenes() {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "assets");
    assets_.update(); // finished loads, hot reloads
    render_bridge_.update_materials(time_.elapsed); // edited .material / .ashader files
    script_bridge_.reload_changed();
    world_bridge_.sync_prefabs();
    render_bridge_.sync_models(); // ModelRef entities get (or refresh) their pieces before any system runs
}

void FrameLoop::run_scripted_phase(Phase phase, ScriptPhase script_phase, const char* scope_name) {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), scope_name);
    scheduler_.run(phase, context_);
    script_bridge_.run(script_phase);
    world_.apply_commands();
}

void FrameLoop::run_game_update(Game& game) {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "Game::on_update");
    game.on_update(context_);
    world_.apply_commands();
}

void FrameLoop::run_fixed_update_cb(void* user) { static_cast<FrameLoop*>(user)->run_fixed_update(); }

void FrameLoop::run_fixed_update() {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "FixedUpdate");
    scheduler_.run(Phase::FixedUpdate, context_);
    script_bridge_.run(ScriptPhase::FixedUpdate);
    vfx_.update(context_, time_.fixed_delta);
    world_.each<GameplaySmoke, const Transform>([&](Entity entity, GameplaySmoke& smoke, const Transform& transform) {
        if (!world_.has<Parent>(entity)) smoke.step(physics_bridge_, transform.position);
    });
    vfx_.apply_smoke_events(world_);
}

void FrameLoop::run_physics(float delta) {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "physics");
    physics_bridge_.step(delta, &FrameLoop::run_fixed_update_cb, this, &terrain_bridge_);
    world_.apply_commands();
}

void FrameLoop::update_transforms_and_primitives() {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "transforms");
    update_transforms(world_, &context_.jobs);
    terrain_bridge_.update_lod();
    render_bridge_.sync_primitives();
}

void FrameLoop::update_audio() {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "audio");
    audio_bridge_.sync();
}

void FrameLoop::render_frame(Game& game, float delta) {
    ASTRAL_PROFILE_SCOPE(&dev_tools_.profiler(), "render");
    const auto world_cells = context_.debug_show.world_cells ? world_bridge_.debug_cells() : std::nullopt;
    dev_tools_.draw_overlays(context_.debug_show, context_.debug, world_cells ? &*world_cells : nullptr);
    dev_tools_.build_ui(game, context_, raw_input_, delta, render_bridge_.extent(), render_bridge_.material_library());
    const WorldPosition origin = world_bridge_.origin();
    render_bridge_.render_frame({
        .sun = context_.sun,
        .environment = context_.environment,
        .debug_lines = context_.debug.lines(),
        .ui_layers = dev_tools_.overlay_layers(context_.hud),
        .time = static_cast<float>(time_.elapsed),
        .sky = context_.sky,
        .post = context_.post,
        .world_origin = {origin.x, origin.y, origin.z},
    });
}

void FrameLoop::advance_frame_state(float delta) {
    context_.debug.advance(delta); // this frame's shapes go; timed ones count down
    context_.hud.clear();
    time_.elapsed += delta;
    ++time_.frame;
}

} // namespace astral
