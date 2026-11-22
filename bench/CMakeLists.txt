add_executable(astral_bench
    main.cpp)
set_target_properties(astral_bench PROPERTIES OUTPUT_NAME astral-bench)
target_link_libraries(astral_bench PRIVATE
    Astral::Core
    Astral::Shader
    Astral::World
    Astral::Script
    Astral::Render
    Astral::Assets
    Astral::App)
astral_set_warnings(astral_bench)

if(ASTRAL_BUILD_TESTS)
    add_test(NAME astral_benchmarks_smoke COMMAND $<TARGET_FILE:astral_bench> --smoke)
endif()
