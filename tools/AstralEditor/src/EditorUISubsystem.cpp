#include "EditorUISubsystem.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Project/Project.hpp"
#include "Astral/Core/Events/EngineEvents.hpp"
#include <iostream>

namespace Astral {

EditorUISubsystem::EditorUISubsystem(Application& app)
    : m_App(app) {}

void EditorUISubsystem::SetSelectedEntity(const Entity& entity) {
    m_SelectedEntity = entity;
    m_App.SetHighlightEntity(entity.GetHandle());
}

void EditorUISubsystem::OnInit() {
    auto* context = m_App.GetVulkanContext();
    auto* window = m_App.GetWindow();
    if (!context || !window) {
        std::cerr << "[Astral::EditorUISubsystem Hata]: VulkanContext veya Window hazir degil!\n";
        return;
    }

    m_EditorUI = std::make_unique<EditorUI>(
        *context,
        window->GetNativeWindow(),
        window->GetInputSystem()
    );
    m_EditorUI->SetRenderer(m_App.GetRenderer());

    m_EditorUI->onPlayToggle = [this]() {
        if (IsPlaying()) StopPlayMode(); else StartPlayMode();
    };
    m_EditorUI->onPauseToggle = [this]() {
        TogglePausePlayMode();
    };
    m_EditorUI->onStopPlay = [this]() {
        StopPlayMode();
    };

    // Genel Runtime Picking olayini dinle ve editor secim durumunu guncelle
    m_PickSub = m_App.GetEventBus().Subscribe<RuntimePickEvent>([this](const RuntimePickEvent& e) {
        if (e.result.hasHit && e.scene && e.result.hitEntity != NullEntityHandle) {
            m_SelectedEntity = Entity(e.result.hitEntity, e.scene);
        } else {
            m_SelectedEntity = Entity();
        }
        m_App.SetHighlightEntity(m_SelectedEntity.GetHandle());
    });

    // Sahne yuklendiginde eski sahne secimini guvenle sifirla
    m_SceneSub = m_App.GetEventBus().Subscribe<SceneLoadedEvent>([this](const SceneLoadedEvent&) {
        m_SelectedEntity = Entity();
        m_App.SetHighlightEntity(NullEntityHandle);
    });
}

void EditorUISubsystem::StartPlayMode() {
    if (m_EditorMode != EditorMode::Edit) return;

    auto activeScene = m_App.GetActiveScene();
    if (!activeScene) return;

    m_AuthoringSceneBackup = activeScene;
    auto runtimeScene = activeScene->Clone();

    m_App.SetActiveScene(runtimeScene);
    m_App.SetPhysicsSimulated(true);
    m_App.SetPaused(false);
    m_EditorMode = EditorMode::Play;

    m_SelectedEntity = Entity();
    m_App.SetHighlightEntity(NullEntityHandle);

    m_App.GetEventBus().Publish(PlayModeChangedEvent{ .isPlaying = true });
    std::cout << "[Astral::EditorUISubsystem] PLAY MODU: Authoring sahnesi yedeklendi, Runtime klonu aktif.\n";
}

void EditorUISubsystem::StopPlayMode() {
    if (m_EditorMode == EditorMode::Edit) return;

    if (m_AuthoringSceneBackup) {
        m_App.SetActiveScene(m_AuthoringSceneBackup);
        m_AuthoringSceneBackup.reset();
    }

    m_App.SetPhysicsSimulated(false);
    m_App.SetPaused(false);
    m_EditorMode = EditorMode::Edit;

    m_SelectedEntity = Entity();
    m_App.SetHighlightEntity(NullEntityHandle);

    m_App.GetEventBus().Publish(PlayModeChangedEvent{ .isPlaying = false });
    std::cout << "[Astral::EditorUISubsystem] STOP: Orijinal authoring sahnesine donuldu.\n";
}

void EditorUISubsystem::TogglePausePlayMode() {
    if (m_EditorMode == EditorMode::Play) {
        m_EditorMode = EditorMode::Paused;
        m_App.SetPaused(true);
        std::cout << "[Astral::EditorUISubsystem] PAUSE: Simulasyon duraklatildi.\n";
    } else if (m_EditorMode == EditorMode::Paused) {
        m_EditorMode = EditorMode::Play;
        m_App.SetPaused(false);
        std::cout << "[Astral::EditorUISubsystem] RESUME: Simulasyon devam ediyor.\n";
    }
}

void EditorUISubsystem::OnUpdate(FrameContext& /*context*/) {
    if (auto* window = m_App.GetWindow()) {
        auto& input = window->GetInputSystem();
        if (input.IsKeyJustPressed(GLFW_KEY_F5)) {
            if (IsPlaying()) {
                StopPlayMode();
            } else {
                StartPlayMode();
            }
        }
        if (input.IsKeyJustPressed(GLFW_KEY_F6)) {
            TogglePausePlayMode();
        }
    }

    if (m_EditorUI && m_App.GetRenderer()) {
        auto& viewportPanel = m_EditorUI->GetViewportPanel();
        if (viewportPanel.HasPendingResize()) {
            auto newSize = viewportPanel.GetPendingResize();
            if (newSize.x > 0.0f && newSize.y > 0.0f) {
                viewportPanel.CleanupDescriptorSet();
                m_App.GetRenderer()->Resize(static_cast<int>(newSize.x), static_cast<int>(newSize.y));
            }
            viewportPanel.ClearPendingResize();
        }
    }
}

void EditorUISubsystem::OnRender(const RenderContext& context) {
    if (auto* window = m_App.GetWindow()) {
        std::string projName = "AstralEngine";
        if (auto proj = Project::GetActive()) {
            projName = proj->GetConfig().name;
        }
        std::string sceneName = context.activeScene ? context.activeScene->GetName() : "Untitled";
        std::string desiredTitle = "AstralEngine Editor - [" + projName + "] - " + sceneName;
        if (window->GetTitle() != desiredTitle) {
            window->SetTitle(desiredTitle);
        }
    }

    if (m_EditorUI && context.activeScene) {
        m_EditorUI->BeginFrame();
        m_EditorUI->RenderPanels(
            *context.activeScene,
            m_SelectedEntity,
            context.gpuTimeMs,
            context.cpuTimeMs
        );
        m_App.SetHighlightEntity(m_SelectedEntity.GetHandle());
        m_EditorUI->EndFrame(
            context.commandBuffer,
            context.swapchainImageView,
            context.swapchainExtent
        );
    }
}

void EditorUISubsystem::OnShutdown() {
    m_App.GetEventBus().Unsubscribe(m_PickSub);
    m_App.GetEventBus().Unsubscribe(m_SceneSub);
    m_EditorUI.reset();
}

} // namespace Astral
