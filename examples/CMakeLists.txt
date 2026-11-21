add_executable(hello_core hello_core/main.cpp)
target_link_libraries(hello_core PRIVATE Astral::Core)
astral_set_warnings(hello_core)

add_executable(hello_triangle hello_triangle/main.cpp)
target_link_libraries(hello_triangle PRIVATE Astral::Platform Astral::Gpu)
astral_set_warnings(hello_triangle)
astral_add_shaders(hello_triangle SOURCES hello_triangle/triangle.vert hello_triangle/triangle.frag)

add_executable(spinning_cubes spinning_cubes/main.cpp)
target_link_libraries(spinning_cubes PRIVATE Astral::App)
astral_set_warnings(spinning_cubes)

add_executable(sdf_scene sdf_scene/main.cpp)
target_link_libraries(sdf_scene PRIVATE Astral::App)
astral_set_warnings(sdf_scene)

add_executable(physics_demo physics_demo/main.cpp)
target_link_libraries(physics_demo PRIVATE Astral::App)
astral_set_warnings(physics_demo)
