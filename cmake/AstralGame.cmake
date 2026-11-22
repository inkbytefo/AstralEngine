# astral_add_game(<name> SOURCES <files...> ASSETS <folder>)
#
#   <name>            the game executable, linked to Astral::App. It gets two definitions:
#                       ASTRAL_GAME_ASSETS  absolute path of the source asset folder (development)
#                       ASTRAL_GAME_PACK    "<name>.apak" (shipped: next to the executable)
#                     so `App({.asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK})` reads
#                     loose files with hot reload while developing and the pack once shipped.
#   <name>_package    cooks the assets (incrementally) and stages a runnable folder:
#                       <build>/packages/<name>/<name>[.exe] + <name>.apak
#                     Built with ALL when ASTRAL_PACKAGE_GAMES is ON.
option(ASTRAL_PACKAGE_GAMES "Build every game's package (cook + .apak) with ALL" OFF)

function(astral_add_game name)
    cmake_parse_arguments(ARG "" "ASSETS" "SOURCES" ${ARGN})
    if(NOT ARG_SOURCES OR NOT ARG_ASSETS)
        message(FATAL_ERROR "astral_add_game(${name}): SOURCES and ASSETS are required")
    endif()
    get_filename_component(assets ${ARG_ASSETS} ABSOLUTE)
    if(TARGET astral_cook)
        set(cook $<TARGET_FILE:astral_cook>)
        set(cook_target astral_cook)
    elseif(TARGET Astral::astral_cook)
        set(cook $<TARGET_FILE:Astral::astral_cook>)
        set(cook_target Astral::astral_cook)
    else()
        message(FATAL_ERROR "astral_add_game(${name}): astral-cook is not available")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Astral::App)
    target_compile_definitions(${name} PRIVATE ASTRAL_GAME_ASSETS="${assets}" ASTRAL_GAME_PACK="${name}.apak")
    if(COMMAND astral_set_warnings)
        astral_set_warnings(${name})
    endif()

    set(stage ${CMAKE_BINARY_DIR}/packages/${name})
    set(pack ${stage}/${name}.apak)
    file(GLOB_RECURSE asset_files CONFIGURE_DEPENDS ${assets}/*)
    add_custom_command(
        OUTPUT ${pack}
        COMMAND ${cook} ${assets} -o ${CMAKE_CURRENT_BINARY_DIR}/${name}.cooked --pack ${pack} -q
        DEPENDS ${asset_files} ${cook_target}
        COMMENT "astral-cook ${name}: ${assets} -> ${pack}"
        VERBATIM)
    set(all "")
    if(ASTRAL_PACKAGE_GAMES)
        set(all ALL)
    endif()
    add_custom_target(${name}_package ${all}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}> ${stage}/
        DEPENDS ${pack} ${name}
        COMMENT "Staging ${stage}"
        VERBATIM)
endfunction()

# astral_check_features(<name> SOURCES <folder with features/<f>/ C++ sources> MANIFESTS <assets folder with <f>/feature.json>)
#
#   Adds the CTest test <name>_features_check: astral-features checks the boundaries of the game's feature
#   packages (ADR-0023): includes only of depended-on features and their exported headers, no internal
#   engine headers, no mutable global state. Call after enable_testing().
function(astral_check_features name)
    cmake_parse_arguments(ARG "" "SOURCES;MANIFESTS" "" ${ARGN})
    if(NOT ARG_SOURCES OR NOT ARG_MANIFESTS)
        message(FATAL_ERROR "astral_check_features(${name}): SOURCES and MANIFESTS are required")
    endif()
    get_filename_component(sources ${ARG_SOURCES} ABSOLUTE)
    get_filename_component(manifests ${ARG_MANIFESTS} ABSOLUTE)
    if(TARGET astral_features)
        set(tool astral_features)
    elseif(TARGET Astral::astral_features)
        set(tool Astral::astral_features)
    else()
        message(FATAL_ERROR "astral_check_features(${name}): astral-features is not available")
    endif()
    add_test(NAME ${name}_features_check COMMAND ${tool} check ${sources} ${manifests})
endfunction()
