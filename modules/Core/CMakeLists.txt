add_library(AstralCore STATIC
    src/Assert.cpp
    src/CommandStack.cpp
    src/JobSystem.cpp
    src/Log.cpp
    src/TaskGraph.cpp
    src/UUID.cpp)
add_library(Astral::Core ALIAS AstralCore)

target_include_directories(AstralCore PUBLIC include)
target_link_libraries(AstralCore PUBLIC glm::glm)
find_package(Threads REQUIRED)
target_link_libraries(AstralCore PUBLIC Threads::Threads)
astral_configure_target(AstralCore)
