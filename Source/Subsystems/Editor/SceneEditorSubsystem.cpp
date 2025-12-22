#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Subsystems/Renderer/Core/RenderSubsystem.h"
#include "../../Subsystems/Asset/AssetSubsystem.h"
#include "../../Subsystems/UI/UISubsystem.h"
#include "../Renderer/Core/Camera.h"
#include "../Renderer/RHI/IRHIDevice.h"
#include "../../Core/Logger.h"
#include "../../Subsystems/Scene/Entity.h"

#include "../../Subsystems/Renderer/RHI/Vulkan/VulkanDevice.h"
#include "../../Subsystems/Renderer/RHI/Vulkan/VulkanResources.h"
#include "backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

// ImGui includes
#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h"
    #include <imgui.h>
    #include <imgui_internal.h> // For DockBuilder
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace AstralEngine {

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 viewPos;
};

// Constructor
SceneEditorSubsystem::SceneEditorSubsystem() : m_selectedEntity((uint32_t)entt::null), m_editorMode(EditorMode::Select) {
    // Initialize editor camera
    m_editorCamera = std::make_unique<Camera>();
    m_editorCamera->SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    m_editorCamera->SetLookAt(glm::vec3(0.0f, 0.0f, 0.0f));
}

// Destructor
SceneEditorSubsystem::~SceneEditorSubsystem() {
    // Cleanup handled by unique_ptr
}

// ISubsystem interface implementation
void SceneEditorSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;

    // Get references to other subsystems
    m_renderSubsystem = static_cast<RenderSubsystem*>(owner->GetSubsystem<RenderSubsystem>());
    m_assetSubsystem = static_cast<AssetSubsystem*>(owner->GetSubsystem<AssetSubsystem>());
    m_uiSubsystem = static_cast<UISubsystem*>(owner->GetSubsystem<UISubsystem>());

    // Initialize default scene
    m_activeScene = std::make_shared<Scene>();
    Logger::Info("SceneEditorSubsystem", "Initialized default empty scene.");

    if (!m_renderSubsystem) {
        Logger::Error("SceneEditorSubsystem", "Failed to get RenderSubsystem reference");
        return;
    }

    if (!m_assetSubsystem) {
        Logger::Error("SceneEditorSubsystem", "Failed to get AssetSubsystem reference");
        return;
    }

    if (!m_uiSubsystem) {
        Logger::Error("SceneEditorSubsystem", "Failed to get UISubsystem reference");
        return;
    }

    // Initialize Viewport Resources (1x1 initially)
    SetupViewportResources();

    // Register PRE-render callback (Offscreen Rendering to Viewport Texture)
    m_renderSubsystem->SetPreRenderCallback([this](IRHICommandList* cmdList) {
        this->RenderScene(cmdList);
    });

    // Clear main render callback to avoid drawing scene to backbuffer (UI only)
    m_renderSubsystem->SetRenderCallback(nullptr);

    // Register UI Draw callback
    m_uiSubsystem->RegisterDrawCallback([this]() {
        this->DrawUI();
    });

    // Register Logger Callback
    Logger::SetLogCallback([this](Logger::LogLevel level, const std::string& category, const std::string& message) {
        std::string fmtMsg = "[" + category + "] " + message;
        m_logBuffer.push_back({level, fmtMsg});
    });

    // Initialize Panels
    m_panels.push_back(std::make_unique<SceneHierarchyPanel>(m_activeScene));
    m_panels.push_back(std::make_unique<PropertiesPanel>());
    static_cast<PropertiesPanel*>(m_panels.back().get())->SetContext(m_activeScene);

    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem initialized successfully");
}

void SceneEditorSubsystem::OnUpdate(float deltaTime) {
    // Defer resource initialization
    if (!m_resourcesInitialized) {
        InitializeDefaultResources();
        m_resourcesInitialized = true;
    }

    // 4. Handle Input (Logic Only)
    HandleViewportInput();
    UpdateEditorCamera(deltaTime);
}

void SceneEditorSubsystem::DrawUI() {
    // 1. Render Main Menu Bar
    RenderMainMenuBar();

    // 2. Setup DockSpace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockSpaceOverViewport(viewport->ID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    // Initialize Layout (First run or reset)
    if (!m_layoutInitialized) {
        ResetLayout();
        m_layoutInitialized = true;
    }

    // 3. Render Panels
    for (auto& panel : m_panels) {
        panel->OnDraw();
    }

    RenderViewportPanel(); 
    RenderToolbar();
    RenderContentBrowser();
    RenderOutputLog();
}

