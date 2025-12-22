#include "MainToolbarPanel.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace AstralEngine {

MainToolbarPanel::MainToolbarPanel() : EditorPanel("Main Toolbar") {
}

MainToolbarPanel::~MainToolbarPanel() {}

void MainToolbarPanel::OnDraw() {
    // Toolbar is unique, usually horizontal and fixed height
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight())); // Below MenuBar
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 48));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    
    if (ImGui::Begin(m_name.c_str(), nullptr, flags)) {
        DrawUtilityButtons();
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        DrawModeSelector();
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        DrawPlayControls();
    }
    ImGui::End();
}

void MainToolbarPanel::DrawUtilityButtons() {
    if (ImGui::Button("Save")) { /* Save current level */ }
    ImGui::SameLine();
    if (ImGui::Button("Build")) { /* Build system */ }
}

void MainToolbarPanel::DrawModeSelector() {
    const char* modes[] = { "Selection", "Landscape", "Foliage", "Mesh Paint", "Modeling" };
    if (ImGui::Button(modes[m_currentMode])) {
        ImGui::OpenPopup("ModeSelectorPopup");
    }
    
    if (ImGui::BeginPopup("ModeSelectorPopup")) {
        for (int i = 0; i < IM_ARRAYSIZE(modes); i++) {
            if (ImGui::Selectable(modes[i], m_currentMode == i)) {
                m_currentMode = i;
            }
        }
        ImGui::EndPopup();
    }
}

void MainToolbarPanel::DrawPlayControls() {
    // Center alignment roughly
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f - 60);
    
    if (ImGui::Button("Play")) { /* Start simulation */ }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) { /* Pause simulation */ }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) { /* Stop simulation */ }
}

} // namespace AstralEngine
