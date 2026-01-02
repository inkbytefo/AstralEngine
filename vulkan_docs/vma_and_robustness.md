# Vulkan Memory Allocator (VMA) & Best Practices

## VMA Integration
Astral Engine uses VMA for efficient GPU memory management. This avoids the overhead of many small allocations and handles memory types automatically.

### Key Concepts
- **VmaAllocator:** The main object used to allocate buffers and images.
- **VmaAllocation:** Represents a specific allocation of memory.
- **VMA_MEMORY_USAGE_AUTO:** Automatically selects the best memory type based on usage flags.

### Best Practices
1. **Prefer Device Local Memory:** Use `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE` for resources accessed frequently by the GPU (textures, vertex buffers).
2. **Host Visible for Staging:** Use `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` for staging buffers.
3. **Dedicated Allocations:** For large resources (like the depth buffer), use `VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT`.

## Robustness fallback
The engine implements a fallback for surface creation:
1. Try `SDL_Vulkan_CreateSurface`.
2. If it fails (e.g., in some RDP or VM environments), manually create a Win32 surface using `vkCreateWin32SurfaceKHR` and the `HWND` from SDL properties.
This ensures the engine can run in diverse Windows environments.
