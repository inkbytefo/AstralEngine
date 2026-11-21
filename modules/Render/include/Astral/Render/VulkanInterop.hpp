#pragma once

// Vulkan-level hooks for code that has to talk to Vulkan directly: the window
// system (surfaces) and UI libraries (ImGui). Game code normally never needs this.

#include "Astral/Render/Renderer.hpp"

#include <vulkan/vulkan.h>

#include <functional>

namespace Astral::Render {

struct VulkanHandles {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    uint32_t apiVersion = 0;
};

VulkanHandles GetVulkanHandles(Renderer& renderer);

// Hands a window surface to the renderer, which creates and owns the swapchain
// (and destroys the surface on shutdown).
void AttachSurface(Renderer& renderer, VkSurfaceKHR surface, uint32_t width, uint32_t height);
VkFormat GetSurfaceFormat(Renderer& renderer);
uint32_t GetSwapchainImageCount(Renderer& renderer);

// Called during EndFrame, after PresentView, with the swapchain image bound as a
// colour attachment target (layout COLOR_ATTACHMENT_OPTIMAL). Used to draw UI.
struct OverlayContext {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkImageView targetView = VK_NULL_HANDLE;
    VkExtent2D extent{};
};
void SetOverlayCallback(Renderer& renderer, std::function<void(const OverlayContext&)> callback);

// A view's final image, in SHADER_READ_ONLY_OPTIMAL after the frame it was rendered in.
// Lets the editor show a view inside a UI panel.
VkImageView GetViewImageView(Renderer& renderer, ViewHandle view);
VkSampler GetLinearSampler(Renderer& renderer);

} // namespace Astral::Render
