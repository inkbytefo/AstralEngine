#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Subsystems/ECS/ECSSubsystem.h"
#include "../../Subsystems/Renderer/RenderSubsystem.h"
#include "../../Subsystems/Asset/AssetSubsystem.h"
#include "../../Subsystems/UI/UISubsystem.h"
#include "../../Subsystems/Renderer/Camera.h"
#include "../../Subsystems/Renderer/GraphicsDevice.h"
#include "../../Core/Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// ImGui includes
#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h"
    #include "ThirdParty/imgui.h"
#endif

namespace AstralEngine {

// Constructor
SceneEditorSubsystem::SceneEditorSubsystem() : m_selectedEntity(0), m_editorMode(EditorMode::Select) {
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
    m_ecsSubsystem = static_cast<ECSSubsystem*>(owner->GetSubsystem<ECSSubsystem>());
    m_renderSubsystem = static_cast<RenderSubsystem*>(owner->GetSubsystem<RenderSubsystem>());
    m_assetSubsystem = static_cast<AssetSubsystem*>(owner->GetSubsystem<AssetSubsystem>());
    m_uiSubsystem = static_cast<UISubsystem*>(owner->GetSubsystem<UISubsystem>());

    if (!m_ecsSubsystem) {
        Logger::Error("SceneEditorSubsystem", "Failed to get ECSSubsystem reference");
        return;
    }

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
    RenderMainMenuBar();

    // Setup docking layout
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    // Render individual panels
    RenderSceneHierarchy();
    RenderPropertiesPanel();
    RenderViewportPanel();
    RenderToolbar();

    // Handle viewport input
    HandleViewportInput();
    UpdateEditorCamera(deltaTime);
}

void SceneEditorSubsystem::OnShutdown() {
    Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem shutting down");

    // Check for unsaved changes
    if (m_sceneModified) {
        if (ShowUnsavedChangesDialog()) {
            // User chose to save, attempt to save current scene
            if (!m_currentScenePath.empty()) {
                SaveScene(m_currentScenePath);
            }
        }
    }
}

// Editor state management
void SceneEditorSubsystem::SetSelectedEntity(uint32_t entity) {
    if (m_ecsSubsystem->IsEntityValid(entity)) {
        m_selectedEntity = entity;
        m_selectedEntities.clear();
        m_selectedEntities.insert(entity);
        Logger::Debug("SceneEditorSubsystem", "Selected entity: {}", entity);
    }
}

void SceneEditorSubsystem::ClearSelection() {
    m_selectedEntity = 0;
    m_selectedEntities.clear();
    Logger::Debug("SceneEditorSubsystem", "Cleared entity selection");
}

bool SceneEditorSubsystem::IsEntitySelected(uint32_t entity) const {
    return m_selectedEntities.find(entity) != m_selectedEntities.end();
}

// Scene management
bool SceneEditorSubsystem::SaveScene(const std::string& filename) {
    try {
        SerializeScene(filename);
        m_currentScenePath = filename;
        m_sceneModified = false;
        Logger::Info("SceneEditorSubsystem", "Scene saved successfully: {}", filename);
        return true;
    } catch (const std::exception& e) {
        Logger::Error("SceneEditorSubsystem", "Failed to save scene: {}", e.what());
        return false;
    }
}

bool SceneEditorSubsystem::LoadScene(const std::string& filename) {
    try {
        // Check for unsaved changes
        if (m_sceneModified && !ShowUnsavedChangesDialog()) {
            return false; // User cancelled
        }

        DeserializeScene(filename);
        m_currentScenePath = filename;
        m_sceneModified = false;
        Logger::Info("SceneEditorSubsystem", "Scene loaded successfully: {}", filename);
        return true;
    } catch (const std::exception& e) {
        Logger::Error("SceneEditorSubsystem", "Failed to load scene: {}", e.what());
        return false;
    }
}

void SceneEditorSubsystem::NewScene() {
    // Check for unsaved changes
    if (m_sceneModified && !ShowUnsavedChangesDialog()) {
        return; // User cancelled
    }

    // Clear current scene
    ClearSelection();
    m_currentScenePath.clear();
    m_sceneModified = false;
    Logger::Info("SceneEditorSubsystem", "New scene created");
}

// Entity creation
uint32_t SceneEditorSubsystem::CreateEmptyEntity(const std::string& name) {
    uint32_t entity = m_ecsSubsystem->CreateEntity();

    // Add name component
    NameComponent* nameComp = m_ecsSubsystem->AddComponent<NameComponent>(entity);
    if (nameComp) {
        nameComp->name = name;
    }

    // Add transform component
    m_ecsSubsystem->AddComponent<TransformComponent>(entity);

    m_sceneModified = true;
    Logger::Info("SceneEditorSubsystem", "Created empty entity: {} (ID: {})", name, entity);
    return entity;
}

uint32_t SceneEditorSubsystem::CreatePrimitiveEntity(const std::string& name, const std::string& primitiveType) {
    uint32_t entity = CreateEmptyEntity(name);

    // Add render component with basic material
    RenderComponent* renderComp = m_ecsSubsystem->AddComponent<RenderComponent>(entity);
    if (renderComp) {
        // TODO: Load appropriate primitive model and material based on primitiveType
        // For now, use default handles
        renderComp->materialHandle = AssetHandle(); // Default material
        renderComp->modelHandle = AssetHandle();    // Default model
    }

    m_sceneModified = true;
    Logger::Info("SceneEditorSubsystem", "Created primitive entity: {} (Type: {})", name, primitiveType);
    return entity;
}

uint32_t SceneEditorSubsystem::CreateLightEntity(const std::string& name, LightComponent::LightType lightType) {
    uint32_t entity = CreateEmptyEntity(name);

    // Add light component
    LightComponent* lightComp = m_ecsSubsystem->AddComponent<LightComponent>(entity);
    if (lightComp) {
        lightComp->type = lightType;
        lightComp->color = glm::vec3(1.0f, 1.0f, 1.0f);
        lightComp->intensity = 1.0f;

        // Set default properties based on light type
        switch (lightType) {
            case LightComponent::LightType::Directional:
                lightComp->castsShadows = true;
                break;
            case LightComponent::LightType::Point:
                lightComp->range = 10.0f;
                break;
            case LightComponent::LightType::Spot:
                lightComp->range = 10.0f;
                lightComp->innerCone = 30.0f;
                lightComp->outerCone = 45.0f;
                break;
        }
    }

    m_sceneModified = true;
    Logger::Info("SceneEditorSubsystem", "Created light entity: {} (Type: {})", name,
                 lightType == LightComponent::LightType::Directional ? "Directional" :
                 lightType == LightComponent::LightType::Point ? "Point" : "Spot");
    return entity;
}

uint32_t SceneEditorSubsystem::CreateCameraEntity(const std::string& name, bool setAsMain) {
    uint32_t entity = CreateEmptyEntity(name);

    // Add camera component
    CameraComponent* cameraComp = m_ecsSubsystem->AddComponent<CameraComponent>(entity);
    if (cameraComp) {
        cameraComp->isMainCamera = setAsMain;
        cameraComp->fieldOfView = 60.0f;
        cameraComp->nearPlane = 0.1f;
        cameraComp->farPlane = 1000.0f;
    }

    m_sceneModified = true;
    Logger::Info("SceneEditorSubsystem", "Created camera entity: {} (Main: {})", name, setAsMain);
    return entity;
}

// Entity operations
void SceneEditorSubsystem::DeleteSelectedEntities() {
    if (m_selectedEntities.empty()) {
        return;
    }

    if (ShowDeleteConfirmationDialog()) {
        for (uint32_t entity : m_selectedEntities) {
            m_ecsSubsystem->DestroyEntity(entity);
        }
        ClearSelection();
        m_sceneModified = true;
        Logger::Info("SceneEditorSubsystem", "Deleted {} entities", m_selectedEntities.size());
    }
}

void SceneEditorSubsystem::DuplicateSelectedEntities() {
    if (m_selectedEntities.empty()) {
        return;
    }

    std::vector<uint32_t> newEntities;
    for (uint32_t entity : m_selectedEntities) {
        // TODO: Implement entity duplication logic
        // This would involve copying all components to a new entity
        Logger::Debug("SceneEditorSubsystem", "Duplicating entity: {}", entity);
    }

    m_sceneModified = true;
}

void SceneEditorSubsystem::ParentEntity(uint32_t child, uint32_t parent) {
    if (!m_ecsSubsystem->IsEntityValid(child) || !m_ecsSubsystem->IsEntityValid(parent)) {
        return;
    }

    // TODO: Implement parenting logic using HierarchyComponent
    m_sceneModified = true;
    Logger::Debug("SceneEditorSubsystem", "Parented entity {} to {}", child, parent);
}

// UI Rendering methods
void SceneEditorSubsystem::RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // File menu
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                NewScene();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                std::string filename = OpenFileDialog("Open Scene", "*.scene");
                if (!filename.empty()) {
                    LoadScene(filename);
                }
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, m_sceneModified)) {
                if (!m_currentScenePath.empty()) {
                    SaveScene(m_currentScenePath);
                } else {
                    std::string filename = SaveFileDialog("Save Scene", "*.scene", ".scene");
                    if (!filename.empty()) {
                        SaveScene(filename);
                    }
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                std::string filename = SaveFileDialog("Save Scene As", "*.scene", ".scene");
                if (!filename.empty()) {
                    SaveScene(filename);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                m_owner->RequestShutdown();
            }
            ImGui::EndMenu();
        }

        // Edit menu
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_commandHistory.empty())) {
                UndoLastCommand();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) {
                RedoNextCommand();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del", false, !m_selectedEntities.empty())) {
                DeleteSelectedEntities();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedEntities.empty())) {
                DuplicateSelectedEntities();
            }
            ImGui::EndMenu();
        }

        // Entity menu
        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Create Empty")) {
                CreateEmptyEntity("Empty Entity");
            }
            if (ImGui::BeginMenu("Create Primitive")) {
                if (ImGui::MenuItem("Cube")) {
                    CreatePrimitiveEntity("Cube", "cube");
                }
                if (ImGui::MenuItem("Sphere")) {
                    CreatePrimitiveEntity("Sphere", "sphere");
                }
                if (ImGui::MenuItem("Plane")) {
                    CreatePrimitiveEntity("Plane", "plane");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Create Light")) {
                if (ImGui::MenuItem("Directional Light")) {
                    CreateLightEntity("Directional Light", LightComponent::LightType::Directional);
                }
                if (ImGui::MenuItem("Point Light")) {
                    CreateLightEntity("Point Light", LightComponent::LightType::Point);
                }
                if (ImGui::MenuItem("Spot Light")) {
                    CreateLightEntity("Spot Light", LightComponent::LightType::Spot);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Create Camera")) {
                CreateCameraEntity("Camera", false);
            }
            ImGui::EndMenu();
        }

        // View menu
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showSceneHierarchy);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::MenuItem("Toolbar", nullptr, &m_showToolbar);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                // TODO: Reset docking layout
            }
            ImGui::EndMenu();
        }

        // Show scene modified indicator
        if (m_sceneModified) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "*");
        }

        ImGui::EndMainMenuBar();
    }
}

