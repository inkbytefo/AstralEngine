#pragma once

#include "../../Core/ISubsystem.h"
#include "../../Core/Logger.h"
#include <memory>
#include <string>
#include <unordered_map>

// Integrate ImGui headers only when the engine is compiled with UI support.
#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h" // Our custom config must be included first
    #include <imgui.h>
    #ifdef ASTRAL_USE_SDL3
        #include <imgui_impl_sdl3.h>
    #endif
    #ifdef ASTRAL_USE_VULKAN
        #include <imgui_impl_vulkan.h>
    #endif
#endif

namespace AstralEngine {

class Engine;
class Window;
class GraphicsDevice;
class SceneEditorSubsystem;

/**
 * @brief Dear ImGui tabanlı UI alt sistemi
 *
 * SDL3 platform backend ve Vulkan renderer backend kullanır.
 * Debug tools, profilers ve editor UI için optimized.
 */
class UISubsystem : public ISubsystem {
public:
    UISubsystem();
    ~UISubsystem() override;

    // Non-copyable
    UISubsystem(const UISubsystem&) = delete;
    UISubsystem& operator=(const UISubsystem&) = delete;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "UISubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::UI; }

    // Frame management
    void BeginFrame();
    void EndFrame();
    void Render(VkCommandBuffer commandBuffer); // Vulkan render pass'te çağrılır

    // UI State
    bool IsCapturingMouse() const;
    bool IsCapturingKeyboard() const;
    bool HasFocus() const;

    // Debug UI
    void ShowDebugWindow(bool* open = nullptr);
    void ShowMetricsWindow(bool* open = nullptr);
    void ShowDemoWindow(bool* open = nullptr);

    // Font management
    bool LoadFont(const std::string& name, const std::string& path, float size = 16.0f);
    void SetDefaultFont(const std::string& name);
    void RebuildFontAtlas();

private:
    void InitializeImGui();
    void SetupImGuiStyle();
    void SetupVulkanBackend();
    void ShutdownImGui();
    void SetupStyle();

#ifdef ASTRAL_USE_IMGUI
    void ProcessSDLEvent(const void* event);
    void SetupFonts();
    void UpdateViewport();
#endif

private:
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    GraphicsDevice* m_graphicsDevice = nullptr;

    bool m_initialized = false;
    bool m_showDemo = false;
    bool m_showMetrics = false;
    bool m_showDebugWindow = false;

    // Font management
    std::unordered_map<std::string, ImFont*> m_fonts;
    std::string m_defaultFont = "default";

    // Vulkan resources
#ifdef ASTRAL_USE_VULKAN
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkCommandPool> m_commandPools;
#endif
};

} // namespace AstralEngine
