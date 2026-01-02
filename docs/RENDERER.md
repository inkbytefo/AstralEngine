# Renderer Architecture

The rendering module of Astral Engine is designed around a modern **Render Hardware Interface (RHI)** abstraction, currently implemented with a **Vulkan 1.3** backend.

## Design Philosophy

- **Explicit Control:** The RHI exposes low-level control over memory and synchronization (Barriers, Transitions).
- **Stateless Command Lists:** Command recording is decoupled from submission.
- **Dynamic Rendering:** We exclusively use Vulkan 1.3 Dynamic Rendering, eliminating the complexity of `VkRenderPass` and `VkFramebuffer` objects.

## RHI Abstraction (`Source/Subsystems/Renderer/RHI`)

The RHI layer provides platform-agnostic interfaces:
- `IRHIDevice`: Factory for creating resources (Buffers, Textures, Pipelines).
- `IRHICommandList`: Interface for recording draw commands.
- `IRHIResource`: Base class for GPU resources.
- `IRHIPipeline`: Graphics pipeline state.

## Vulkan Backend (`Source/Subsystems/Renderer/RHI/Vulkan`)

The Vulkan implementation is "production-grade" with a focus on robustness and performance.

### Key Features
1.  **Dynamic Rendering:**
    - Uses `vkCmdBeginRendering` instead of Render Passes.
    - Simplifies pipeline creation and swapchain management.
    - Supports dynamic attachment layouts.

2.  **Memory Management:**
    - Integrated **Vulkan Memory Allocator (VMA)** for efficient GPU memory allocation.
    - RAII wrappers (`VulkanBuffer`, `VulkanTexture`) ensure no leaks.

3.  **Synchronization:**
    - **Per-Frame Resources:** Command Pools, Uniform Buffers, and Semaphores are duplicated per frame-in-flight (Double/Triple Buffering).
    - **Staging Buffers:** Dedicated transfer queue logic (via `CreateAndUploadBuffer`) for CPU-to-GPU data uploads.

4.  **Pipeline Management:**
    - Pipelines are created with `VkPipelineRenderingCreateInfo` to be compatible with dynamic rendering.
    - Shader reflection (planned) or explicit descriptor layouts.

### Frame Lifecycle
1.  **BeginFrame:**
    - Wait for fences.
    - Acquire next swapchain image.
    - Handle `VK_ERROR_OUT_OF_DATE_KHR` (Swapchain Recreation).
2.  **Record Commands:**
    - `BeginRendering` (binds swapchain image and depth buffer).
    - Set Viewport/Scissor.
    - Bind Pipeline & Descriptors.
    - Draw.
    - `EndRendering` (transitions image layout to `PRESENT_SRC_KHR`).
3.  **Submit:**
    - Submit command buffer to Graphics Queue.
    - Signal `RenderFinished` semaphore.
4.  **Present:**
    - Queue Present.

## PBR Material System

Astral Engine uses a standard metallic-roughness PBR workflow.

### Supported Maps
- **Albedo (Base Color):** Standard RGB color map.
- **Normal Map:** Tangent space normal mapping for surface detail.
- **Metallic-Roughness Map:** Encapsulated into a single texture (G: Roughness, B: Metalness).
- **AO (Ambient Occlusion):** Baked occlusion data.
- **Emissive Map:** Supports self-illuminating surfaces.

### Shading Model
- **Cook-Torrance BRDF:** Using GGX distribution, Schlick-GGX geometry, and Fresnel-Schlick.
- **Dynamic Calculation:** All shading is computed in the fragment shader with support for point, directional, and spot lights (planned updates).

## Initialization Robustness

The engine implements a multi-layered fallback system to handle diverse Windows environments (RDP, old drivers, headless setups).

### 1. Window Fallback
If `SDL_CreateWindow` with `SDL_WINDOW_VULKAN` fails, the `PlatformSubsystem` retries without the Vulkan flag. This ensures the engine can always acquire a system window.

### 2. Manual Surface Hooking
Since a non-Vulkan window prevents standard `SDL_Vulkan_CreateSurface` from working, the RHI layer manually creates the surface:
- Uses `VK_KHR_win32_surface` extension.
- Retrieves `HWND` directly from SDL3 properties.
- Calls `vkCreateWin32SurfaceKHR` to bind the Vulkan surface directly to the OS window handle.

### 3. Native Extension Loading
Bypasses SDL's internal extension enumeration (which can be unstable on some drivers) in favor of explicit manual extension lists (`VK_KHR_surface`, `VK_KHR_win32_surface`).

## Depth Buffering
- The engine automatically manages a Depth-Stencil attachment.
- Format is automatically selected (D32_SFLOAT preferred).
- Integrated into the Dynamic Rendering pass via `GetDepthBuffer()`.
