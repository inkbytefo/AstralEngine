add_library(AstralEngine STATIC
    src/Assets/AssetManager.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/SdfMeshing.cpp
    src/Platform/Input.cpp
    src/Platform/Window.cpp
    src/Runtime/Engine.cpp
    src/Runtime/RenderSync.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneSerializer.cpp)
add_library(Astral::Engine ALIAS AstralEngine)

target_include_directories(AstralEngine PUBLIC include PRIVATE src)
target_link_libraries(AstralEngine
    PUBLIC Astral::Render nlohmann_json::nlohmann_json
    PRIVATE glfw Jolt astral_cgltf astral_stb)
astral_configure_target(AstralEngine)
# Single-header libraries compiled here: keep their warnings out of the build log.
set_source_files_properties(src/Assets/AssetManager.cpp PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-missing-field-initializers;-Wno-unused-parameter;-Wno-sign-compare>")
