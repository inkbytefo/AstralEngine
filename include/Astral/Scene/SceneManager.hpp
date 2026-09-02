#pragma once

#include "Astral/Scene/Scene.hpp"
#include <memory>
#include <functional>
#include <iostream>

namespace Astral {

/**
 * @brief Manages scene ownership and safe transitions.
 *
 * Enforces clean lifecycle boundaries between editor authoring, scene transitions,
 * and runtime simulation. Holds the active scene via std::shared_ptr ownership.
 */
class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() {
        UnloadCurrentScene();
    }

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) noexcept = default;
    SceneManager& operator=(SceneManager&&) noexcept = default;

    /// Aktif sahneyi guvenle degistirir (Onceki sahneyi durdurur ve kaynaklari serbest birakir)
    void SetActiveScene(std::shared_ptr<Scene> scene) {
        if (m_ActiveScene == scene) return;

        UnloadCurrentScene();
        m_ActiveScene = std::move(scene);

        if (m_ActiveScene) {
            std::cout << "[Astral::SceneManager] Yeni sahne yuklendi: " << m_ActiveScene->GetName() << "\n";
        }
    }

    /// Aktif sahneyi dondurur
    [[nodiscard]] std::shared_ptr<Scene> GetActiveScene() const noexcept {
        return m_ActiveScene;
    }

    [[nodiscard]] bool HasActiveScene() const noexcept {
        return m_ActiveScene != nullptr;
    }

    /// Mevcut sahneyi guvenle sonlandirir
    void UnloadCurrentScene() {
        if (m_ActiveScene) {
            if (m_ActiveScene->IsRunning()) {
                m_ActiveScene->OnRuntimeStop();
            }
            std::cout << "[Astral::SceneManager] Sahne kaldirildi: " << m_ActiveScene->GetName() << "\n";
            m_ActiveScene.reset();
        }
    }

private:
    std::shared_ptr<Scene> m_ActiveScene{ nullptr };
};

} // namespace Astral
