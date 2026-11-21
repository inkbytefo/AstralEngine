#pragma once

#include "Vk/VkCommon.hpp"
#include "Vk/Resources.hpp"

#include <vector>

namespace Astral::Render::Vk {

class Device;

class Swapchain {
public:
    Swapchain(Device& device, vk::SurfaceKHR surface, uint32_t width, uint32_t height);
    ~Swapchain();

    void Recreate(uint32_t width, uint32_t height);

    // Returns false if the swapchain is out of date (caller should Recreate and skip the frame).
    bool Acquire(vk::Semaphore signal, uint32_t& imageIndex);
    // Returns false if the swapchain became out of date or suboptimal.
    bool Present(vk::Queue queue, uint32_t imageIndex);

    vk::Format Format() const { return m_Format; }
    vk::Extent2D Extent() const { return m_Extent; }
    uint32_t ImageCount() const { return static_cast<uint32_t>(m_Images.size()); }
    vk::Image ImageAt(uint32_t i) const { return m_Images[i]; }
    vk::ImageView ViewAt(uint32_t i) const { return m_Views[i]; }
    ImageState& StateAt(uint32_t i) { return m_States[i]; }
    vk::Semaphore RenderFinished(uint32_t i) const { return m_RenderFinished[i]; }

private:
    void Create(uint32_t width, uint32_t height);
    void Destroy();

    Device& m_Device;
    vk::SurfaceKHR m_Surface;
    vk::SwapchainKHR m_Swapchain;
    vk::Format m_Format = vk::Format::eUndefined;
    vk::Extent2D m_Extent;
    std::vector<vk::Image> m_Images;
    std::vector<vk::ImageView> m_Views;
    std::vector<ImageState> m_States;
    std::vector<vk::Semaphore> m_RenderFinished; // one per image (reuse rules of present)
};

} // namespace Astral::Render::Vk
