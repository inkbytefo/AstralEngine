#pragma once

#include "../../Core/ISubsystem.h"
#include "../../Core/Logger.h"
#include "../../ECS/Components.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Forward declarations
namespace AstralEngine {
    class Engine;
    class ECSSubsystem;
    class RenderSubsystem;
    class AssetSubsystem;
    class UISubsystem;
    class Camera;
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

private:
    // Core systems
    Engine* m_owner = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
    RenderSubsystem* m_renderSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    UISubsystem* m_uiSubsystem = nullptr;

    // Editor state
    uint32_t m_selectedEntity = 0;
    std::unordered_set<uint32_t> m_selectedEntities;
    EditorMode m_editorMode = EditorMode::Select;

    // UI state
    bool m_showSceneHierarchy = true;
    bool m_showProperties = true;
    bool m_showViewport = true;
    bool m_showToolbar = true;

    // Scene management
    std::string m_currentScenePath;
    bool m_sceneModified = false;

    // Viewport state
    std::unique_ptr<Camera> m_editorCamera;
    bool m_viewportFocused = false;
    bool m_viewportHovered = false;

    // UI rendering methods
    void RenderMainMenuBar();
    void RenderSceneHierarchy();
    void RenderPropertiesPanel();
    void RenderViewportPanel();
    void RenderToolbar();

    // Helper methods
    void RenderEntityNode(uint32_t entity, bool isRoot = true);
    void RenderComponentProperties(uint32_t entity);
    void RenderTransformComponent(TransformComponent* transform);
    void RenderRenderComponent(RenderComponent* render);
    void RenderLightComponent(LightComponent* light);
    void RenderCameraComponent(CameraComponent* camera);

    void HandleViewportInput();
    void HandleEntitySelection();
    void UpdateEditorCamera(float deltaTime);

    // Entity hierarchy helpers
    std::vector<uint32_t> GetEntityChildren(uint32_t parent) const;
    std::string GetEntityDisplayName(uint32_t entity) const;
    bool IsEntityDescendant(uint32_t entity, uint32_t potentialAncestor) const;

    // File dialog helpers
    std::string OpenFileDialog(const std::string& title, const std::string& filter);
    std::string SaveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultExt);

    // Scene serialization
    void SerializeScene(const std::string& filename);
    void DeserializeScene(const std::string& filename);

    // Gizmo helpers
    void RenderGizmo();
    bool IsGizmoVisible() const;
    void UpdateGizmoTransform();

    // Confirmation dialogs
    bool ShowDeleteConfirmationDialog();
    bool ShowUnsavedChangesDialog();

    // Utility methods
    ImVec2 GetViewportSize() const;
    bool IsViewportFocused() const { return m_viewportFocused; }
    bool IsViewportHovered() const { return m_viewportHovered; }

    // Drag and drop helpers
    void HandleDragDrop();
    void HandleHierarchyDragDrop();

    // Undo/Redo system (basic implementation)
    struct EditorCommand {
        virtual ~EditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetDescription() const = 0;
    };

    void ExecuteCommand(std::unique_ptr<EditorCommand> command);
    void UndoLastCommand();
    void RedoNextCommand();

    std::vector<std::unique_ptr<EditorCommand>> m_commandHistory;
    std::vector<std::unique_ptr<EditorCommand>> m_redoStack;
    size_t m_maxUndoSteps = 50;
};

} // namespace AstralEngine