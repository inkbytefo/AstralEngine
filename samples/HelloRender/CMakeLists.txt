add_executable(HelloRender main.cpp)
target_link_libraries(HelloRender PRIVATE Astral::Render glfw)
astral_configure_target(HelloRender)
