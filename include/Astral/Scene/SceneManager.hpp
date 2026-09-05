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

    using SceneChangeCallback = std::function<void(const std::shared_ptr<Scene>& oldScene, const std::shared_ptr<Scene>& newScene)>;

    /// Aktif sahneyi guvenle degistirir (Onceki sahneyi durdurur, yenisini baslatir ve bildirir)
    void SetActiveScene(std::shared_ptr<Scene> scene) {
        if (m_ActiveScene == scene) return;

        std::shared_ptr<Scene> oldScene = m_ActiveScene;
        if (m_ActiveScene) {
            if (m_ActiveScene->IsRunning()) {
                m_ActiveScene->OnRuntimeStop();
            }
            std::cout << "[Astral::SceneManager] Sahne kaldirildi: " << m_ActiveScene->GetName() << "\n";
        }

        m_ActiveScene = std::move(scene);

        if (m_ActiveScene) {
            if (m_IsRuntimeActive && !m_ActiveScene->IsRunning()) {
                m_ActiveScene->OnRuntimeStart();
            }
            std::cout << "[Astral::SceneManager] Yeni sahne yuklendi: " << m_ActiveScene->GetName() << "\n";
        }

        if (m_OnSceneChanged) {
            m_OnSceneChanged(oldScene, m_ActiveScene);
        }
    }

    /// Aktif sahneyi dondurur
    [[nodiscard]] std::shared_ptr<Scene> GetActiveScene() const noexcept {
        return m_ActiveScene;
    }

    [[nodiscard]] bool HasActiveScene() const noexcept {
        return m_ActiveScene != nullptr;
    }

    /// Runtime simülasyon aktiflik durumunu ayarlar
    void SetRuntimeActive(bool active) noexcept {
        m_IsRuntimeActive = active;
        if (m_ActiveScene) {
            if (active && !m_ActiveScene->IsRunning()) {
                m_ActiveScene->OnRuntimeStart();
            } else if (!active && m_ActiveScene->IsRunning()) {
                m_ActiveScene->OnRuntimeStop();
            }
        }
    }
    [[nodiscard]] bool IsRuntimeActive() const noexcept { return m_IsRuntimeActive; }

    void SetOnSceneChanged(SceneChangeCallback callback) {
        m_OnSceneChanged = std::move(callback);
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
    bool m_IsRuntimeActive{ false };
    SceneChangeCallback m_OnSceneChanged;
};

} // namespace Astral
