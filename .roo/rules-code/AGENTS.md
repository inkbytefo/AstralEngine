# Project Coding Rules (Non-Obvious Only)

## Critical Dependencies
- Always use safeWriteJson() from src/utils/ instead of JSON.stringify for file writes (prevents corruption)
- API retry mechanism in src/api/providers/utils/ is mandatory (not optional as it appears)
- Database queries MUST use the query builder in packages/evals/src/db/queries/ (raw SQL will fail)
- Provider interface in packages/types/src/ has undocumented required methods

## Vulkan Resource Management
- All Vulkan resources follow RAII pattern with explicit Shutdown() methods that must be called in reverse initialization order
- Never rely on destructors alone for Vulkan cleanup - always call Shutdown() explicitly
- VulkanRenderer initialization chain must complete fully or causes cascading shutdown of previous components

## Event System Threading
- EventManager uses dual-mutex design: m_handlersMutex for subscriber management, m_queueMutex for event publishing
- Events must be processed on main thread via ProcessEvents() - never call from worker threads
- Event subscriptions return handler IDs that must be stored for proper cleanup

## Shader and Asset Loading
- Shaders are automatically compiled during build via CMake's compile_vulkan_shaders() function
- Base path resolution uses argv[0] and is critical for shader/asset loading - always check GetBasePath()
- Compiled .spv files are copied to output directory automatically - never manually copy shader files

## Subsystem Registration Order
- Subsystems must be registered in specific order: PlatformSubsystem → AssetSubsystem → RenderSubsystem
- This order is enforced by dependencies - RenderSubsystem requires PlatformSubsystem's window handle
- Registration order violations cause runtime initialization failures

## Memory Management Patterns
- Use std::unique_ptr for ownership transfer and RAII cleanup
- Prefer stack allocation over heap when possible for performance
- All Vulkan resources require explicit Shutdown() calls in reverse initialization order