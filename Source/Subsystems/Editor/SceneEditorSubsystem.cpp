#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Subsystems/Renderer/Core/RenderSubsystem.h"
#include "../../Subsystems/Asset/AssetSubsystem.h"
#include "../../Subsystems/UI/UISubsystem.h"
#include "../../Subsystems/Renderer/Camera.h"
#include "../../Subsystems/Renderer/GraphicsDevice.h"
#include "../../Core/Logger.h"
#include "../../Subsystems/Scene/Entity.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// ImGui includes
#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h"
    #include <imgui.h>
    #include <imgui_internal.h> // For DockBuilder
#endif

namespace AstralEngine {

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

    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem initialized successfully");
}

void SceneEditorSubsystem::OnUpdate(float deltaTime) {
    // Main editor update logic
    
    // 1. Render Main Menu Bar
    RenderMainMenuBar();

    // 2. Setup DockSpace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockSpaceOverViewport(viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    // 3. Render Panels
    RenderViewportPanel();
    RenderSceneHierarchy();
    RenderPropertiesPanel();
    RenderToolbar();

    // 4. Handle Input
    HandleViewportInput();
    UpdateEditorCamera(deltaTime);
}

void SceneEditorSubsystem::OnShutdown() {
    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem shutting down");

    // Check for unsaved changes
    if (m_sceneModified) {
        // In a real app, we might block shutdown here or force save
        // For now, just log it
        Logger::Warning("SceneEditorSubsystem", "Shutdown with unsaved changes");
    }
}

// Editor state management
void SceneEditorSubsystem::SetSelectedEntity(uint32_t entityID) {
    if (m_activeScene->Reg().valid((entt::entity)entityID)) {
        m_selectedEntity = entityID;
        m_selectedEntities.clear();
        m_selectedEntities.insert(entityID);
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
            m_activeScene->DestroyEntity((entt::entity)entityID);
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
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) NewScene();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene("scene.yaml"); // Placeholder
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
            ImGui::MenuItem("Toolbar", nullptr, &m_showToolbar);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void SceneEditorSubsystem::RenderSceneHierarchy() {
    if (!m_showSceneHierarchy) return;

    ImGui::Begin("World Outliner", &m_showSceneHierarchy);

    // List all entities that do not have a parent (root entities)
    // Or if no RelationshipComponent, list all entities
    
    m_activeScene->Reg().each([&](auto entityID) {
        Entity entity(entityID, m_activeScene.get());
        
        bool isRoot = true;
        if (entity.HasComponent<RelationshipComponent>()) {
            if (entity.GetComponent<RelationshipComponent>().Parent != entt::null) {
                isRoot = false;
            }
        }
        
        if (isRoot) {
            RenderEntityNode((uint32_t)entityID, true);
        }
    });
    
    // Right-click on blank space to create entity
    if (ImGui::BeginPopupContextWindow(0, 1, false)) {
        if (ImGui::MenuItem("Create Empty Entity")) CreateEmptyEntity("Empty Entity");
        ImGui::EndPopup();
    }

    ImGui::End();
}

void SceneEditorSubsystem::RenderEntityNode(uint32_t entityID, bool isRoot) {
    Entity entity((entt::entity)entityID, m_activeScene.get());
    if (!entity) return;

    std::string name = "Entity";
    if (entity.HasComponent<NameComponent>()) {
        name = entity.GetComponent<NameComponent>().name;
    } else if (entity.HasComponent<TagComponent>()) {
        name = entity.GetComponent<TagComponent>().tag;
    }
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    if (m_selectedEntity == entityID) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    bool hasChildren = false;
    if (entity.HasComponent<RelationshipComponent>()) {
        if (!entity.GetComponent<RelationshipComponent>().Children.empty()) {
            hasChildren = true;
        }
    }
    
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entityID, flags, "%s", name.c_str());
    
    if (ImGui::IsItemClicked()) {
        SetSelectedEntity(entityID);
    }
    
    if (opened) {
        if (hasChildren) {
            for (auto childID : entity.GetComponent<RelationshipComponent>().Children) {
                RenderEntityNode((uint32_t)childID, false);
            }
        }
        ImGui::TreePop();
    }
}

void SceneEditorSubsystem::RenderPropertiesPanel() {
    if (!m_showProperties) return;

    ImGui::Begin("Details", &m_showProperties);

    if (m_selectedEntity != (uint32_t)entt::null) {
        RenderComponentProperties(m_selectedEntity);
    } else {
        ImGui::Text("Select an entity to view properties.");
    }

    ImGui::End();
}

