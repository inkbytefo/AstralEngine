add_executable(Sandbox main.cpp SandboxSystems.hpp)
target_link_libraries(Sandbox PRIVATE Astral::Engine)
astral_configure_target(Sandbox)
