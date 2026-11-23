astral_add_module(NAME editor_core
    SOURCES
        src/document.cpp
        src/command.cpp
        src/transaction.cpp
        src/agent.cpp
        src/agent_task.cpp
        src/world_authoring.cpp
    DEPS
        Astral::Core
        Astral::World
        Astral::Assets)

astral_add_tests(MODULE editor_core
    SOURCES
        ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
        tests/editor_core_test.cpp
        tests/world_authoring_test.cpp)