void SceneEditorSubsystem::RenderSceneHierarchy() {
    if (!m_showSceneHierarchy) return;

    ImGui::Begin("Scene Hierarchy", &m_showSceneHierarchy);

    // Toolbar
    if (ImGui::Button("Create Empty")) {
        CreateEmptyEntity("Empty Entity");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Light")) {
        CreateLightEntity("Point Light", LightComponent::LightType::Point);
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Camera")) {
        CreateCameraEntity("Camera", false);
    }

    ImGui::Separator();

    // Entity tree
    std::vector<uint32_t> rootEntities;
    for (uint32_t entity : m_ecsSubsystem->QueryEntities<NameComponent>()) {
        // TODO: Check if entity has no parent (is root)
        rootEntities.push_back(entity);
    }

    for (uint32_t entity : rootEntities) {
        RenderEntityNode(entity, true);
    }

    ImGui::End();
}

void SceneEditorSubsystem::RenderEntityNode(uint32_t entity, bool isRoot) {
    if (!m_ecsSubsystem->IsEntityValid(entity)) {
        return;
    }

    std::string displayName = GetEntityDisplayName(entity);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (m_selectedEntities.find(entity) != m_selectedEntities.end()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (GetEntityChildren(entity).empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", displayName.c_str());

    // Handle selection
    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl) {
            // Multi-select
            if (m_selectedEntities.find(entity) != m_selectedEntities.end()) {
                m_selectedEntities.erase(entity);
                if (m_selectedEntity == entity) {
                    m_selectedEntity = 0;
                }
            } else {
                m_selectedEntities.insert(entity);
                m_selectedEntity = entity;
            }
        } else {
            // Single select
            SetSelectedEntity(entity);
        }
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            DeleteSelectedEntities();
        }
        if (ImGui::MenuItem("Duplicate")) {
            DuplicateSelectedEntities();
        }
        if (ImGui::MenuItem("Rename")) {
            // TODO: Implement rename functionality
        }
        ImGui::EndPopup();
    }

    if (nodeOpen) {
        // Render children
        for (uint32_t child : GetEntityChildren(entity)) {
            RenderEntityNode(child, false);
        }
        ImGui::TreePop();
    }
}

