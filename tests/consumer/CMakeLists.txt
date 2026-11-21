# Standalone project: verifies an installed Astral is usable via find_package.
cmake_minimum_required(VERSION 3.24)
project(AstralConsumer LANGUAGES CXX)

find_package(Astral 0.4 CONFIG REQUIRED COMPONENTS Core Gpu Platform)

add_executable(astral_consumer main.cpp)
target_link_libraries(astral_consumer PRIVATE Astral::Core Astral::Gpu Astral::Platform)

astral_add_shaders(astral_consumer SOURCES fill.comp)
