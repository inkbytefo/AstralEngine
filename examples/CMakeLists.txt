add_executable(hello_core hello_core/main.cpp)
target_link_libraries(hello_core PRIVATE Astral::Core)
astral_set_warnings(hello_core)
