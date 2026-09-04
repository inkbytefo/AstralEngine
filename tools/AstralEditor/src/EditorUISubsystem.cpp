#include "EditorUISubsystem.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Project/Project.hpp"
#include <iostream>

namespace Astral {

EditorUISubsystem::EditorUISubsystem(Application& app)
    : m_App(app) {}

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
}

void EditorUISubsystem::OnUpdate(FrameContext& /*context*/) {
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

    if (m_EditorUI && context.activeScene && context.selectedEntity) {
        m_EditorUI->BeginFrame();
        m_EditorUI->RenderPanels(
            *context.activeScene,
            *context.selectedEntity,
            context.gpuTimeMs,
            context.cpuTimeMs
        );
        m_EditorUI->EndFrame(
            context.commandBuffer,
            context.swapchainImageView,
            context.swapchainExtent
        );
    }
}

void EditorUISubsystem::OnShutdown() {
    m_EditorUI.reset();
}

} // namespace Astral
