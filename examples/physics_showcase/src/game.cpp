#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/lab/lab.hpp"
#include "features/lifetime/lifetime.hpp"
#include "features/player/player.hpp"
#include "features/playground/playground.hpp"
#include "features/rain/rain.hpp"
#include "features/shooter/shooter.hpp"
#include "features/showcase/showcase.hpp"
#include "stations/stations.hpp"

#include <cstdio>
#include <string>

using namespace astral;

namespace physics_showcase {

namespace {

/// The station the viewer is in: the nearest one within its radius (on the ground plane), or none.
const showcase::Station* current_station(World& w, Vec3 at) {
    const showcase::Station* best = nullptr;
    float best_d = 1e30f;
    w.each<const showcase::Station, const Transform>([&](const showcase::Station& s, const Transform& t) {
        const float dx = t.position.x - at.x, dz = t.position.z - at.z;
        const float d = dx * dx + dz * dz;
        if (d < s.radius * s.radius && d < best_d) {
            best_d = d;
            best = &s;
        }
    });
    return best;
}

std::string stats_text(Context& ctx) {
    int broken = 0, hits = 0;
    ctx.world.each<const lab::LabStats>([&](const lab::LabStats& s) {
        broken += s.broken_joints;
        hits = s.batch_ray_hits;
    });
    std::uint32_t contacts = 0, launches = 0;
    ctx.world.each<const playground::ContactStats>([&](const playground::ContactStats& s) {
        contacts += s.begin_count;
        launches += s.pad_launches;
    });
    float wheel = 0.0f;
    ctx.world.each<const showcase::Spinner>([&](const showcase::Spinner& s) { wheel = s.speed; });
    char line[256];
    const std::size_t bodies = ctx.physics ? ctx.physics->body_count() : 0;
    std::snprintf(line, sizeof line, "gövde %zu   temas %u   ped %u   kırılan bağ %d   sensör ışını %d/4   çark %.1f rad/s",
                  bodies, contacts, launches, broken, hits, static_cast<double>(wheel));
    return line;
}

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("lifetime", lifetime::install_lifetime);
    game.add("player", player::install_player);
    game.add("shooter", shooter::install_shooter);
    game.add("rain", rain::install_rain);
    game.add("lab", lab::install_lab);
    game.add("playground", playground::install_playground);
    game.add("showcase", showcase::install_showcase);
    return game.install(ctx);
}

void ShowcaseGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    build_terrain_and_lake(ctx);
    build_water_station(ctx);
    build_pile_station(ctx);
    build_joints_station(ctx, handles);
    build_surfaces_station(ctx, handles);
    build_player(ctx, handles);

    ctx.sun = {.direction = normalize(Vec3{-0.55f, -0.62f, -0.45f}), .color = {1.0f, 0.95f, 0.86f}, .intensity = 2.2f};
    ctx.environment = {.sky = {0.42f, 0.58f, 0.85f}, .ground = {0.12f, 0.14f, 0.09f}, .ambient = 0.3f, .exposure = 0.55f,
                       .aerial_extinction = 0.004f, .aerial_start_m = 40.0f};
    ctx.debug_show.physics = overlays_;
    ctx.debug_show.physics_joints = overlays_;
    ctx.debug_show.fields = overlays_;
    ctx.log.info("physics showcase: WASD walk, Space jump, arrows turn, left click shoot, right click push, F fly, F1 debug, Esc quit");
}

void ShowcaseGame::on_update(Context& ctx) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f;

    Vec3 at{};
    if (const Transform* t = ctx.world.get<const Transform>(handles.player)) at = t->position;
    if (const player::PlayerCamera* cam = ctx.world.get<const player::PlayerCamera>(handles.camera); cam && cam->flying) {
        if (const Transform* t = ctx.world.get<const Transform>(handles.camera)) at = t->position;
    }
    if (const showcase::Station* s = current_station(ctx.world, at)) {
        ctx.hud.text(font, s->title, {24 * unit, 18 * unit}, {.size = 40 * unit, .color = {1, 0.85f, 0.45f, 1}, .shadow = true});
        ctx.hud.text(font, s->hint, {24 * unit, 64 * unit}, {.size = 18 * unit, .color = {1, 1, 1, 0.9f}, .shadow = true});
    }
    ctx.hud.text(font, stats_text(ctx), {screen.x - 24 * unit, 18 * unit},
                 {.size = 16 * unit, .color = {0.75f, 0.9f, 1, 0.9f}, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font, "WASD yürü · Space zıpla · oklar dön · sol tık at · sağ tık it · F uçan kamera · F1 debug · Esc çıkış",
                 {screen.x * 0.5f, screen.y - 34 * unit}, {.size = 16 * unit, .color = {1, 1, 1, 0.7f}, .align = TextAlign::Center});
}

} // namespace physics_showcase
