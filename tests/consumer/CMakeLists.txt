# Standalone project: verifies an installed Astral is usable via find_package.
cmake_minimum_required(VERSION 3.24)
project(AstralConsumer LANGUAGES CXX)

find_package(Astral 0.17 CONFIG REQUIRED COMPONENTS Core Gpu Platform Physics World Render Assets App)

add_executable(astral_consumer main.cpp)
target_link_libraries(astral_consumer PRIVATE Astral::App)

astral_add_shaders(astral_consumer SOURCES fill.comp)

# A game the way games are built: exe + assets; consumer_game_package cooks and packs with the
# installed astral-cook.
astral_add_game(consumer_game SOURCES game.cpp ASSETS assets)

# E0 isolation check (ADR-0035): game and server targets must never link editor modules.
get_target_property(_app_iface Astral::App INTERFACE_LINK_LIBRARIES)
foreach(_dep IN LISTS _app_iface)
    if(_dep MATCHES "Editor" OR _dep MATCHES "editor")
        message(FATAL_ERROR "Astral::App illegally pulls in editor dependency: ${_dep}")
    endif()
endforeach()
get_target_property(_game_links consumer_game LINK_LIBRARIES)
foreach(_dep IN LISTS _game_links)
    if(_dep MATCHES "Editor" OR _dep MATCHES "editor")
        message(FATAL_ERROR "consumer_game illegally links editor dependency: ${_dep}")
    endif()
endforeach()
