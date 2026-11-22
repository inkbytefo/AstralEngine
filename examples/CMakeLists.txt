# Examples: every one is a game built like templates/blank (src/ + assets/ + tests/), see AGENTS.md.
#   hello_headless   a game without window or GPU: features + Luau + a deterministic loop
#   hello_triangle   the smallest picture: one mesh, one .ashader material
#   spinning_cubes   C++ parallel system + Luau rules + free camera
#   sdf_scene        signed-distance-field shapes, orbiting lights
#   physics_demo     rigid bodies, raycasts, a character controller (features: player, shooter, rain, lifetime)
#   rt_showcase      the ray-traced "RGB room"
#   model_viewer     a glTF model in a studio, with hot reload
#   scene_viewer     scenes and prefabs, reloaded live
#   shader_showcase  .ashader / .material: leaf, glass, lava, hologram, sky, post
#   pong             3D Pong: three feature packages, rules in Luau
#   open_world       a 4 x 4 km streamed map, floating origin, day and night
add_subdirectory(hello_headless)
add_subdirectory(hello_triangle)
add_subdirectory(spinning_cubes)
add_subdirectory(sdf_scene)
add_subdirectory(physics_demo)
add_subdirectory(rt_showcase)
add_subdirectory(model_viewer)
add_subdirectory(scene_viewer)
add_subdirectory(shader_showcase)
add_subdirectory(pong)
add_subdirectory(open_world)
