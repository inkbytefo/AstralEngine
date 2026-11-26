#include "game.hpp"

#include "features/pong_rules/rules.hpp"

#include <algorithm>
#include <string>

using namespace astral;

namespace pong {

bool install(Context& ctx) {
    FeatureSet game;
    game.add("pong_rules", install_rules);
    game.add("pong_scorelights", install_scorelights);
    game.add("pong_audio", install_audio);
    return game.install(ctx);
}

void set_player_ai(World& world, bool on) {
    world.each<Paddle>([&](Paddle& p) {
        if (p.side == Side::Player) p.ai = on;
    });
}

const Match* match(World& world) {
    const Match* found = nullptr;
    world.each<const Match>([&](const Match& m) {
        if (!found) found = &m;
    });
    return found;
}

void PongGame::on_start(Context& ctx) {
    if (!install(ctx)) {
        ctx.quit();
        return;
    }
    ctx.scenes().load_scene("court.scene");
    if (demo_) set_player_ai(ctx.world, true);
    if (ctx.window && !demo_) ctx.window->set_mouse_captured(true);
}

void PongGame::on_update(Context& ctx) {
    notices_.read(ctx.world, [&](const SaveNotice& n) {
        notice_kind_ = n.kind;
        notice_timer_ = 2.0f;
    });
    if (notice_timer_ > 0.0f) notice_timer_ = std::max(0.0f, notice_timer_ - ctx.time.delta);
    const Match* m = ctx.singleton<Match>();
    if (!m) return;
    draw_hud(ctx, *m);
    if (!ctx.window) return;
    const int key = m->player * 1000 + m->opponent + (m->game_over_timer > 0 ? 100000 : 0);
    if (key == shown_) return;
    shown_ = key;
    std::string title = "Astral Pong — " + std::to_string(m->player) + " : " + std::to_string(m->opponent);
    if (m->game_over_timer > 0) title += m->player > m->opponent ? "  — you win!" : "  — the AI wins";
    ctx.window->set_title(title);
}

/// Score at the top, the result when a match ends, controls at the bottom.
void PongGame::draw_hud(Context& ctx, const Match& m) const {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f; // the HUD scales with the window
    const Vec4 blue{0.45f, 0.72f, 1.0f, 1}, red{1.0f, 0.5f, 0.38f, 1}, white{1, 1, 1, 1};
    const float y = 18 * unit, size = 54 * unit;
    ctx.hud.text(font, std::to_string(m.player), {screen.x * 0.5f - 30 * unit, y}, {.size = size, .color = blue, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font, ":", {screen.x * 0.5f, y}, {.size = size, .color = white, .align = TextAlign::Center, .shadow = true});
    ctx.hud.text(font, std::to_string(m.opponent), {screen.x * 0.5f + 30 * unit, y}, {.size = size, .color = red, .shadow = true});
    if (m.game_over_timer > 0) {
        const bool won = m.player > m.opponent;
        const float h = 120 * unit;
        ctx.hud.rect({0, screen.y * 0.5f - h * 0.5f}, {screen.x, screen.y * 0.5f + h * 0.5f}, {0, 0, 0, 0.55f});
        ctx.hud.text(font, won ? "KAZANDIN!" : "YAPAY ZEKÂ KAZANDI", {screen.x * 0.5f, screen.y * 0.5f - 34 * unit},
                     {.size = 64 * unit, .color = won ? blue : red, .align = TextAlign::Center, .shadow = true});
    }
    if (!demo_) {
        if (notice_timer_ > 0.0f) {
            const char* msg = notice_kind_ == SaveNotice::Kind::Saved
                                  ? "Maç kaydedildi"
                                  : (notice_kind_ == SaveNotice::Kind::Loaded ? "Maç yüklendi" : "Kayıtlı maç yok");
            ctx.hud.text(font, msg, {screen.x * 0.5f, screen.y - 66 * unit},
                         {.size = 20 * unit, .color = {0.85f, 0.95f, 1.0f, 0.9f}, .align = TextAlign::Center, .shadow = true});
        }
        ctx.hud.text(font,
                     "WASD / oklar / fare / gamepad: raket   •   F5 / LB: kaydet   •   F9 / RB: yükle   •   F1: debug   •   Esc: çıkış",
                     {screen.x * 0.5f, screen.y - 34 * unit},
                     {.size = 18 * unit, .color = {1, 1, 1, 0.7f}, .align = TextAlign::Center});
    }
}

} // namespace pong
