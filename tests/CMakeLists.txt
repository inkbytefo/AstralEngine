# Unit tests: CPU only, no window, no GPU.
add_executable(AstralUnitTests
    unit/Main.cpp
    unit/CoreTests.cpp
    unit/EcsTests.cpp
    unit/SceneTests.cpp
    unit/SdfTests.cpp
    unit/PhysicsTests.cpp)
target_link_libraries(AstralUnitTests PRIVATE Astral::Engine astral_doctest)
astral_configure_target(AstralUnitTests)
add_test(NAME unit COMMAND AstralUnitTests)

# GPU tests: offscreen rendering. Need a Vulkan 1.3 device (lavapipe works in CI).
add_executable(AstralGpuTests gpu/Main.cpp gpu/RenderTests.cpp gpu/EngineTests.cpp)
target_link_libraries(AstralGpuTests PRIVATE Astral::Engine astral_doctest)
astral_configure_target(AstralGpuTests)
add_test(NAME gpu COMMAND AstralGpuTests)
set_tests_properties(gpu PROPERTIES ENVIRONMENT "ASTRAL_VALIDATION=1" LABELS gpu)
