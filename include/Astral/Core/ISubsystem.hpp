#pragma once

#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/Input/ActionMap.hpp"
#include "Astral/Core/Events/EventBus.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Window.hpp"

#include <vulkan/vulkan.hpp>

namespace Astral {

class Scene;
class Entity;

struct FrameContext {
    Registry& registry;
    InputSystem& input;
    ActionMap& actions;
    EventBus& events;
    Window& window;
    float deltaTime;
};

/// Render asamasi baglami — Alt sistemlerin swapchain uzerine ek cizimler (or. ImGui editor panelleri,
/// oyun ici HUD vb.) yapabilmesini saglar.
struct RenderContext {
    vk::CommandBuffer commandBuffer;
    vk::ImageView swapchainImageView;
    vk::Extent2D swapchainExtent;
    Scene* activeScene = nullptr;
    Entity* selectedEntity = nullptr;
    float gpuTimeMs = 0.0f;
    float cpuTimeMs = 0.0f;
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    virtual void OnInit() = 0;
    virtual void OnUpdate(FrameContext& context) = 0;
    virtual void OnShutdown() = 0;

    /// Swapchain render asamasi hook'u. Gerek duymayan sistemler (or. Physics, Input) implemente etmek zorunda degildir.
    virtual void OnRender(const RenderContext& /*context*/) {}

    /// Eger true donerse, Application swapchain goruntusunu UI/render sistemi icin hazirlar (eColorAttachmentOptimal).
    virtual bool HasRenderPass() const { return false; }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    bool m_Enabled = true;
};

} // namespace Astral

