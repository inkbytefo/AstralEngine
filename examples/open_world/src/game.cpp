#include "game.hpp"

#include "features/day_night/day_night.hpp"
#include "features/fly_camera/fly_camera.hpp"

using namespace astral;

namespace open_world {

namespace {

/// Captures are image tests: every cell in range is in the picture, whatever the timing (after the camera moved).
class FinishLoading final : public System {
public:
    void update(Context& ctx) override { ctx.maps().finish_world_loading(); }
};

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("day_night", day_night::install_day_night);
    return game.install(ctx);
}

void OpenWorldGame::on_start(Context& ctx) {
    if (!install_features(ctx) || !ctx.maps().open_world("maps/test.world")) {
        ctx.quit();
        return;
    }
    ctx.sun.direction = normalize(Vec3{-0.4f, -0.55f, -0.35f});
    day_night::apply(ctx, night_);
    ctx.world.create(day_night::DayNight{.night = night_, .applied = night_ ? 1 : 0});
    // Start near one corner, 40 m up, looking into the world; the first frames already cross the
    // 2 km origin threshold (the start is 2.4 km from zero).
    ctx.world.create(Transform{.position = {1700, 40, 1700 + nudge_}}, Camera{.far_plane = 2500.0f}, StreamingSource{},
                     fly::FlyCamera{.speed = 80.0f, .fast_speed = 400.0f, .yaw = radians(-135.0f), .pitch = radians(-12.0f),
                                    .autopilot = capturing_ ? 120.0f : 0.0f});
    if (capturing_) ctx.add_system<FinishLoading>(Phase::PostUpdate);
    ctx.maps().finish_world_loading(); // a loading screen would show here
}

} // namespace open_world
