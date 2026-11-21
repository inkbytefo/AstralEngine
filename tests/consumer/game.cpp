// A game built with astral_add_game from an installed Astral: loads its scene from loose files in
// development and from consumer_game.apak once staged by consumer_game_package.
#include <astral/astral.hpp>

#include <cstdio>

int main() {
    struct Hello final : astral::Game {
        std::size_t loaded = 0;
        bool packed = false;
        void on_start(astral::Context& ctx) override {
            packed = ctx.assets->from_pack();
            loaded = ctx.load_scene("hello.scene").entities.size();
        }
    } game;
    astral::App app({.headless = true, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK});
    app.run_frames(game, 1);
    std::printf("consumer_game: %zu entities from %s\n", game.loaded, game.packed ? "the pack" : "loose files");
    return game.loaded == 1 ? 0 : 1;
}
