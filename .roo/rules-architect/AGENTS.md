# Project Architecture Rules (Non-Obvious Only)

## Hidden Coupling Between Components
- RenderSubsystem requires PlatformSubsystem's window handle - registration order is mandatory
- VulkanRenderer depends on GraphicsDevice from RenderSubsystem - must be retrieved via GetGraphicsDevice()
- EventManager singleton must be initialized before any event subscriptions or publishing
- Base path resolution from argv[0] affects all asset loading operations throughout the engine

## Undocumented Architectural Decisions
- Turkish architecture documentation (MIMARI.md) reflects original development team's language preference
- Mixed language approach: Turkish docs, English code comments by intentional design
- Subsystem registration order enforced by dependencies, not arbitrary convention
- Dual-mutex EventManager design prevents deadlocks but requires main thread processing

## Non-Standard Patterns That Must Be Followed
- RAII pattern with explicit Shutdown() calls for Vulkan resources - destructors alone insufficient
- Shader compilation during CMake configure phase, not at runtime
- Multi-strategy SDL3 detection: External → System → pkg-config → FetchContent fallback
- Base path resolution using argv[0] and std::filesystem - critical for all asset operations

## Performance Bottlenecks Discovered Through Investigation
- VulkanRenderer initialization chain must complete fully or causes cascading shutdown
- Event processing requires main thread via ProcessEvents() - cannot be offloaded to worker threads
- Shader file loading depends on base path resolution - failures are silent without proper logging
- Frame time tracking built into VulkanRenderer but requires TRACE log level for visibility

## Architectural Constraints Not Obvious from Code Structure
- Platform-specific Vulkan defines set automatically by CMake (VK_USE_PLATFORM_WIN32_KHR, etc.)
- SDL3 integration uses conditional compilation with #ifdef ASTRAL_USE_SDL3 guards
- Logger file output location determined automatically - no explicit path configuration needed
- Vertex struct contains both data and Vulkan binding logic - not pure data container

## Design Patterns That Violate Typical Practices
- EventManager acts as service locator but requires manual ProcessEvents() calls
- GraphicsDevice is actually VulkanGraphicsContext wrapper, not standalone device
- AssetSubsystem currently only manages base path - not full asset management as name suggests
- VulkanRenderer is simplified implementation with many placeholder methods

## Cross-Platform Considerations
- Windows: Automatic SDL3.dll copying via CMake post-build commands
- Linux: X11/Wayland backend detection handled automatically by SDL3
- macOS: Retina display support and Metal integration considerations for future
- AVX2 optimizations enabled automatically on MSVC Release builds

## Future Extension Points
- ECS subsystem integration planned but not yet implemented
- Physics and Audio subsystems referenced but not created
- Multi-threading support designed but subsystem updates currently single-threaded
- Profiling integration available but requires ASTRAL_ENABLE_PROFILING=ON CMake flag