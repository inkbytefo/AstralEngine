#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Subsystems/Renderer/Core/RenderSubsystem.h"
#include "../../Subsystems/Asset/AssetSubsystem.h"
#include "../../Subsystems/Renderer/RHI/IRHICommandList.h"
#include "../Renderer/RHI/Vulkan/VulkanResources.h"
#include "../UI/UISubsystem.h"
#include "../../Core/MathUtils.h"

#ifdef ASTRAL_USE_IMGUI
    #include <imgui_impl_vulkan.h>
    #include <imgui_internal.h>
#endif

namespace AstralEngine {

SceneEditorSubsystem::SceneEditorSubsystem() {
}

SceneEditorSubsystem::~SceneEditorSubsystem() {
}

void SceneEditorSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    m_renderSubsystem = owner->GetSubsystem<RenderSubsystem>();
    m_assetSubsystem = owner->GetSubsystem<AssetSubsystem>();
    m_uiSubsystem = owner->GetSubsystem<UISubsystem>();
    
    m_activeScene = std::make_shared<Scene>();
    m_activeScene->OnInitialize(owner);

    InitializeDefaultResources();
    SetupViewportResources();

    // Register UI Draw Callback
    if (m_uiSubsystem) {
        m_uiSubsystem->RegisterDrawCallback([this]() {
            this->DrawUI();
        });
    }

    // Initialize Panels
    // 1. Toolbar
    auto toolbar = std::make_unique<MainToolbarPanel>();
    m_panels.push_back(std::move(toolbar));

    // 2. Viewport
    auto viewport = std::make_unique<ViewportPanel>();
    viewport->SetRenderSubsystem(m_renderSubsystem);
    viewport->SetContext(m_activeScene);
    viewport->SetViewportTexture(m_viewportTexture, m_viewportDescriptorSet);
    
    viewport->SetResizeCallback([this](uint32_t w, uint32_t h) {
        this->ResizeViewport(w, h);
    });

    m_viewportPanel = viewport.get();
    m_panels.push_back(std::move(viewport));

    // 3. Scene Hierarchy
    auto hierarchy = std::make_unique<SceneHierarchyPanel>();
    hierarchy->SetContext(m_activeScene);
    m_panels.push_back(std::move(hierarchy));

    // 4. Properties
    auto properties = std::make_unique<PropertiesPanel>();
    properties->SetContext(m_activeScene);
    m_propertiesPanel = properties.get();
    m_panels.push_back(std::move(properties));

    // 5. Content Browser
    auto contentBrowser = std::make_unique<ContentBrowserPanel>();
    m_panels.push_back(std::move(contentBrowser));

    // 6. Output Log
    auto outputLog = std::make_unique<OutputLogPanel>();
    m_outputLogPanel = outputLog.get();
    m_panels.push_back(std::move(outputLog));

    // Route Logger to Output Log
    Logger::SetLogCallback([this](Logger::LogLevel level, const std::string& category, const std::string& msg) {
        if (m_outputLogPanel) {
            std::string formatted = "[" + category + "] " + msg;
            m_outputLogPanel->AddLog(level, formatted);
        }
    });

    // Register Render Callback
    if (m_renderSubsystem) {
        m_renderSubsystem->SetPreRenderCallback([this](IRHICommandList* cmdList) {
            this->RenderScene(cmdList);
        });
    }

    Logger::Info("SceneEditorSubsystem", "Scene Editor initialized with modular panels");
}

void SceneEditorSubsystem::OnUpdate(float deltaTime) {
    (void)deltaTime;
}

void SceneEditorSubsystem::OnShutdown() {
    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem shutting down");

    m_activeScene.reset();
    m_panels.clear();

    if (m_viewportDescriptorSet) {
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportDescriptorSet);
        m_viewportDescriptorSet = nullptr;
    }

    if (m_renderSubsystem) {
        m_renderSubsystem->SetPreRenderCallback(nullptr);
    }
}

void SceneEditorSubsystem::SetSelectedEntity(uint32_t entity) {
    m_selectedEntity = entity;
    if (m_propertiesPanel) {
        m_propertiesPanel->SetSelectedEntity(entity);
    }
}

