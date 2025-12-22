#pragma once

#include "EditorPanel.h"

namespace AstralEngine {

class MainToolbarPanel : public EditorPanel {
public:
    MainToolbarPanel();
    virtual ~MainToolbarPanel() override;

    virtual void OnDraw() override;

private:
    void DrawModeSelector();
    void DrawPlayControls();
    void DrawUtilityButtons();

    int m_currentMode = 0; // 0: Select, 1: Landscape, 2: Foliage...
};

} // namespace AstralEngine
