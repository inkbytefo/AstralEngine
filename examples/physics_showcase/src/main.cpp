// physics_showcase: the physics of the engine in one world. Four stations around a lake: a crate pile with rain, joints
// (motors, SwingTwist, SixDOF, a breaking joint, a scene-authored part), surfaces (material table, cooked shapes, a
// torque-driven wheel, a scanner beam), and water and wind (buoyancy, a hinged bridge, a jump pad, a glider).
//   physics_showcase                       WASD walk, Space jump, arrows turn, left click shoot, right click push, F fly
//   physics_showcase --capture out.ppm [--frames N] [--width W] [--height H] [--overlays] [--raster]
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    physics_showcase::ShowcaseGame game(options.has("--overlays"));
    astral::RenderSettings render;
    render.bloom_strength = 0.05f;
    render.ray_tracing = !options.has("--raster");
    return astral::run_game(options, {.title = "Astral — physics showcase", .render = render, .asset_root = ASTRAL_GAME_ASSETS,
                                      .pack = ASTRAL_GAME_PACK, .physics_interpolation = true},
                            game, 240);
}
