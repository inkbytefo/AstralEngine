add_executable(EngineTests
    src/main.cpp
    src/EcsTests.cpp
    src/PhysicsTests.cpp
    src/IdentityTests.cpp
    src/SceneTests.cpp
    src/SerializationTests.cpp
    src/BrickGridTests.cpp
    src/CommandStackTests.cpp
    src/EventBusTests.cpp
    src/ActionMapTests.cpp
    src/VmaTests.cpp
    src/JobSystemTests.cpp
    src/TaskGraphTests.cpp
    src/ProjectTests.cpp
    src/GpuSmokeTest.cpp
    src/CameraTests.cpp
    src/ApplicationLoopTests.cpp
    src/ServiceBoundariesTests.cpp
    src/EditorGameplayCycleTests.cpp
    ${CMAKE_SOURCE_DIR}/Projects/Sandbox/src/PuzzleGameSubsystem.cpp
)

target_include_directories(EngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Projects/Sandbox/src
)

target_link_libraries(EngineTests PRIVATE
    AstralEngine
)

add_executable(CameraGpuTests src/CameraGpuTests.cpp)
target_include_directories(CameraGpuTests PRIVATE "${CMAKE_SOURCE_DIR}/Projects/Sandbox/src")
target_link_libraries(CameraGpuTests PRIVATE AstralEngine)
set_target_properties(CameraGpuTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set_target_properties(EngineTests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ==============================================================================
# CTest Entegrasyonu ve Test Kaydi
# ==============================================================================
include(CTest)

if(BUILD_TESTING)
    # 1. Tum Headless CPU Regresyon Test Paketi (CI dostu)
    add_test(NAME EngineTests.AllCpu COMMAND EngineTests)
    set_tests_properties(EngineTests.AllCpu PROPERTIES
        LABELS "CPU"
        TIMEOUT 60
    )

    # 2. Moduler Izole CPU Testleri
    macro(register_cpu_subtest name flag timeout_sec)
        add_test(NAME EngineTests.${name} COMMAND EngineTests ${flag})
        set_tests_properties(EngineTests.${name} PROPERTIES
            LABELS "CPU"
            TIMEOUT ${timeout_sec}
        )
    endmacro()

    register_cpu_subtest(ECS --ecs 15)
    register_cpu_subtest(Physics --physics 15)
    register_cpu_subtest(Identity --identity 15)
    register_cpu_subtest(Scene --scene 15)
    register_cpu_subtest(Serialization --serialization 30)
    register_cpu_subtest(BrickGrid --brickgrid 15)
    register_cpu_subtest(CommandStack --command 15)
    register_cpu_subtest(EventBus --eventbus 15)
    register_cpu_subtest(ActionMap --actionmap 15)
    register_cpu_subtest(JobSystem --jobs 30)
    register_cpu_subtest(TaskGraph --taskgraph 30)
    register_cpu_subtest(Project --project 15)
    register_cpu_subtest(ApplicationLoop --loop 20)
    register_cpu_subtest(ServiceBoundaries --boundaries 20)
    register_cpu_subtest(EditorGameplayCycle --gameplay 20)

    # 3. GPU Testleri (Ayrik GPU Etiketi - Donanim Kosucusu Gerektirir)
    add_test(NAME EngineTests.GPU.Smoke COMMAND EngineTests --gpu --gpu-frames 5)
    set_tests_properties(EngineTests.GPU.Smoke PROPERTIES
        LABELS "GPU"
        TIMEOUT 60
    )

    add_test(NAME EngineTests.GPU.Camera COMMAND CameraGpuTests "${CMAKE_CURRENT_BINARY_DIR}/camera_gpu_output" "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/a21/after")
    set_tests_properties(EngineTests.GPU.Camera PROPERTIES
        LABELS "GPU"
        TIMEOUT 60
    )
endif()
