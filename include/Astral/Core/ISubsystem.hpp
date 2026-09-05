#pragma once

#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/Input/ActionMap.hpp"
#include "Astral/Core/Events/EventBus.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Window.hpp"

namespace Astral {

class Scene;
class Entity;
class JobSystem;
class Window;
struct RenderContext;

struct FrameContext {
    Registry& registry;
    InputSystem& input;
    ActionMap& actions;
    EventBus& events;
    JobSystem& jobSystem;
    Window* window{nullptr};
    float deltaTime{0.0f};
};

/// Alt sistem guncelleme asamalari (deterministik calisma sirasi sozlesmesi)
enum class SystemStage : uint8_t {
    Input = 0,            // 1. Olaylar, fare, klavye ve ham girdi okuma
    Gameplay = 1,         // 2. Oyun/istemci mantigi, hareket, kamera kontrolleri (degisken delta)
    FixedSimulation = 2,  // 3. Fizik, carpişma ve sabit adimli simulasyon (sabit timestep)
    Transform = 3,        // 4. World transform ve hiyerarsi hesaplama (son konumlar kesinlesir)
    RenderExtraction = 4  // 5. GPU tamponlarina ayni karede veri cikarimi (sifir kare gecikmesi)
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    virtual void OnInit() = 0;
    virtual void OnUpdate(FrameContext& context) = 0;
    virtual void OnShutdown() = 0;

    /// Bu sistemin hangi asamada calisacagini belirler (Varsayilan: Gameplay)
    [[nodiscard]] virtual SystemStage GetStage() const { return m_Stage; }
    void SetStage(SystemStage stage) noexcept { m_Stage = stage; }

    /// Swapchain render asamasi hook'u. Gerek duymayan sistemler (or. Physics, Input) implemente etmek zorunda degildir.
    virtual void OnRender(const RenderContext& /*context*/) {}

    /// Eger true donerse, Application swapchain goruntusunu UI/render sistemi icin hazirlar (eColorAttachmentOptimal).
    virtual bool HasRenderPass() const { return false; }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_Enabled; }

protected:
    explicit ISubsystem(SystemStage defaultStage = SystemStage::Gameplay)
        : m_Stage(defaultStage) {}

private:
    SystemStage m_Stage = SystemStage::Gameplay;
    bool m_Enabled = true;
};

} // namespace Astral

