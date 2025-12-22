#pragma once

#include "EditorPanel.h"
#include "../Renderer/Core/Camera.h"
#include "../../Subsystems/Renderer/RHI/IRHIResource.h"
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace AstralEngine {

class Scene;
class RenderSubsystem;

class ViewportPanel : public EditorPanel {
public:
    ViewportPanel();
    virtual ~ViewportPanel() override;

    virtual void OnDraw() override;

    void SetContext(std::shared_ptr<Scene> scene) { m_scene = scene; }
    void SetRenderSubsystem(RenderSubsystem* render) { m_renderSubsystem = render; }
    
    void SetViewportTexture(std::shared_ptr<IRHITexture> texture, void* descriptorSet) { 
        m_viewportTexture = texture; 
        m_viewportDescriptorSet = descriptorSet;
    }

    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
    void SetResizeCallback(ResizeCallback callback) { m_onResize = callback; }

    Camera* GetCamera() { return m_camera.get(); }
    const glm::vec2& GetSize() const { return m_size; }
    bool IsFocused() const { return m_isFocused; }
    bool IsHovered() const { return m_isHovered; }
    void SetDraggingFile(bool dragging) { m_isDraggingFile = dragging; }
    bool IsPointOverViewport(float x, float y) const;

private:
    void DrawToolbar();
    void HandleInput();

    std::shared_ptr<Scene> m_scene;
    RenderSubsystem* m_renderSubsystem = nullptr;
    std::unique_ptr<Camera> m_camera;
    
    std::shared_ptr<IRHITexture> m_viewportTexture;
    void* m_viewportDescriptorSet = nullptr;

    glm::vec2 m_size{1.0f, 1.0f};
    bool m_isFocused = false;
    bool m_isHovered = false;

    // Viewport Settings
    enum class ViewMode { Lit, Wireframe, Unlit };
    ViewMode m_viewMode = ViewMode::Lit;
    float m_cameraSpeed = 1.0f;
    bool m_showGrid = true;
    bool m_showGizmos = true;
    bool m_isDraggingFile = false;
    ResizeCallback m_onResize;
    
    glm::vec2 m_windowPos{0.0f, 0.0f};
    glm::vec2 m_windowSize{0.0f, 0.0f};
};

} // namespace AstralEngine
