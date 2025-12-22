#include "ContentBrowserPanel.h"
#include <imgui.h>
#include "../../Core/Logger.h"

namespace AstralEngine {

ContentBrowserPanel::ContentBrowserPanel() : EditorPanel("Content Browser") {
    m_baseDirectory = std::filesystem::current_path() / "Assets";
    m_currentDirectory = m_baseDirectory;
    
    if (!std::filesystem::exists(m_baseDirectory)) {
        std::filesystem::create_directories(m_baseDirectory);
    }
}

ContentBrowserPanel::~ContentBrowserPanel() {}

void ContentBrowserPanel::OnDraw() {
    if (m_isDrawer) {
        // Implementation for "Drawer" mode (sliding panel)
        // For now, let's just make it a dedicated overlay or restricted window
        if (m_isDrawerActive) {
             ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 400));
             ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.8f, 350));
             if (ImGui::Begin("Content Drawer", &m_isDrawerActive, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
                 DrawBrowserContent();
             }
             if (!ImGui::IsWindowFocused() && ImGui::IsMouseClicked(0)) {
                 // m_isDrawerActive = false; // UE5 style: close on lose focus
             }
             ImGui::End();
        }
    } else {
        if (!m_isOpen) return;

        if (ImGui::Begin(m_name.c_str(), &m_isOpen)) {
            DrawBrowserContent();
        }
        ImGui::End();
    }
}

void ContentBrowserPanel::DrawBrowserContent() {
    // Toolbar
    if (ImGui::Button("Import")) { /* Trigger Import */ }
    ImGui::SameLine();
    if (ImGui::Button("Add")) { /* Trigger Add Menu */ }
    ImGui::SameLine();
    if (ImGui::Button("Save All")) { /* Trigger Save All */ }
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##Search", m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::Separator();

    // Split layout: Sidebar + Grid
    static float sidebarWidth = 200.0f;
    ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0), true);
    DrawSideBar();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("ContentGrid", ImVec2(0, 0), false);
    DrawContentGrid();
    ImGui::EndChild();
}

void ContentBrowserPanel::DrawSideBar() {
    if (ImGui::TreeNodeEx("Content", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow)) {
        // Recursively list core folders
        ImGui::Selectable("Models");
        ImGui::Selectable("Textures");
        ImGui::Selectable("Materials");
        ImGui::Selectable("Shaders");
        ImGui::TreePop();
    }
}

void ContentBrowserPanel::DrawContentGrid() {
    // Placeholder grid
    float cellSize = m_thumbnailSize + m_padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, 0, false);

    for (int i = 0; i < 20; ++i) {
        ImGui::Button("Asset", ImVec2(m_thumbnailSize, m_thumbnailSize));
        ImGui::TextWrapped("Asset_%d", i);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
}

} // namespace AstralEngine
