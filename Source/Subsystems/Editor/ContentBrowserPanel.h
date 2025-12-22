#pragma once

#include "EditorPanel.h"
#include <string>
#include <vector>
#include <filesystem>

namespace AstralEngine {

class ContentBrowserPanel : public EditorPanel {
public:
    ContentBrowserPanel();
    virtual ~ContentBrowserPanel() override;

    virtual void OnDraw() override;

    void SetDrawerMode(bool isDrawer) { m_isDrawer = isDrawer; }
    void ToggleDrawer() { m_isDrawerActive = !m_isDrawerActive; }

private:
    void DrawBrowserContent();
    void DrawContentGrid();
    void DrawSideBar();

    bool m_isDrawer = false;
    bool m_isDrawerActive = false;
    
    std::filesystem::path m_currentDirectory;
    std::filesystem::path m_baseDirectory;
    
    float m_thumbnailSize = 96.0f;
    float m_padding = 16.0f;

    char m_searchBuffer[256] = {0};
};

} // namespace AstralEngine
