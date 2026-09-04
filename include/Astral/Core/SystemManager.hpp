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

    void UpdateAll(FrameContext& context) {
        for (const auto& system : m_Systems) {
            if (system->IsEnabled()) {
                system->OnUpdate(context);
            }
        }
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
