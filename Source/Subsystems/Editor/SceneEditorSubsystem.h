#pragma once

#include "../../Core/ISubsystem.h"
#include "../../Core/Logger.h"
#include "../../ECS/Components.h"
#include "../../Subsystems/Scene/Scene.h"
#include "../Renderer/Core/Mesh.h"
#include "../Renderer/Core/Material.h"
#include "../Renderer/Core/Texture.h"
#include "../../Subsystems/Asset/AssetHandle.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Forward declarations
namespace AstralEngine {
    class Engine;
    class RenderSubsystem;
    class AssetSubsystem;
    class UISubsystem;
    class Camera;
    class IRHIDevice;
}

// ImGui forward declarations
#ifdef ASTRAL_USE_IMGUI
struct ImVec2;
#endif

#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h"
    #include <imgui.h>
#endif

namespace AstralEngine {

/**
 * @brief Scene Editor Subsystem - Comprehensive scene editing functionality
 *
 * Provides a complete scene editor interface with entity management,
 * property editing, viewport controls, and scene persistence.
 * Integrates with existing ECS, Render, and Asset subsystems.
 */
class SceneEditorSubsystem : public ISubsystem {
public:
    SceneEditorSubsystem();
    ~SceneEditorSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "SceneEditorSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::UI; }

    // Editor state management
    void SetSelectedEntity(uint32_t entity);
    uint32_t GetSelectedEntity() const { return m_selectedEntity; }
    void ClearSelection();

    bool IsEntitySelected(uint32_t entity) const;
    const std::unordered_set<uint32_t>& GetSelectedEntities() const { return m_selectedEntities; }

    // Editor modes
    enum class EditorMode {
        Select,
        Move,
        Rotate,
        Scale
    };

    void SetEditorMode(EditorMode mode) { m_editorMode = mode; }
    EditorMode GetEditorMode() const { return m_editorMode; }

    // Scene management
    bool SaveScene(const std::string& filename);
    bool LoadScene(const std::string& filename);
    void NewScene();

    // Entity creation helpers
    uint32_t CreateEmptyEntity(const std::string& name = "Empty Entity");
    uint32_t CreatePrimitiveEntity(const std::string& name, const std::string& primitiveType);
    uint32_t CreateLightEntity(const std::string& name, LightComponent::LightType lightType);
    uint32_t CreateCameraEntity(const std::string& name, bool setAsMain = false);

    // Entity operations
    void DeleteSelectedEntities();
    void DuplicateSelectedEntities();
    void ParentEntity(uint32_t child, uint32_t parent);

    // UI Draw (Called by UISubsystem)
    void DrawUI();

    // Render callback
    void RenderScene(class IRHICommandList* cmdList);

private:
    // Core systems
    Engine* m_owner = nullptr;
    std::shared_ptr<Scene> m_activeScene;
    RenderSubsystem* m_renderSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    UISubsystem* m_uiSubsystem = nullptr;

    // Editor State
    uint32_t m_selectedEntity = (uint32_t)entt::null;
    std::unordered_set<uint32_t> m_selectedEntities;
    EditorMode m_editorMode = EditorMode::Select;
    bool m_sceneModified = false;
    std::string m_currentScenePath;

    // UI State
    bool m_showViewport = true;
    bool m_showSceneHierarchy = true;
    bool m_showProperties = true;
    bool m_showToolbar = true;

    // Viewport State
    std::unique_ptr<Camera> m_editorCamera;
    ImVec2 m_viewportSize;
    bool m_viewportFocused = false;
    bool m_viewportHovered = false;

    // Render Resources
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::shared_ptr<IRHIDescriptorSetLayout> m_globalDescriptorSetLayout;
    std::vector<std::shared_ptr<IRHIBuffer>> m_uniformBuffers;
    std::vector<std::shared_ptr<IRHIDescriptorSet>> m_globalDescriptorSets;
    std::unique_ptr<Mesh> m_defaultMesh;
    std::shared_ptr<Texture> m_defaultTexture;
    std::unique_ptr<Material> m_defaultMaterial;

    // Asset Handles (for Async Loading)
    AssetHandle m_modelHandle;
    AssetHandle m_textureHandle;
    AssetHandle m_materialHandle;
    bool m_textureCreated = false;
    bool m_resourcesInitialized = false;

    // Internal Helpers
    void InitializeDefaultResources();
    void CreateGlobalLayout();
    void CreateUBOs(IRHIDevice* device);
    void CreateGlobalDescriptorSets();
    
    // UI Renderers
    void RenderMainMenuBar();
    void RenderViewportPanel();
    void RenderSceneHierarchy();
    void RenderPropertiesPanel();
    void RenderToolbar();
    void RenderEntityNode(uint32_t entityID, bool isRoot);
    void RenderComponentProperties(uint32_t entityID);

    // Input & Logic
    void HandleViewportInput();
    void UpdateEditorCamera(float deltaTime);

    // Hierarchy Helpers
    std::vector<uint32_t> GetEntityChildren(uint32_t parentID) const;
    std::string GetEntityDisplayName(uint32_t entityID) const;
    bool IsEntityDescendant(uint32_t entityID, uint32_t potentialAncestorID) const;

    // Dialogs
    bool ShowDeleteConfirmationDialog();
    bool ShowUnsavedChangesDialog();
    std::string OpenFileDialog(const std::string& title, const std::string& filter);
    std::string SaveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultExt);

    // Component Renderers (Stubs for now)
    void RenderTransformComponent(TransformComponent* transform);
    void RenderRenderComponent(RenderComponent* render);
    void RenderLightComponent(LightComponent* light);
    void RenderCameraComponent(CameraComponent* camera);
    
    // Gizmo (Stub)
    void RenderGizmo();
    bool IsGizmoVisible() const;
    void UpdateGizmoTransform();
    ImVec2 GetViewportSize() const;
    void HandleDragDrop();
    void HandleHierarchyDragDrop();

    // Commands (Stub)
    struct EditorCommand {
        virtual ~EditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetDescription() const = 0;
    };
    
    void ExecuteCommand(std::unique_ptr<EditorCommand> command);
    void UndoLastCommand();
    void RedoNextCommand();
    void SerializeScene(const std::string& filename);
    void DeserializeScene(const std::string& filename);

    std::vector<std::unique_ptr<EditorCommand>> m_commandHistory;
    std::vector<std::unique_ptr<EditorCommand>> m_redoStack;
    size_t m_maxUndoSteps = 50;
    
    int m_gizmoOperation = -1;
};

} // namespace AstralEngine