void SceneEditorSubsystem::OnShutdown() {
    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem shutting down");

    // Clear resources before RenderSubsystem shuts down
    m_meshCache.clear();
    m_defaultMesh.reset();
    m_defaultMaterial.reset();
    m_defaultTexture.reset();
    m_globalDescriptorSets.clear();
    for (auto& buffer : m_uniformBuffers) {
        buffer.reset();
    }
    m_uniformBuffers.clear();

    // Reset active scene to clear entities and their components
    if (m_activeScene) {
        m_activeScene->Reg().clear();
        m_activeScene.reset();
    }

    // Unregister callbacks
    if (m_renderSubsystem) {
        m_renderSubsystem->SetRenderCallback(nullptr);
    }

    // Check for unsaved changes
    if (m_sceneModified) {
        Logger::Warning("SceneEditorSubsystem", "Shutdown with unsaved changes");
    }
}

// Editor state management
void SceneEditorSubsystem::SetSelectedEntity(uint32_t entityID) {
    if (m_activeScene->Reg().valid((entt::entity)entityID)) {
        m_selectedEntity = entityID;
        m_selectedEntities.clear();
        m_selectedEntities.insert(entityID);
        
        // Update Properties Panel
        for (auto& panel : m_panels) {
            if (panel->GetName() == "Details") {
                static_cast<PropertiesPanel*>(panel.get())->SetSelectedEntity(entityID);
            }
        }

        Logger::Debug("SceneEditorSubsystem", "Selected entity: {}", entityID);
    }
}

void SceneEditorSubsystem::ClearSelection() {
    m_selectedEntity = (uint32_t)entt::null;
    m_selectedEntities.clear();
    Logger::Debug("SceneEditorSubsystem", "Cleared entity selection");
}

bool SceneEditorSubsystem::IsEntitySelected(uint32_t entityID) const {
    return m_selectedEntities.find(entityID) != m_selectedEntities.end();
}

// Scene management
bool SceneEditorSubsystem::SaveScene(const std::string& filename) {
    // Placeholder for serialization logic
    m_currentScenePath = filename;
    m_sceneModified = false;
    Logger::Info("SceneEditorSubsystem", "Scene saved: {}", filename);
    return true;
}

bool SceneEditorSubsystem::LoadScene(const std::string& filename) {
    // Placeholder for deserialization logic
    m_currentScenePath = filename;
    m_sceneModified = false;
    Logger::Info("SceneEditorSubsystem", "Scene loaded: {}", filename);
    return true;
}

void SceneEditorSubsystem::NewScene() {
    m_activeScene = std::make_shared<Scene>();
    ClearSelection();
    m_currentScenePath.clear();
    m_sceneModified = false;
    Logger::Info("SceneEditorSubsystem", "New scene created");
}

// Entity creation
uint32_t SceneEditorSubsystem::CreateEmptyEntity(const std::string& name) {
    Entity entity = m_activeScene->CreateEntity(name);
    
    // Ensure it has a TransformComponent
    if (!entity.HasComponent<TransformComponent>()) {
        entity.AddComponent<TransformComponent>();
    }

    m_sceneModified = true;
    Logger::Info("SceneEditorSubsystem", "Created entity: {}", name);
    return (uint32_t)entity;
}

uint32_t SceneEditorSubsystem::CreatePrimitiveEntity(const std::string& name, const std::string& primitiveType) {
    uint32_t entityID = CreateEmptyEntity(name);
    Entity entity((entt::entity)entityID, m_activeScene.get());

    // Add RenderComponent
    entity.AddComponent<RenderComponent>();
    
    // TODO: Set actual model/material handles based on primitiveType
    (void)primitiveType; // Suppress unused parameter warning
    
    m_sceneModified = true;
    return entityID;
}