void SceneEditorSubsystem::RenderPropertiesPanel() {
    if (!m_showProperties) return;

    ImGui::Begin("Properties", &m_showProperties);

    if (m_selectedEntities.empty()) {
        ImGui::Text("No entity selected");
    } else if (m_selectedEntities.size() == 1) {
        // Single entity selected
        uint32_t entity = *m_selectedEntities.begin();
        RenderComponentProperties(entity);
    } else {
        // Multiple entities selected
        ImGui::Text("Multiple entities selected (%zu)", m_selectedEntities.size());
        // TODO: Show multi-selection properties
    }

    ImGui::End();
}

void SceneEditorSubsystem::RenderComponentProperties(uint32_t entity) {
    if (!m_ecsSubsystem->IsEntityValid(entity)) {
        return;
    }

    // Transform Component
    if (TransformComponent* transform = m_ecsSubsystem->GetComponent<TransformComponent>(entity)) {
        RenderTransformComponent(transform);
    }

    // Render Component
    if (RenderComponent* render = m_ecsSubsystem->GetComponent<RenderComponent>(entity)) {
        RenderRenderComponent(render);
    }

    // Light Component
    if (LightComponent* light = m_ecsSubsystem->GetComponent<LightComponent>(entity)) {
        RenderLightComponent(light);
    }

    // Camera Component
    if (CameraComponent* camera = m_ecsSubsystem->GetComponent<CameraComponent>(entity)) {
        RenderCameraComponent(camera);
    }

    // Name Component
    if (NameComponent* name = m_ecsSubsystem->GetComponent<NameComponent>(entity)) {
        ImGui::Separator();
        ImGui::Text("Name Component");
        static char nameBuffer[256] = "";
        strcpy_s(nameBuffer, name->name.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            name->name = nameBuffer;
            m_sceneModified = true;
        }
    }

    // Tag Component
    if (TagComponent* tag = m_ecsSubsystem->GetComponent<TagComponent>(entity)) {
        ImGui::Text("Tag Component");
        static char tagBuffer[256] = "";
        strcpy_s(tagBuffer, tag->tag.c_str());
        if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer))) {
            tag->tag = tagBuffer;
            m_sceneModified = true;
        }
    }
}

