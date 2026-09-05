#pragma once

#include <vulkan/vulkan.hpp>

namespace Astral {

class Scene;

/// @brief Render asamasi baglami — Alt sistemlerin swapchain uzerine ek cizimler (or. ImGui editor panelleri,
/// oyun ici HUD vb.) yapabilmesini saglayan Vulkan tabanli render yapisi.
struct RenderContext {
    vk::CommandBuffer commandBuffer;
    vk::ImageView swapchainImageView;
    vk::Extent2D swapchainExtent;
    Scene* activeScene{nullptr};
    float gpuTimeMs{0.0f};
    float cpuTimeMs{0.0f};
};

} // namespace Astral