uint32_t SceneEditorSubsystem::CreateLightEntity(const std::string& name, LightComponent::LightType lightType) {
    uint32_t entityID = CreateEmptyEntity(name);
    Entity entity((entt::entity)entityID, m_activeScene.get());

    LightComponent& lightComp = entity.AddComponent<LightComponent>();
    lightComp.type = lightType;
    
    switch (lightType) {
        case LightComponent::LightType::Directional:
            lightComp.castsShadows = true;
            break;
        case LightComponent::LightType::Point:
            lightComp.range = 10.0f;
            break;
        case LightComponent::LightType::Spot:
            lightComp.range = 20.0f;
            lightComp.innerConeAngle = 30.0f;
            lightComp.outerConeAngle = 45.0f;
            break;
    }

    m_sceneModified = true;
    return entityID;
}

bool SceneEditorSubsystem::ImportModel(const std::string& path) {
    if (!m_assetSubsystem) return false;

    AssetManager* am = m_assetSubsystem->GetAssetManager();
    if (!am) return false;

    // Load Model
    AssetHandle modelHandle = am->Load<ModelData>(path);
    if (!modelHandle.IsValid()) {
        Logger::Error("SceneEditorSubsystem", "Failed to load model: {}", path);
        return false;
    }

    // Get default material (reuse existing handle if possible, or load default)
    if (!m_materialHandle.IsValid()) {
        m_materialHandle = am->Load<MaterialData>("Materials/Default.amat");
    }

    // Create Entity
    std::filesystem::path fsPath(path);
    std::string entityName = fsPath.stem().string();
    uint32_t entityID = CreateEmptyEntity(entityName);
    Entity entity((entt::entity)entityID, m_activeScene.get());

    // Add RenderComponent
    entity.AddComponent<RenderComponent>(m_materialHandle, modelHandle);

    Logger::Info("SceneEditorSubsystem", "Imported model entity: {}", entityName);
    return true;
}

uint32_t SceneEditorSubsystem::CreateCameraEntity(const std::string& name, bool setAsMain) {
    uint32_t entityID = CreateEmptyEntity(name);
    Entity entity((entt::entity)entityID, m_activeScene.get());

    CameraComponent& camComp = entity.AddComponent<CameraComponent>();
    camComp.isMainCamera = setAsMain;

    m_sceneModified = true;
    return entityID;
}

// Entity operations
void SceneEditorSubsystem::DeleteSelectedEntities() {
    if (m_selectedEntities.empty()) return;

    if (ShowDeleteConfirmationDialog()) {
        for (uint32_t entityID : m_selectedEntities) {
            Entity entityToRemove((entt::entity)entityID, m_activeScene.get());
            m_activeScene->DestroyEntity(entityToRemove);
        }
        ClearSelection();
        m_sceneModified = true;
    }
}

void SceneEditorSubsystem::DuplicateSelectedEntities() {
    // Placeholder for duplication logic
}

void SceneEditorSubsystem::ParentEntity(uint32_t childID, uint32_t parentID) {
    Entity child((entt::entity)childID, m_activeScene.get());
    Entity parent((entt::entity)parentID, m_activeScene.get());
    
    if (child && parent) {
        // Check for cycles before parenting
        if (!IsEntityDescendant(parentID, childID)) {
             // Add RelationshipComponent if missing
            if (!child.HasComponent<RelationshipComponent>()) child.AddComponent<RelationshipComponent>();
            if (!parent.HasComponent<RelationshipComponent>()) parent.AddComponent<RelationshipComponent>();

            // Remove from old parent
            RelationshipComponent& childRel = child.GetComponent<RelationshipComponent>();
            if (childRel.Parent != entt::null) {
                Entity oldParent(childRel.Parent, m_activeScene.get());
                if (oldParent.HasComponent<RelationshipComponent>()) {
                    auto& children = oldParent.GetComponent<RelationshipComponent>().Children;
                    children.erase(std::remove(children.begin(), children.end(), (entt::entity)childID), children.end());
                }
            }

            // Set new parent
            childRel.Parent = (entt::entity)parentID;
            parent.GetComponent<RelationshipComponent>().Children.push_back((entt::entity)childID);
            
            m_sceneModified = true;
        }
    }
}