void SceneEditorSubsystem::RenderTransformComponent(TransformComponent* transform) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Position
        glm::vec3 position = transform->position;
        if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
            transform->position = position;
            m_sceneModified = true;
        }

        // Rotation
        glm::vec3 rotation = glm::degrees(transform->rotation);
        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) {
            transform->rotation = glm::radians(rotation);
            m_sceneModified = true;
        }

        // Scale
        glm::vec3 scale = transform->scale;
        if (ImGui::DragFloat3("Scale", &scale.x, 0.1f, 0.1f, 100.0f)) {
            transform->scale = scale;
            m_sceneModified = true;
        }
    }
}

void SceneEditorSubsystem::RenderRenderComponent(RenderComponent* render) {
    if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Visibility
        bool visible = render->visible;
        if (ImGui::Checkbox("Visible", &visible)) {
            render->visible = visible;
            m_sceneModified = true;
        }

        // Render Layer
        int renderLayer = render->renderLayer;
        if (ImGui::InputInt("Render Layer", &renderLayer)) {
            render->renderLayer = renderLayer;
            m_sceneModified = true;
        }

        // Shadow settings
        bool castsShadows = render->castsShadows;
        if (ImGui::Checkbox("Cast Shadows", &castsShadows)) {
            render->castsShadows = castsShadows;
            m_sceneModified = true;
        }

        bool receivesShadows = render->receivesShadows;
        if (ImGui::Checkbox("Receive Shadows", &receivesShadows)) {
            render->receivesShadows = receivesShadows;
            m_sceneModified = true;
        }

        // Material and Model handles (read-only for now)
        ImGui::Text("Material Handle: %llu", render->materialHandle.GetHandle());
        ImGui::Text("Model Handle: %llu", render->modelHandle.GetHandle());
    }
}

