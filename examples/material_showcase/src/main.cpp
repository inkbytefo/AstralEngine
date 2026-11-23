// material_showcase — M13 Advanced PBR & Path Tracer studio gallery (ADR-0033).
//   material_showcase                                  window (press P to toggle Hybrid RT / Path Tracer)
//   material_showcase --capture out.ppm [--frames 64] [--path-tracer] [--raster]
#include "gallery.hpp"
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    astral::RenderSettings render = material_showcase::showcase_settings(options.has("--path-tracer"));
    if (options.has("--raster")) {
        render.ray_tracing = false;
        render.path_tracer = false;
    }
    material_showcase::ShowcaseGame game;
    return astral::run_game(options, {.title = "Astral — M13 Material Showcase", .render = render}, game, 64);
}
