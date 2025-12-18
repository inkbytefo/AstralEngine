add_subdirectory(Core)
add_subdirectory(ECS)
add_subdirectory(Events)
add_subdirectory(ThirdParty)
add_subdirectory(Subsystems)

target_sources(AstralEngine PRIVATE
    main.cpp
    test_sdl3.cpp
)
