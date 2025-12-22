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

#include "EditorPanel.h"
#include "SceneHierarchyPanel.h"
#include "PropertiesPanel.h"
#include "ViewportPanel.h"
#include "MainToolbarPanel.h"
#include "ContentBrowserPanel.h"
#include "OutputLogPanel.h"

// Forward declarations
namespace AstralEngine {
    class Engine;
    class RenderSubsystem;
    class AssetSubsystem;
    class UISubsystem;
    class Camera;
    class IRHIDevice;
}

#ifdef ASTRAL_USE_IMGUI
    #include "../../Core/AstralImConfig.h"
    #include <imgui.h>
#endif

namespace AstralEngine {

/**
 * @brief Scene Editor Subsystem - UE5-Style Professional Editor
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

    // Selection management
    void SetSelectedEntity(uint32_t entity);
    uint32_t GetSelectedEntity() const { return m_selectedEntity; }

    // Scene management
    void NewScene();
    bool SaveScene(const std::string& filename);
    bool LoadScene(const std::string& filename);

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
    bool m_sceneModified = false;
    bool m_layoutInitialized = false;

    // Modular Panels
    std::vector<std::unique_ptr<EditorPanel>> m_panels;
    
    // Quick access
    ViewportPanel* m_viewportPanel = nullptr;
    OutputLogPanel* m_outputLogPanel = nullptr;
    PropertiesPanel* m_propertiesPanel = nullptr;

    // Resources
    void InitializeDefaultResources();
    void SetupViewportResources();
    void ResizeViewport(uint32_t width, uint32_t height);

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::shared_ptr<IRHIDescriptorSetLayout> m_globalDescriptorSetLayout;
    std::vector<std::shared_ptr<IRHIBuffer>> m_uniformBuffers;
    std::vector<std::shared_ptr<IRHIDescriptorSet>> m_globalDescriptorSets;
    std::shared_ptr<Mesh> m_defaultMesh;
    std::shared_ptr<Texture> m_defaultTexture;
    std::unique_ptr<Material> m_defaultMaterial;
    
    std::shared_ptr<IRHITexture> m_viewportTexture;
    std::shared_ptr<IRHITexture> m_viewportDepth;
    void* m_viewportDescriptorSet = nullptr;

    // UI Layout
    void RenderMainMenuBar();
    void ResetLayout(); // Reconfigures DockSpace to UE5 style
    
    // Commands
    void ExecuteCommand(uint32_t type);
};

} // namespace AstralEngine
