#pragma once

#include "Astral/Core/ISubsystem.hpp"

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

namespace Astral {

class SystemManager {
public:
    template<typename T, typename... Args>
        requires std::derived_from<T, ISubsystem>
    T& PushSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& reference = *system;
        m_Systems.push_back(std::move(system));
        return reference;
    }

    void InitAll() {
        for (const auto& system : m_Systems) {
            system->OnInit();
        }
    }

    /// Belirli bir asamaya (Input, Gameplay, FixedSimulation, Transform, RenderExtraction)
    /// ait tum etkin alt sistemleri eklenme sirasina gore calistirir.
    void UpdateStage(SystemStage stage, FrameContext& context) {
        for (const auto& system : m_Systems) {
            if (system->IsEnabled() && system->GetStage() == stage) {
                system->OnUpdate(context);
            }
        }
    }

    /// Tum asamalari sirasiyla calistirir:
    /// Input -> Gameplay -> FixedSimulation -> Transform -> RenderExtraction
    void UpdateAll(FrameContext& context) {
        UpdateStage(SystemStage::Input, context);
        UpdateStage(SystemStage::Gameplay, context);
        UpdateStage(SystemStage::FixedSimulation, context);
        UpdateStage(SystemStage::Transform, context);
        UpdateStage(SystemStage::RenderExtraction, context);
    }

    /// Belirtilen asamada kayitli toplam sistem sayisini dondurur.
    [[nodiscard]] size_t GetSystemCount(SystemStage stage) const noexcept {
        size_t count = 0;
        for (const auto& system : m_Systems) {
            if (system->GetStage() == stage) {
                count++;
            }
        }
        return count;
    }

    /// Toplam kayitli sistem sayisini dondurur.
    [[nodiscard]] size_t Size() const noexcept { return m_Systems.size(); }

    void RenderAll(const RenderContext& context) {
        for (const auto& system : m_Systems) {
            if (system->IsEnabled()) {
                system->OnRender(context);
            }
        }
    }

    bool HasRenderSubsystem() const {
        for (const auto& system : m_Systems) {
            if (system->IsEnabled() && system->HasRenderPass()) {
                return true;
            }
        }
        return false;
    }

    void ShutdownAll() {
        for (auto system = m_Systems.rbegin(); system != m_Systems.rend(); ++system) {
            (*system)->OnShutdown();
        }
    }

private:
    std::vector<std::unique_ptr<ISubsystem>> m_Systems;
};

} // namespace Astral
