#pragma once

#include "EditorPanel.h"
#include <memory>

namespace AstralEngine {

class Scene;

class SceneHierarchyPanel : public EditorPanel {
public:
    SceneHierarchyPanel(std::shared_ptr<Scene> context);
    virtual ~SceneHierarchyPanel() override = default;

    virtual void OnDraw() override;

    void SetContext(std::shared_ptr<Scene> context) { m_context = context; }

private:
    void DrawEntityNode(uint32_t entityID);

    std::shared_ptr<Scene> m_context;
};

} // namespace AstralEngine
