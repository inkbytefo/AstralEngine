# Examples: every one is a game built like templates/blank (src/ + assets/ + tests/), see AGENTS.md.
# Small starting points:
#   hello_headless     a game without window or GPU: features + Luau + a deterministic loop (and scripted input)
#   hello_triangle     the smallest picture: one mesh, one .ashader material
#   spinning_cubes     C++ parallel system + Luau rules + free camera
#   pong               3D Pong: feature packages, rules in Luau, save slots, gamepad
# Showcases (one area of the engine each):
#   physics_showcase   pile, joints, surfaces, water and wind
#   lighting_showcase  ray-traced room, SDF yard, many-lights night
#   materials_showcase PBR shading models, .material workflow, .ashader night gallery
#   world_showcase     4 x 4 km streamed world, floating origin, terrain layers and LOD, day/night
#   vfx_showcase       atmosphere, froxel fog, gameplay smoke, GPU particles, VFX and audio from Luau
#   asset_viewer       glTF/GLB models and scenes, orbit/fly camera, stats HUD, cook/package
# Tools:
#   world_editor       standalone AI-first native desktop editor (E0/E1/E2)
add_subdirectory(hello_headless)
add_subdirectory(hello_triangle)
add_subdirectory(spinning_cubes)
add_subdirectory(pong)
add_subdirectory(physics_showcase)
add_subdirectory(lighting_showcase)
add_subdirectory(materials_showcase)
add_subdirectory(world_showcase)
add_subdirectory(vfx_showcase)
add_subdirectory(asset_viewer)
add_subdirectory(world_editor)
