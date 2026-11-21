# Standalone project: verifies an installed Astral is usable via find_package.
cmake_minimum_required(VERSION 3.24)
project(AstralConsumer LANGUAGES CXX)

find_package(Astral 0.1 CONFIG REQUIRED COMPONENTS Core)

add_executable(astral_consumer main.cpp)
target_link_libraries(astral_consumer PRIVATE Astral::Core)