void SceneEditorSubsystem::RenderLightComponent(LightComponent* light) {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Light type
        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(light->type);
        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
            light->type = static_cast<LightComponent::LightType>(currentType);
            m_sceneModified = true;
        }

        // Color
        glm::vec3 color = light->color;
        if (ImGui::ColorEdit3("Color", &color.x)) {
            light->color = color;
            m_sceneModified = true;
        }

        // Intensity
        float intensity = light->intensity;
        if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f)) {
            light->intensity = intensity;
            m_sceneModified = true;
        }

        // Type-specific properties
        if (light->type == LightComponent::LightType::Point || light->type == LightComponent::LightType::Spot) {
            float range = light->range;
            if (ImGui::DragFloat("Range", &range, 0.1f, 0.1f, 1000.0f)) {
                light->range = range;
                m_sceneModified = true;
            }
        }

        if (light->type == LightComponent::LightType::Spot) {
            float innerCone = light->innerCone;
            if (ImGui::DragFloat("Inner Cone", &innerCone, 1.0f, 0.0f, 180.0f)) {
                light->innerCone = innerCone;
                m_sceneModified = true;
            }

            float outerCone = light->outerCone;
            if (ImGui::DragFloat("Outer Cone", &outerCone, 1.0f, 0.0f, 180.0f)) {
                light->outerCone = outerCone;
                m_sceneModified = true;
            }
        }

        // Shadow casting
        bool castsShadows = light->castsShadows;
        if (ImGui::Checkbox("Cast Shadows", &castsShadows)) {
            light->castsShadows = castsShadows;
            m_sceneModified = true;
        }
    }
}

