# Installs every module registered by astral_add_module() and the package config,
# so consumers can write: find_package(Astral CONFIG REQUIRED COMPONENTS Core ...)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

get_property(astral_modules GLOBAL PROPERTY ASTRAL_MODULES)
install(TARGETS ${astral_modules}
    EXPORT AstralTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(astral_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/Astral)
install(EXPORT AstralTargets NAMESPACE Astral:: DESTINATION ${astral_cmake_dir})

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/AstralConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AstralConfig.cmake
    INSTALL_DESTINATION ${astral_cmake_dir}
    PATH_VARS CMAKE_INSTALL_DATADIR)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AstralConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
    ${CMAKE_CURRENT_LIST_DIR}/AstralShaders.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AstralConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AstralConfigVersion.cmake
    DESTINATION ${astral_cmake_dir})
