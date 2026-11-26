# Examples: every one is a game built like templates/blank (src/ + assets/ + tests/), see AGENTS.md.
#   hello_headless   a game without window or GPU: features + Luau + a deterministic loop
#   hello_triangle   the smallest picture: one mesh, one .ashader material
#   spinning_cubes   C++ parallel system + Luau rules + free camera
#   sdf_scene        signed-distance-field shapes, orbiting lights
#   physics_demo     rigid bodies, raycasts, a character controller (features: player, shooter, rain, lifetime)
#   physics_playground  M12: height-field terrain + lake, buoyancy, wind, plank bridge on hinges, jump pad (trigger), glider
#   physics_lab      P0: motorized SixDOF/SwingTwist/Hinge joints, breakable joints, materials, cooked shapes, batched queries
#   physics_showcase the physics in one world: pile, joints, surfaces, water and wind (replaces the three above)
#   lighting_showcase ray-traced room, SDF yard, many-lights night (replaces rt_showcase and sdf_scene)
#   materials_showcase PBR models, .material workflow and .ashader night gallery (replaces material_showcase, material_workflow, shader_showcase)
#   rt_showcase      the ray-traced "RGB room"
#   material_showcase M13 advanced PBR studio: clearcoat, sheen, anisotropy, glass, SSS, wetness, decals & path tracer
#   material_workflow material hot-reload & UV inspection scene workflow
#   model_viewer     a glTF model in a studio, with hot reload
#   scene_viewer     scenes and prefabs, reloaded live
#   asset_viewer     glTF/GLB models and scenes with orbit/fly camera, stats HUD and cook/pack (replaces the two above)
#   shader_showcase  .ashader / .material: leaf, glass, lava, hologram, sky, post
#   pong             3D Pong: three feature packages, rules in Luau
#   open_world       a 4 x 4 km streamed map, floating origin, day and night
#   world_editor     standalone AI-first native desktop editor (E0/E1/E2)
#   terrain_lab      chunked terrain LOD & collision streaming lab
#   world_showcase   4 x 4 km streamed world, floating origin, terrain layers & LOD, day/night (replaces open_world and terrain_lab)
#   vfx_lab          volumetric atmosphere, smoke, and VFX timeline lab
#   vfx_showcase     atmosphere, froxel fog, gameplay smoke, GPU particles, and Luau-driven VFX/audio (replaces vfx_lab)
add_subdirectory(hello_headless)
add_subdirectory(hello_triangle)
add_subdirectory(spinning_cubes)
add_subdirectory(sdf_scene)
add_subdirectory(physics_demo)
add_subdirectory(physics_playground)
add_subdirectory(physics_lab)
add_subdirectory(physics_showcase)
add_subdirectory(lighting_showcase)
add_subdirectory(materials_showcase)
add_subdirectory(rt_showcase)
add_subdirectory(material_showcase)
add_subdirectory(material_workflow)
add_subdirectory(model_viewer)
add_subdirectory(scene_viewer)
add_subdirectory(asset_viewer)
add_subdirectory(shader_showcase)
add_subdirectory(pong)
add_subdirectory(open_world)
add_subdirectory(world_editor)
add_subdirectory(terrain_lab)
add_subdirectory(world_showcase)
add_subdirectory(vfx_lab)
add_subdirectory(vfx_showcase)