void SceneEditorSubsystem::RenderCameraComponent(CameraComponent* camera) {
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Main camera
        bool isMain = camera->isMainCamera;
        if (ImGui::Checkbox("Main Camera", &isMain)) {
            camera->isMainCamera = isMain;
            m_sceneModified = true;
        }

        // Projection type
        const char* projectionTypes[] = { "Perspective", "Orthographic" };
        int currentProjection = static_cast<int>(camera->projectionType);
        if (ImGui::Combo("Projection", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes))) {
            camera->projectionType = static_cast<CameraComponent::ProjectionType>(currentProjection);
            m_sceneModified = true;
        }

        // Perspective settings
        if (camera->projectionType == CameraComponent::ProjectionType::Perspective) {
            float fov = camera->fieldOfView;
            if (ImGui::DragFloat("Field of View", &fov, 1.0f, 1.0f, 179.0f)) {
                camera->fieldOfView = fov;
                m_sceneModified = true;
            }
        } else {
            // Orthographic settings
            float left = camera->orthoLeft;
            float right = camera->orthoRight;
            float bottom = camera->orthoBottom;
            float top = camera->orthoTop;

            if (ImGui::DragFloat("Left", &left, 0.1f)) {
                camera->orthoLeft = left;
                m_sceneModified = true;
            }
            if (ImGui::DragFloat("Right", &right, 0.1f)) {
                camera->orthoRight = right;
                m_sceneModified = true;
            }
            if (ImGui::DragFloat("Bottom", &bottom, 0.1f)) {
                camera->orthoBottom = bottom;
                m_sceneModified = true;
            }
            if (ImGui::DragFloat("Top", &top, 0.1f)) {
                camera->orthoTop = top;
                m_sceneModified = true;
            }
        }

        // Near/Far planes
        float nearPlane = camera->nearPlane;
        float farPlane = camera->farPlane;
        if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.001f, 10.0f)) {
            camera->nearPlane = nearPlane;
            m_sceneModified = true;
        }
        if (ImGui::DragFloat("Far Plane", &farPlane, 1.0f, nearPlane + 0.1f, 10000.0f)) {
            camera->farPlane = farPlane;
            m_sceneModified = true;
        }
    }
}

void SceneEditorSubsystem::RenderViewportPanel() {
    if (!m_showViewport) return;

    ImGui::Begin("Viewport", &m_showViewport);

    // Get viewport size
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

    // TODO: Render 3D scene here using RenderSubsystem
    // For now, just show a placeholder
    ImGui::Text("3D Viewport");
    ImGui::Text("Size: %.0f x %.0f", viewportSize.x, viewportSize.y);

    // Editor mode indicator
    const char* modeNames[] = { "Select", "Move", "Rotate", "Scale" };
    ImGui::Text("Mode: %s", modeNames[static_cast<int>(m_editorMode)]);

    // Camera controls
    if (ImGui::Button("Reset Camera")) {
        m_editorCamera->SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        m_editorCamera->SetLookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    }

    ImGui::End();
}

void SceneEditorSubsystem::RenderToolbar() {
    if (!m_showToolbar) return;

    ImGui::Begin("Toolbar", &m_showToolbar);

    // Editor modes
    if (ImGui::Button("Select")) {
        m_editorMode = EditorMode::Select;
    }
    ImGui::SameLine();
    if (ImGui::Button("Move")) {
        m_editorMode = EditorMode::Move;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate")) {
        m_editorMode = EditorMode::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::Button("Scale")) {
        m_editorMode = EditorMode::Scale;
    }

    ImGui::Separator();

    // Quick actions
    if (ImGui::Button("Create Cube")) {
        CreatePrimitiveEntity("Cube", "cube");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Light")) {
        CreateLightEntity("Point Light", LightComponent::LightType::Point);
    }

    ImGui::End();
}

// Helper methods
std::vector<uint32_t> SceneEditorSubsystem::GetEntityChildren(uint32_t parent) const {
    std::vector<uint32_t> children;
    // TODO: Implement using HierarchyComponent
    return children;
}

