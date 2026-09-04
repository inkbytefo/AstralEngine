#pragma once

#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"

// Modular panel includes
#include "Astral/Editor/EditorTheme.hpp"
#include "Astral/Editor/EditorWorkspace.hpp"
#include "Astral/Editor/EditorMenuBar.hpp"
#include "Astral/Editor/EditorStatusBar.hpp"
#include "Astral/Editor/Panels/SceneHierarchy.hpp"
#include "Astral/Editor/Panels/Inspector.hpp"
#include "Astral/Editor/Panels/Statistics.hpp"
#include "Astral/Editor/Panels/ViewportPanel.hpp"
#include "Astral/Editor/Panels/ContentBrowser.hpp"

#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>

namespace Astral {

class EditorUI {
public:
    EditorUI(VulkanContext& context, GLFWwindow* window);
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    /// ImGui yeni kare baslatma
    void BeginFrame();

    /// Tum editor panellerini ciz (DockSpace + MenuBar + Panels + StatusBar)
    void RenderPanels(Scene& scene, Entity& selectedEntity, float gpuTimeMs, float cpuTimeMs);

    /// Dynamic Rendering ile ImGui cizim verilerini Swapchain uzerine basma
    void EndFrame(vk::CommandBuffer cmd, vk::ImageView swapchainImageView, vk::Extent2D extent);

    [[nodiscard]] bool WantsCaptureMouse() const;
    [[nodiscard]] bool WantsCaptureKeyboard() const;

    void SetRenderer(class SDFRenderer* renderer) noexcept { m_ViewportPanel.SetRenderer(renderer); }
    [[nodiscard]] ViewportPanel& GetViewportPanel() noexcept { return m_ViewportPanel; }

private:
    VulkanContext& m_Context;
    vk::UniqueDescriptorPool m_ImguiPool;

    // Vulkan 1.3 / 1.4 Dynamic Rendering fonksiyon isaretcileri
    PFN_vkCmdBeginRendering m_PfnCmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering m_PfnCmdEndRendering = nullptr;

    // Modular panels
    SceneHierarchy m_SceneHierarchy;
    Inspector m_Inspector;
    Statistics m_Statistics;
    ViewportPanel m_ViewportPanel;
    ContentBrowser m_ContentBrowser;

    // Editor state
    bool m_ResetLayout = false;
    bool m_ShowDemoWindow = false;

    void InitImGui(GLFWwindow* window);
    void ShutdownImGui();
    void SetupDockSpace(Scene& scene, Entity& selectedEntity);
};

} // namespace Astral
