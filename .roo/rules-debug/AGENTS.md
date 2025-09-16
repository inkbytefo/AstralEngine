# Project Debug Rules (Non-Obvious Only)

## Hidden Log Locations
- Logger creates files in executable directory automatically - check `AstralEngine.log` next to binary
- Vulkan validation errors only appear when `ASTRAL_VULKAN_VALIDATION=ON` and build is Debug
- Shader compilation errors are logged during CMake configure phase, not runtime

## Critical Debug Flags
- `ASTRAL_LOG_LEVEL=TRACE` enables verbose logging including Vulkan API calls
- `ASTRAL_VULKAN_VALIDATION=ON` enables Vulkan validation layers (Debug builds only)
- CMake `--debug-find` shows detailed dependency detection process

## Silent Failure Points
- VulkanRenderer initialization chain fails silently if any step fails - check logs for "Failed to initialize" messages
- Shader file loading fails silently if base path resolution fails - verify `GetBasePath()` returns correct directory
- Event subscriptions fail silently if EventManager not initialized - ensure subsystems registered in correct order

## Required Environment Variables
- `ASTRAL_LOG_LEVEL` controls logging verbosity (Trace, Debug, Info, Warning, Error, Critical)
- `VULKAN_SDK` required for system Vulkan detection (optional if using External/Vulkan)
- `SDL3_ROOT` required for pre-installed SDL3 (optional if using ASTRAL_VENDOR_SDL3=ON)

## Debug Build Specifics
- Debug builds add `_d` suffix to executable name (`AstralEngine_d.exe`)
- Vulkan validation layers only enabled in Debug builds with `ASTRAL_VULKAN_VALIDATION=ON`
- Shader compilation happens during build, not runtime - check build output for compilation errors

## Common Debug Scenarios
- **"SDL3 not found"**: Enable `-DASTRAL_VENDOR_SDL3=ON` or set `SDL3_ROOT` path
- **"Vulkan initialization failed"**: Check validation layer messages in debug output
- **"Shader file not found"**: Verify base path resolution and shader compilation during build
- **Subsystem initialization order**: Platform → Asset → Render sequence is mandatory

## Performance Debugging
- Use `ASTRAL_ENABLE_PROFILING=ON` CMake option to enable profiling markers
- Frame time tracking built into VulkanRenderer - check `m_frameTime` variable
- Event processing performance visible with TRACE log level