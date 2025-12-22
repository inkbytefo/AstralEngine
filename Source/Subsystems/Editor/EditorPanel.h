#pragma once

#include <string>
#include <imgui.h>

namespace AstralEngine {

/**
 * @brief Base class for editor panels to standardize UI logic.
 */
class EditorPanel {
public:
    EditorPanel(const std::string& name) : m_name(name) {}
    virtual ~EditorPanel() = default;

    virtual void OnDraw() = 0;
    
    const std::string& GetName() const { return m_name; }
    bool& IsOpen() { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

protected:
    std::string m_name;
    bool m_isOpen = true;
};

} // namespace AstralEngine
