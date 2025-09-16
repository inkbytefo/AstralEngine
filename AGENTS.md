# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Build Commands

```bash
# Configure with automatic SDL3 build (recommended)
cmake .. -DASTRAL_VENDOR_SDL3=ON -DCMAKE_BUILD_TYPE=Debug

# Configure with pre-installed SDL3
cmake .. -DSDL3_ROOT="/path/to/sdl3" -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . --config Debug --parallel

# Run with debug logging
ASTRAL_LOG_LEVEL=DEBUG ./bin/AstralEngine_Debug
```

## Non-Obvious Project Patterns

### Subsystem Registration Order is Critical
Subsystems must be registered in specific order in [`main.cpp`](Source/main.cpp:36-43):
1. PlatformSubsystem (window, input)
2. AssetSubsystem (asset management) 
3. RenderSubsystem (3D rendering)

This order is enforced by dependencies - RenderSubsystem requires PlatformSubsystem's window handle.

### Shader Compilation System
Shaders are automatically compiled during build via CMake's [`compile_vulkan_shaders()`](CMakeLists.txt:760-793) function. Compiled `.spv` files are copied to output directory. Never manually copy shader files - the build system handles this.

### Vulkan Renderer Initialization Chain
VulkanRenderer requires a complex initialization sequence discovered in [`VulkanRenderer.cpp`](Source/Subsystems/Renderer/VulkanRenderer.cpp:175-254):
1. Initialize shaders from `Assets/Shaders/` (must be `.spv` files)
2. Create descriptor set layout
3. Initialize pipeline
4. Initialize vertex buffer
5. Create descriptor pool
6. Create uniform buffers
7. Create descriptor sets
8. Update descriptor sets

Failure at any step causes cascading shutdown of previous components.

### Event System Thread Safety
EventManager uses dual-mutex design ([`EventManager.h`](Source/Events/EventManager.h:73-74)):
- `m_handlersMutex` for subscriber management
- `m_queueMutex` for event publishing
Events must be processed on main thread via [`ProcessEvents()`](Source/Events/EventManager.h:42).

### Logger File Output Location
Logger automatically creates log files in executable's directory ([`Logger.h`](Source/Core/Logger.h:20)). Use [`InitializeFileLogging()`](Source/Core/Logger.h:56) without path parameter for default behavior.

### Base Path Resolution
Engine resolves base path from [`argv[0]`](Source/main.cpp:30) using `std::filesystem::path(argv[0]).parent_path()`. This is critical for shader/asset loading in [`VulkanRenderer.cpp`](Source/Subsystems/Renderer/VulkanRenderer.cpp:320-322).

### Platform-Specific Vulkan Definitions
CMake automatically sets platform-specific Vulkan defines ([`CMakeLists.txt`](CMakeLists.txt:132-160)):
- Windows: `VK_USE_PLATFORM_WIN32_KHR`
- Linux: `VK_USE_PLATFORM_XCB_KHR`
- Enables AVX2 optimizations on MSVC Release builds

### SDL3 Integration Strategy
The project uses multi-strategy SDL3 detection ([`CMakeLists.txt`](CMakeLists.txt:172-229)):
1. Check `External/SDL3/` directory first
2. Try system SDL3 via CMake Config mode
3. Fallback to pkg-config
4. Finally use FetchContent to build from source

### Memory Management Pattern
All Vulkan resources follow RAII pattern with explicit [`Shutdown()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:256-313) methods that must be called in reverse initialization order. Never rely on destructors alone for Vulkan cleanup.