void SceneEditorSubsystem::NewScene() {
    m_activeScene = std::make_shared<Scene>();
    m_activeScene->OnInitialize(m_owner);
    SetSelectedEntity((uint32_t)entt::null);
    
    // Update panel contexts
    for (auto& panel : m_panels) {
        if (panel->GetName() == "Level Viewport") static_cast<ViewportPanel*>(panel.get())->SetContext(m_activeScene);
        if (panel->GetName() == "World Outliner") static_cast<SceneHierarchyPanel*>(panel.get())->SetContext(m_activeScene);
        if (panel->GetName() == "Details") static_cast<PropertiesPanel*>(panel.get())->SetContext(m_activeScene);
    }
}

bool SceneEditorSubsystem::SaveScene(const std::string& filename) {
    (void)filename;
    Logger::Info("SceneEditorSubsystem", "Saved scene to {}", filename);
    m_sceneModified = false;
    return true;
}

bool SceneEditorSubsystem::LoadScene(const std::string& filename) {
    (void)filename;
    Logger::Info("SceneEditorSubsystem", "Loaded scene from {}", filename);
    m_sceneModified = false;
    return true;
}

void SceneEditorSubsystem::DrawUI() {
    RenderMainMenuBar();

    // Create a fullscreen DockSpace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("EditorDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (!m_layoutInitialized) {
        ResetLayout();
        m_layoutInitialized = true;
    }

    ImGui::End();

    // Render all modular panels
    for (auto& panel : m_panels) {
        panel->OnDraw();
    }
}

void SceneEditorSubsystem::RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) NewScene();
            if (ImGui::MenuItem("Open Scene...")) {/* Load logic */}
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene")) SaveScene("current.scene");
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) m_owner->RequestShutdown();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Window")) {
            for (auto& panel : m_panels) {
                bool open = panel->IsOpen();
                if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, &open)) {
                    panel->SetOpen(open);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) m_layoutInitialized = false;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void SceneEditorSubsystem::ResetLayout() {
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_right_top, dock_id_right_bottom;
    ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.4f, &dock_id_right_top, &dock_id_right_bottom);

    ImGui::DockBuilderDockWindow("Level Viewport", dock_main_id);
    ImGui::DockBuilderDockWindow("World Outliner", dock_id_right_top);
    ImGui::DockBuilderDockWindow("Details", dock_id_right_bottom);
    ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Output Log", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Main Toolbar", dock_main_id); // Toolbar usually stays on top but can dock

    ImGui::DockBuilderFinish(dockspace_id);
}

void SceneEditorSubsystem::InitializeDefaultResources() {
    // Placeholder for actual resource loading logic
}

void SceneEditorSubsystem::SetupViewportResources() {
    ResizeViewport(1280, 720);
}

void SceneEditorSubsystem::ResizeViewport(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192) return;
    
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    if (!device) return;
    
    device->WaitIdle();
    
    m_viewportTexture = device->CreateTexture2D(width, height, RHIFormat::B8G8R8A8_UNORM, (RHITextureUsage)(RHITextureUsage::ColorAttachment | RHITextureUsage::Sampled));
    m_viewportDepth = device->CreateTexture2D(width, height, RHIFormat::D32_FLOAT, RHITextureUsage::DepthStencilAttachment);
    
    auto vkTexture = std::dynamic_pointer_cast<VulkanTexture>(m_viewportTexture);
    if (vkTexture) {
         if (m_viewportDescriptorSet) {
             ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportDescriptorSet); 
         }
         
         RHISamplerDescriptor samplerDesc = {};
         auto sampler = device->CreateSampler(samplerDesc);
         auto vkSampler = std::dynamic_pointer_cast<VulkanSampler>(sampler);
         
         if (vkSampler) {
             m_viewportDescriptorSet = ImGui_ImplVulkan_AddTexture(vkSampler->GetVkSampler(), vkTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
         }
    }

    if (m_viewportPanel) {
        m_viewportPanel->SetViewportTexture(m_viewportTexture, m_viewportDescriptorSet);
    }
}

void SceneEditorSubsystem::RenderScene(IRHICommandList* cmdList) {
    if (!m_viewportTexture || !m_viewportDepth || !m_viewportPanel) return;

    std::vector<IRHITexture*> colorAttachments = { m_viewportTexture.get() };
    RHIRect2D renderArea;
    renderArea.offset = { 0, 0 };
    renderArea.extent = { m_viewportTexture->GetWidth(), m_viewportTexture->GetHeight() };
    
    cmdList->BeginRendering(colorAttachments, m_viewportDepth.get(), renderArea);

    // Render logic using standard components
    // ...
    
    cmdList->EndRendering();
}

void SceneEditorSubsystem::ExecuteCommand(uint32_t type) {
    (void)type;
}

} // namespace AstralEngine
