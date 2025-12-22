#pragma once

#include "EditorPanel.h"
#include <memory>
#include <cstdint>

namespace AstralEngine {

class Scene;

class PropertiesPanel : public EditorPanel {
public:
    PropertiesPanel();
    virtual ~PropertiesPanel() override = default;

    virtual void OnDraw() override;

    void SetSelectedEntity(uint32_t entityID) { m_selectedEntity = entityID; }
    void SetContext(std::shared_ptr<Scene> context) { m_context = context; }

private:
    void DrawComponents(uint32_t entityID);

    std::shared_ptr<Scene> m_context;
    uint32_t m_selectedEntity = 0xFFFFFFFF; // entt::null equivalent
};

} // namespace AstralEngine