// UI Rendering methods
void SceneEditorSubsystem::RenderMainMenuBar() {
    bool openImportPopup = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) NewScene();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene("scene.yaml"); // Placeholder
            ImGui::Separator();
            if (ImGui::MenuItem("Import 3D Model...")) {
                openImportPopup = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) m_owner->RequestShutdown();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete", "Del")) DeleteSelectedEntities();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showSceneHierarchy);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ImGui::MenuItem("Content Browser", nullptr, &m_showContentBrowser);
            ImGui::MenuItem("Output Log", nullptr, &m_showOutputLog);
            ImGui::MenuItem("Toolbar", nullptr, &m_showToolbar);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Handle Popup opening outside the menu bar scope
    if (openImportPopup) {
        ImGui::OpenPopup("ImportModelPopup");
    }

     // Import Model Popup
    if (ImGui::BeginPopupModal("ImportModelPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::string selectedFile = "";
        static std::string statusMsg = "";
        static std::vector<std::string> availableModels;
        
        // Refresh list if empty
        if (availableModels.empty()) {
             // Hardcoded common models + scan logic could go here
             // For now, let's list known files or scan
             availableModels.clear();
             try {
                 if (std::filesystem::exists("Assets/Models")) {
                     for (const auto& entry : std::filesystem::directory_iterator("Assets/Models")) {
                         if (entry.is_regular_file()) {
                             std::string ext = entry.path().extension().string();
                             if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") {
                                 // Store relative path: "Models/filename.ext"
                                 availableModels.push_back("Models/" + entry.path().filename().string());
                             }
                         }
                     }
                 }
             } catch (...) {}
        }

        ImGui::Text("Available Models:");
        if (ImGui::BeginListBox("##ModelList", ImVec2(300, 200))) {
            for (const auto& modelPath : availableModels) {
                bool isSelected = (selectedFile == modelPath);
                if (ImGui::Selectable(modelPath.c_str(), isSelected)) {
                    selectedFile = modelPath;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }
        
        if (ImGui::Button("Refresh List")) {
            availableModels.clear(); 
        }

        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedFile.c_str());

        if (!statusMsg.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", statusMsg.c_str());
        }

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            if (selectedFile.empty()) {
                statusMsg = "Please select a file.";
            } else {
                if (ImportModel(selectedFile)) {
                    statusMsg = "";
                    ImGui::CloseCurrentPopup();
                } else {
                    statusMsg = "Failed to load model!";
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            statusMsg = "";
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


void SceneEditorSubsystem::RenderViewportPanel() {
    if (!m_showViewport) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", &m_showViewport);
    
    m_viewportFocused = ImGui::IsWindowFocused();
    m_viewportHovered = ImGui::IsWindowHovered();
    
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    
    // Resize if needed
    if (viewportSize.x != m_viewportSize.x || viewportSize.y != m_viewportSize.y) {
        ResizeViewport((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
    }
    
    // Display Image
    if (m_viewportDescriptorSet) {
        ImGui::Image((ImTextureID)m_viewportDescriptorSet, viewportSize, ImVec2(0, 0), ImVec2(1, 1));
    } else {
        ImGui::Text("Loading Viewport...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void SceneEditorSubsystem::RenderToolbar() {
    if (!m_showToolbar) return;
    
    ImGui::Begin("Toolbar", &m_showToolbar, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    if (ImGui::Button("Select")) m_editorMode = EditorMode::Select;
    ImGui::SameLine();
    if (ImGui::Button("Move")) m_editorMode = EditorMode::Move;
    ImGui::SameLine();
    if (ImGui::Button("Rotate")) m_editorMode = EditorMode::Rotate;
    ImGui::SameLine();
    if (ImGui::Button("Scale")) m_editorMode = EditorMode::Scale;
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    
    if (ImGui::Button("Play")) {
        // TODO: Start PIE (Play In Editor)
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        // TODO: Stop PIE
    }
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        m_layoutInitialized = false;
    }

    ImGui::End();
}

void SceneEditorSubsystem::ResetLayout() {
    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    
    // Clear existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id); 
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    
    // Split the dockspace into regions
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, nullptr, &dock_main_id); // Small strip for Toolbar if needed, or put it in main

    // Dock windows
    ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
    ImGui::DockBuilderDockWindow("World Outliner", dock_id_left);
    ImGui::DockBuilderDockWindow("Details", dock_id_right);
    ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Output Log", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);

    ImGui::DockBuilderFinish(dockspace_id);
}

void SceneEditorSubsystem::HandleViewportInput() {
    if (m_viewportFocused && m_viewportHovered) {
        // Handle camera movement, selection clicks, etc.
    }
}

void SceneEditorSubsystem::UpdateEditorCamera(float deltaTime) {
    (void)deltaTime;
    if (m_viewportFocused) {
        // Update camera based on input
    }
}

// Helpers
std::vector<uint32_t> SceneEditorSubsystem::GetEntityChildren(uint32_t parentID) const {
    std::vector<uint32_t> children;
    if (m_activeScene->Reg().valid((entt::entity)parentID)) {
        Entity parent((entt::entity)parentID, m_activeScene.get());
        if (parent.HasComponent<RelationshipComponent>()) {
            for (auto child : parent.GetComponent<RelationshipComponent>().Children) {
                children.push_back((uint32_t)child);
            }
        }
    }
    return children;
}

std::string SceneEditorSubsystem::GetEntityDisplayName(uint32_t entityID) const {
    if (m_activeScene->Reg().valid((entt::entity)entityID)) {
        Entity entity((entt::entity)entityID, m_activeScene.get());
        if (entity.HasComponent<NameComponent>()) {
            return entity.GetComponent<NameComponent>().name;
        }
    }
    return "Entity";
}

bool SceneEditorSubsystem::IsEntityDescendant(uint32_t entityID, uint32_t potentialAncestorID) const {
    if (entityID == potentialAncestorID) return true;
    
    if (m_activeScene->Reg().valid((entt::entity)entityID)) {
        Entity entity((entt::entity)entityID, m_activeScene.get());
        if (entity.HasComponent<RelationshipComponent>()) {
            entt::entity parent = entity.GetComponent<RelationshipComponent>().Parent;
            if (parent != entt::null) {
                return IsEntityDescendant((uint32_t)parent, potentialAncestorID);
            }
        }
    }
    return false;
}

// Dialog stubs
std::string SceneEditorSubsystem::OpenFileDialog(const std::string& title, const std::string& filter) { 
    (void)title; (void)filter;
    return ""; 
}
std::string SceneEditorSubsystem::SaveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultExt) { 
    (void)title; (void)filter; (void)defaultExt;
    return ""; 
}
bool SceneEditorSubsystem::ShowDeleteConfirmationDialog() { return true; }
bool SceneEditorSubsystem::ShowUnsavedChangesDialog() { return true; }

// Dummy implementations for private helpers
void SceneEditorSubsystem::RenderTransformComponent(TransformComponent* transform) { (void)transform; }
void SceneEditorSubsystem::RenderRenderComponent(RenderComponent* render) { (void)render; }
void SceneEditorSubsystem::RenderLightComponent(LightComponent* light) { (void)light; }
void SceneEditorSubsystem::RenderCameraComponent(CameraComponent* camera) { (void)camera; }
void SceneEditorSubsystem::RenderGizmo() {}
bool SceneEditorSubsystem::IsGizmoVisible() const { return false; }
void SceneEditorSubsystem::UpdateGizmoTransform() {}
ImVec2 SceneEditorSubsystem::GetViewportSize() const { return ImVec2(0,0); }
void SceneEditorSubsystem::HandleDragDrop() {}
void SceneEditorSubsystem::HandleHierarchyDragDrop() {}
void SceneEditorSubsystem::ExecuteCommand(std::unique_ptr<EditorCommand> command) { (void)command; }
void SceneEditorSubsystem::UndoLastCommand() {}
void SceneEditorSubsystem::RedoNextCommand() {}
void SceneEditorSubsystem::SerializeScene(const std::string& filename) { (void)filename; }
void SceneEditorSubsystem::DeserializeScene(const std::string& filename) { (void)filename; }

void SceneEditorSubsystem::RenderScene(IRHICommandList* cmdList) {
    if (!m_viewportTexture || !m_viewportDepth) return;
    
    // Begin Rendering to Viewport Texture
    std::vector<IRHITexture*> colorAttachments = { m_viewportTexture.get() };
    RHIRect2D renderArea;
    renderArea.offset = { 0, 0 };
    renderArea.extent = { m_viewportTexture->GetWidth(), m_viewportTexture->GetHeight() };
    
    cmdList->BeginRendering(colorAttachments, m_viewportDepth.get(), renderArea);

    if (m_defaultMaterial && m_defaultMesh && !m_globalDescriptorSets.empty()) {
        IRHIDevice* device = m_renderSubsystem->GetDevice();
        uint32_t currentFrame = device->GetCurrentFrameIndex();
        
        if (currentFrame < m_globalDescriptorSets.size()) {
            // Bind Pipeline and Global Descriptor Set
            cmdList->BindPipeline(m_defaultMaterial->GetPipeline());
            cmdList->BindDescriptorSet(m_defaultMaterial->GetPipeline(), m_globalDescriptorSets[currentFrame].get(), 0);

            // Draw Entities
            auto view = m_activeScene->Reg().view<const TransformComponent, const RenderComponent>();
            for (auto entity : view) {
                const auto& transform = view.get<const TransformComponent>(entity);
                const auto& render = view.get<const RenderComponent>(entity);
                
                if (!render.visible) continue;

                // Get Mesh
                std::shared_ptr<Mesh> mesh = GetOrCreateMesh(render.modelHandle);
                if (!mesh) mesh = m_defaultMesh;
                if (!mesh) continue;

                UniformBufferObject ubo{};
                
                // Calculate View/Proj
                float aspect = (float)renderArea.extent.width / (float)renderArea.extent.height;
                ubo.proj = m_editorCamera->GetProjectionMatrix(aspect);
                ubo.viewPos = glm::vec4(m_editorCamera->GetPosition(), 1.0f);
                ubo.view = m_editorCamera->GetViewMatrix();
                
                ubo.model = glm::translate(glm::mat4(1.0f), transform.position);
                ubo.model = glm::rotate(ubo.model, transform.rotation.x, glm::vec3(1,0,0));
                ubo.model = glm::rotate(ubo.model, transform.rotation.y, glm::vec3(0,1,0));
                ubo.model = glm::rotate(ubo.model, transform.rotation.z, glm::vec3(0,0,1));
                ubo.model = glm::scale(ubo.model, transform.scale);
                
                // Write to buffer
                void* mappedData = m_uniformBuffers[currentFrame]->Map();
                if (mappedData) {
                    memcpy(mappedData, &ubo, sizeof(UniformBufferObject));
                    m_uniformBuffers[currentFrame]->Unmap();
                }
                
                // Draw
                mesh->Draw(cmdList);
            }
        }
    }
    
    cmdList->EndRendering();
}

void SceneEditorSubsystem::SetupViewportResources() {
    ResizeViewport(1, 1);
}

void SceneEditorSubsystem::ResizeViewport(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    
    m_viewportSize = ImVec2((float)width, (float)height);
    
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    if (!device) return;
    
    device->WaitIdle();
    
    // Create Texture
    // Note: We need ColorAttachment | Sampled usage
    m_viewportTexture = device->CreateTexture2D(width, height, RHIFormat::B8G8R8A8_UNORM, (RHITextureUsage)(RHITextureUsage::ColorAttachment | RHITextureUsage::Sampled));
    
    // Create Depth
    m_viewportDepth = device->CreateTexture2D(width, height, RHIFormat::D32_FLOAT, RHITextureUsage::DepthStencilAttachment);
    
    // Update ImGui Descriptor Set
    auto vkTexture = std::dynamic_pointer_cast<VulkanTexture>(m_viewportTexture);
    if (vkTexture) {
         if (m_viewportDescriptorSet) {
             ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportDescriptorSet); 
             m_viewportDescriptorSet = nullptr;
         }
         
         // Create Sampler
         RHISamplerDescriptor samplerDesc = {};
         auto sampler = device->CreateSampler(samplerDesc);
         auto vkSampler = std::dynamic_pointer_cast<VulkanSampler>(sampler);
         
         if (vkSampler) {
             m_viewportDescriptorSet = ImGui_ImplVulkan_AddTexture(vkSampler->GetVkSampler(), vkTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
         }
    }
}

void SceneEditorSubsystem::CreateGlobalLayout() {
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    std::vector<RHIDescriptorSetLayoutBinding> bindings;
    RHIDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = RHIShaderStage::Vertex | RHIShaderStage::Fragment;
    bindings.push_back(uboBinding);
    
    m_globalDescriptorSetLayout = device->CreateDescriptorSetLayout(bindings);
}

void SceneEditorSubsystem::CreateUBOs(IRHIDevice* device) {
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = device->CreateBuffer(
            sizeof(UniformBufferObject),
            RHIBufferUsage::Uniform,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );
    }
}

void SceneEditorSubsystem::CreateGlobalDescriptorSets() {
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_globalDescriptorSets[i] = device->AllocateDescriptorSet(m_globalDescriptorSetLayout.get());
        m_globalDescriptorSets[i]->UpdateUniformBuffer(0, m_uniformBuffers[i].get(), 0, sizeof(UniformBufferObject));
    }
}

void SceneEditorSubsystem::InitializeDefaultResources() {
    if (!m_renderSubsystem || !m_assetSubsystem) return;
    
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    
    CreateGlobalLayout();
    CreateUBOs(device);
    CreateGlobalDescriptorSets();
    
    // Start Async Load
    AssetManager* am = m_assetSubsystem->GetAssetManager();
    if (am) {
        m_modelHandle = am->Load<ModelData>("Models/Cube.obj");
        m_textureHandle = am->Load<TextureData>("Models/testobject/VAZ2101_Body_BaseColor.png");
        m_materialHandle = am->Load<MaterialData>("Materials/Default.amat");
    }
}

std::shared_ptr<Mesh> SceneEditorSubsystem::GetOrCreateMesh(const AssetHandle& handle) {
    if (!handle.IsValid()) return nullptr;
    
    // Check cache
    uint64_t assetID = handle.GetID();
    auto it = m_meshCache.find(assetID);
    if (it != m_meshCache.end()) {
        return it->second;
    }
    
    // Not in cache, create it
    if (!m_assetSubsystem || !m_renderSubsystem) return nullptr;
    
    AssetManager* am = m_assetSubsystem->GetAssetManager();
    auto modelData = am->GetAsset<ModelData>(handle);
    
    if (!modelData || !modelData->isValid) return nullptr;
    
    // Create Mesh
    IRHIDevice* device = m_renderSubsystem->GetDevice();
    auto mesh = std::make_shared<Mesh>(device, *modelData);
    
    // Store in cache
    m_meshCache[assetID] = mesh;
    
    // Update default mesh if not set
    if (!m_defaultMesh) m_defaultMesh = mesh;
    
    return mesh;
}

void SceneEditorSubsystem::RenderContentBrowser() {
    if (!m_showContentBrowser) return;

    ImGui::Begin("Content Browser", &m_showContentBrowser);

    static std::filesystem::path currentPath = "Assets";
    
    // Back button
    if (currentPath != "Assets") {
        if (ImGui::Button("<- Back")) {
            currentPath = currentPath.parent_path();
        }
        ImGui::Separator();
    }

    float padding = 16.0f;
    float thumbnailSize = 64.0f;
    float cellSize = thumbnailSize + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, 0, false);

    try {
        if (std::filesystem::exists(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                std::string filename = entry.path().filename().string();
                
                ImGui::PushID(filename.c_str());
                
                if (entry.is_directory()) {
                     if (ImGui::Button(("DIR\n" + filename).c_str(), ImVec2(thumbnailSize, thumbnailSize))) {
                         currentPath /= entry.path().filename();
                     }
                } else {
                     if (ImGui::Button(("FILE\n" + filename).c_str(), ImVec2(thumbnailSize, thumbnailSize))) {
                         // Selection logic or drag source
                     }
                     // Drag Source
                     if (ImGui::BeginDragDropSource()) {
                         std::string itemPath = entry.path().string();
                         ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.length() + 1);
                         ImGui::Text("%s", filename.c_str());
                         ImGui::EndDragDropSource();
                     }
                }
                
                ImGui::TextWrapped("%s", filename.c_str());
                ImGui::NextColumn();
                ImGui::PopID();
            }
        }
    } catch (...) {}

    ImGui::Columns(1);
    ImGui::End();
}

void SceneEditorSubsystem::RenderOutputLog() {
    if (!m_showOutputLog) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Output Log", &m_showOutputLog);

    // Toolbar
    if (ImGui::Button("Clear")) m_logBuffer.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScrollLog);

    ImGui::Separator();

    // Log Area
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& log : m_logBuffer) {
        ImVec4 color = ImVec4(1, 1, 1, 1);
        switch (log.level) {
            case Logger::LogLevel::Trace: color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
            case Logger::LogLevel::Debug: color = ImVec4(0.0f, 0.8f, 1.0f, 1.0f); break;
            case Logger::LogLevel::Info: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
            case Logger::LogLevel::Warning: color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); break;
            case Logger::LogLevel::Error: color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
            case Logger::LogLevel::Critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
            default: break;
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(log.message.c_str());
        ImGui::PopStyleColor();
    }

    if (m_autoScrollLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}


} // namespace AstralEngine