std::string SceneEditorSubsystem::GetEntityDisplayName(uint32_t entity) const {
    if (NameComponent* name = m_ecsSubsystem->GetComponent<NameComponent>(entity)) {
        return name->name;
    }
    return "Entity " + std::to_string(entity);
}

bool SceneEditorSubsystem::IsEntityDescendant(uint32_t entity, uint32_t potentialAncestor) const {
    // TODO: Implement hierarchy traversal
    return false;
}

// File dialog helpers
std::string SceneEditorSubsystem::OpenFileDialog(const std::string& title, const std::string& filter) {
    // TODO: Implement native file dialog
    // For now, return empty string
    return "";
}

std::string SceneEditorSubsystem::SaveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultExt) {
    // TODO: Implement native file dialog
    // For now, return empty string
    return "";
}

// Scene serialization
void SceneEditorSubsystem::SerializeScene(const std::string& filename) {
    // TODO: Implement JSON serialization of scene data
    // This would include all entities and their components
}

void SceneEditorSubsystem::DeserializeScene(const std::string& filename) {
    // TODO: Implement JSON deserialization of scene data
    // This would recreate all entities and their components
}

// Gizmo helpers
void SceneEditorSubsystem::RenderGizmo() {
    // TODO: Implement transform gizmos
}

bool SceneEditorSubsystem::IsGizmoVisible() const {
    return !m_selectedEntities.empty();
}

void SceneEditorSubsystem::UpdateGizmoTransform() {
    // TODO: Update gizmo position/rotation based on selected entity
}

// Confirmation dialogs
bool SceneEditorSubsystem::ShowDeleteConfirmationDialog() {
    // TODO: Implement confirmation dialog
    return true; // For now, always confirm
}

bool SceneEditorSubsystem::ShowUnsavedChangesDialog() {
    // TODO: Implement unsaved changes dialog
    return true; // For now, always save
}

// Utility methods
ImVec2 SceneEditorSubsystem::GetViewportSize() const {
    // TODO: Get actual viewport size from ImGui
    return ImVec2(800, 600);
}

// Drag and drop helpers
void SceneEditorSubsystem::HandleDragDrop() {
    // TODO: Implement drag and drop functionality
}

void SceneEditorSubsystem::HandleHierarchyDragDrop() {
    // TODO: Implement hierarchy drag and drop
}

// Input handling
void SceneEditorSubsystem::HandleViewportInput() {
    // TODO: Handle mouse/keyboard input in viewport
    // This would include camera controls and entity selection
}

void SceneEditorSubsystem::HandleEntitySelection() {
    // TODO: Handle entity selection in viewport
    // This would involve ray casting and entity picking
}

void SceneEditorSubsystem::UpdateEditorCamera(float deltaTime) {
    // TODO: Update editor camera based on input
    // This would include WASD movement, mouse look, etc.
}

// Command system
void SceneEditorSubsystem::ExecuteCommand(std::unique_ptr<EditorCommand> command) {
    command->Execute();
    m_commandHistory.push_back(std::move(command));

    // Limit undo history
    if (m_commandHistory.size() > m_maxUndoSteps) {
        m_commandHistory.erase(m_commandHistory.begin());
    }

    // Clear redo stack
    m_redoStack.clear();
    m_sceneModified = true;
}

void SceneEditorSubsystem::UndoLastCommand() {
    if (!m_commandHistory.empty()) {
        auto command = std::move(m_commandHistory.back());
        command->Undo();
        m_commandHistory.pop_back();
        m_redoStack.push_back(std::move(command));
    }
}

void SceneEditorSubsystem::RedoNextCommand() {
    if (!m_redoStack.empty()) {
        auto command = std::move(m_redoStack.back());
        command->Execute();
        m_redoStack.pop_back();
        m_commandHistory.push_back(std::move(command));
    }
}

} // namespace AstralEngine