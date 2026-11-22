// Shader showcase (M10, ADR-0022): every surface here is a .material file on a .ashader shader.
//
//   shader_showcase                      fly around: WASD, Q/E, Shift fast, right mouse look, F1 -> Materials
//   shader_showcase --capture out.ppm [--frames 60] [--width 1280] [--height 720] [--raster]
//
// Left to right: a leaf plant swaying in the wind (alpha test + vertex()), two glass spheres (blend_mix, one a child
// material of the other), a lava ball (animated noise, lights its surroundings), a hologram (blend_add). The sky is a
// sky shader, the vignette a post shader. Edit any file under assets/ while it runs and the picture changes.
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    astral::RenderSettings render;
    render.bloom_strength = 0.06f;
    render.ray_tracing = !options.has("--raster"); // no ray tracing: the raster-only look
    shader_showcase::ShowcaseGame game;
    return astral::run_game(options, {.title = "Astral Shader Showcase", .render = render, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 60);
}
