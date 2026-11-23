#pragma once
// Thin EditorPanel interface and DI-owned PanelRegistry (ADR-0037).

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

struct EditorPanelContext;

class EditorPanel {
public:
    virtual ~EditorPanel() = default;
    EditorPanel(const EditorPanel&) = delete;
    EditorPanel& operator=(const EditorPanel&) = delete;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view title() const noexcept = 0;
    [[nodiscard]] virtual bool is_visible(const EditorPanelContext& ec) const noexcept = 0;
    virtual void draw(EditorPanelContext& ec) = 0;

protected:
    EditorPanel() = default;
};

class PanelRegistry {
public:
    void add(std::unique_ptr<EditorPanel> panel) {
        if (panel) panels_.push_back(std::move(panel));
    }

    void draw_visible(EditorPanelContext& ec) {
        for (const auto& p : panels_) {
            if (p && p->is_visible(ec)) p->draw(ec);
        }
    }

    [[nodiscard]] const std::vector<std::unique_ptr<EditorPanel>>& panels() const noexcept { return panels_; }

private:
    std::vector<std::unique_ptr<EditorPanel>> panels_;
};

} // namespace astral::editor