void SceneEditorSubsystem::RenderComponentProperties(uint32_t entityID) {
    Entity entity((entt::entity)entityID, m_activeScene.get());
    
    // Name Component
    if (entity.HasComponent<NameComponent>()) {
        auto& name = entity.GetComponent<NameComponent>().name;
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strncpy_s(buffer, name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
            name = std::string(buffer);
            m_sceneModified = true;
        }
    }
    
    // Transform Component
    if (entity.HasComponent<TransformComponent>()) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tc = entity.GetComponent<TransformComponent>();
            
            bool changed = false;
            changed |= ImGui::DragFloat3("Position", &tc.position.x, 0.1f);
            
            glm::vec3 rotationDegrees = glm::degrees(tc.rotation);
            if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.1f)) {
                tc.rotation = glm::radians(rotationDegrees);
                changed = true;
            }
            
            changed |= ImGui::DragFloat3("Scale", &tc.scale.x, 0.01f);
            
            if (changed) m_sceneModified = true;
        }
    }

    // Camera Component
    if (entity.HasComponent<CameraComponent>()) {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cc = entity.GetComponent<CameraComponent>();
            
            bool changed = false;
            changed |= ImGui::Checkbox("Main Camera", &cc.isMainCamera);
            
            const char* projectionTypes[] = { "Perspective", "Orthographic" };
            int currentType = (int)cc.projectionType;
            if (ImGui::Combo("Projection", &currentType, projectionTypes, IM_ARRAYSIZE(projectionTypes))) {
                cc.projectionType = (CameraComponent::ProjectionType)currentType;
                changed = true;
            }
            
            if (cc.projectionType == CameraComponent::ProjectionType::Perspective) {
                changed |= ImGui::DragFloat("FOV", &cc.fieldOfView, 0.1f, 1.0f, 179.0f);
                changed |= ImGui::DragFloat("Near Plane", &cc.nearPlane, 0.01f);
                changed |= ImGui::DragFloat("Far Plane", &cc.farPlane, 1.0f);
            } else {
                changed |= ImGui::DragFloat("Size", &cc.orthoTop, 0.1f);
                // Simplified ortho size for now
            }
            
            if (changed) m_sceneModified = true;
        }
    }
    
    // Light Component
    if (entity.HasComponent<LightComponent>()) {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& lc = entity.GetComponent<LightComponent>();
            
            bool changed = false;
            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = (int)lc.type;
            if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
                lc.type = (LightComponent::LightType)currentType;
                changed = true;
            }
            
            changed |= ImGui::ColorEdit3("Color", &lc.color.x);
            changed |= ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f);
            
            if (lc.type != LightComponent::LightType::Directional) {
                changed |= ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f);
            }
            
            if (lc.type == LightComponent::LightType::Spot) {
                changed |= ImGui::DragFloat("Inner Cone", &lc.innerConeAngle, 0.1f, 0.0f, 89.0f);
                changed |= ImGui::DragFloat("Outer Cone", &lc.outerConeAngle, 0.1f, 0.0f, 90.0f);
            }
            
            changed |= ImGui::Checkbox("Casts Shadows", &lc.castsShadows);
            
            if (changed) m_sceneModified = true;
        }
    }

    // Add Component Button
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
            entity.AddComponent<CameraComponent>();
            m_sceneModified = true;
            ImGui::CloseCurrentPopup();
        }
        if (!entity.HasComponent<LightComponent>() && ImGui::MenuItem("Light")) {
            entity.AddComponent<LightComponent>();
            m_sceneModified = true;
            ImGui::CloseCurrentPopup();
        }
        // Add more components here
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
    
    // Placeholder for actual render texture
    ImGui::Text("3D Viewport");
    ImGui::Text("Size: %.0f x %.0f", viewportSize.x, viewportSize.y);
    
    // In the future:
    // ImGui::Image((void*)(intptr_t)m_renderSubsystem->GetFinalImageID(), viewportSize, ImVec2(0, 1), ImVec2(1, 0));

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

    ImGui::End();
}

void SceneEditorSubsystem::HandleViewportInput() {
    if (m_viewportFocused && m_viewportHovered) {
        // Handle camera movement, selection clicks, etc.
    }
}

void SceneEditorSubsystem::UpdateEditorCamera(float deltaTime) {
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
std::string SceneEditorSubsystem::OpenFileDialog(const std::string& title, const std::string& filter) { return ""; }
std::string SceneEditorSubsystem::SaveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultExt) { return ""; }
bool SceneEditorSubsystem::ShowDeleteConfirmationDialog() { return true; }
bool SceneEditorSubsystem::ShowUnsavedChangesDialog() { return true; }

// Dummy implementations for private helpers
void SceneEditorSubsystem::RenderTransformComponent(TransformComponent* transform) {}
void SceneEditorSubsystem::RenderRenderComponent(RenderComponent* render) {}
void SceneEditorSubsystem::RenderLightComponent(LightComponent* light) {}
void SceneEditorSubsystem::RenderCameraComponent(CameraComponent* camera) {}
void SceneEditorSubsystem::RenderGizmo() {}
bool SceneEditorSubsystem::IsGizmoVisible() const { return false; }
void SceneEditorSubsystem::UpdateGizmoTransform() {}
ImVec2 SceneEditorSubsystem::GetViewportSize() const { return ImVec2(0,0); }
void SceneEditorSubsystem::HandleDragDrop() {}
void SceneEditorSubsystem::HandleHierarchyDragDrop() {}
void SceneEditorSubsystem::ExecuteCommand(std::unique_ptr<EditorCommand> command) {}
void SceneEditorSubsystem::UndoLastCommand() {}
void SceneEditorSubsystem::RedoNextCommand() {}
void SceneEditorSubsystem::SerializeScene(const std::string& filename) {}
void SceneEditorSubsystem::DeserializeScene(const std::string& filename) {}

} // namespace AstralEngine
