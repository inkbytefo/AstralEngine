# Vulkan 1.3 - Dynamic Rendering & Synchronization 2

## Dynamic Rendering (VK_KHR_dynamic_rendering)
Dynamic rendering allows you to begin rendering without creating a `VkRenderPass` or `VkFramebuffer` object. This simplifies the setup significantly.

### Enabling
In Vulkan 1.3, it is a core feature. Enable it in `VkPhysicalDeviceVulkan13Features`:
```cpp
VkPhysicalDeviceVulkan13Features features13{};
features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
features13.dynamicRendering = VK_TRUE;
```

### Usage
Use `vkCmdBeginRendering` instead of `vkCmdBeginRenderPass`.
```cpp
VkRenderingInfo renderingInfo{};
renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
renderingInfo.renderArea = { {0, 0}, {width, height} };
renderingInfo.layerCount = 1;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachments = &colorAttachment;
renderingInfo.pDepthAttachment = &depthAttachment;

vkCmdBeginRendering(commandBuffer, &renderingInfo);
```

## Synchronization 2 (VK_KHR_synchronization2)
Synchronization 2 provides a more unified and simplified interface for pipeline barriers and events.

### Enabling
Enable it in `VkPhysicalDeviceVulkan13Features`:
```cpp
features13.synchronization2 = VK_TRUE;
```

### Usage
Use `vkCmdPipelineBarrier2` instead of `vkCmdPipelineBarrier`.
```cpp
VkDependencyInfo dependencyInfo{};
dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
dependencyInfo.imageMemoryBarrierCount = 1;
dependencyInfo.pImageMemoryBarriers = &imageBarrier;

vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
```
Barriers now use `VkImageMemoryBarrier2`, which includes stage and access masks in a single struct.
