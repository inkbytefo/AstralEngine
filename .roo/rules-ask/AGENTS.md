# Project Documentation Rules (Non-Obvious Only)

## Hidden Documentation Locations
- Platform subsystem has detailed knowledge base in [`Source/Subsystems/Platform/knowledge.md`](Source/Subsystems/Platform/knowledge.md:1) with SDL3 integration details
- Architecture document in [`MIMARI.md`](MIMARI.md:1) contains Turkish design philosophy and subsystem interaction patterns
- Build instructions in [`BUILD_INSTRUCTIONS.md`](BUILD_INSTRUCTIONS.md:1) have platform-specific dependency requirements
- Integration guides for SDL3, Vulkan, ImGui, and Jolt Physics are separate files in root directory

## Counterintuitive Code Organization
- Turkish comments in architecture docs but English code comments - mixed language approach by design
- Vertex struct in [`RendererTypes.h`](Source/Subsystems/Renderer/RendererTypes.h:30) contains both data and Vulkan binding logic
- Event system uses type_index mapping but requires manual ProcessEvents() calls on main thread
- Logger creates files automatically in executable directory without explicit path configuration

## Misleading Naming Conventions
- "GraphicsDevice" is actually VulkanGraphicsContext wrapper, not standalone device
- "VulkanRenderer" is simplified implementation - many methods are placeholders
- "AssetSubsystem" currently only manages base path resolution, not actual assets
- "PlatformSubsystem" knowledge.md contains more implementation details than header files

## Important Context Not Evident from File Structure
- SDL3 integration uses multi-strategy detection (External → System → pkg-config → FetchContent)
- Vulkan initialization requires specific sequence - any failure causes cascading shutdown
- Shader compilation happens during CMake configure, not at runtime
- Base path resolution from argv[0] is critical for all asset loading operations
- EventManager singleton must be initialized before any event subscriptions

## Architecture Decisions Not Obvious
- Subsystem registration order is enforced by dependencies, not arbitrary
- Dual-mutex design in EventManager prevents deadlocks but requires main thread processing
- RAII pattern with explicit Shutdown() calls for Vulkan resources (destructors not sufficient)
- Turkish architecture documentation reflects original development team's language preference