#include "ViewportPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Subsystems/Renderer/Core/RenderSubsystem.h"
#include "../../Subsystems/Scene/Scene.h"

namespace AstralEngine {

ViewportPanel::ViewportPanel() : EditorPanel("Level Viewport") {
    m_camera = std::make_unique<Camera>();
    m_camera->SetPosition(glm::vec3(0.0f, 2.0f, 10.0f));
}

ViewportPanel::~ViewportPanel() {}

void ViewportPanel::OnDraw() {
    if (!m_isOpen) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    // Create a tab bar for UE5 style multiple views if needed later, for now just the window
    if (ImGui::Begin(m_name.c_str(), &m_isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        m_windowPos = { pos.x, pos.y };
        m_windowSize = { size.x, size.y };

        m_isFocused = ImGui::IsWindowFocused();
        m_isHovered = ImGui::IsWindowHovered();

        // 1. Draw Viewport Toolbar (Floating overlay)
        DrawToolbar();

        // 2. Render the actual texture
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        
        // Sanity check for dimensions (avoid extreme or negative values during layout initialization)
        if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0 && 
            viewportPanelSize.x < 8192 && viewportPanelSize.y < 8192) {
            
            if (viewportPanelSize.x != m_size.x || viewportPanelSize.y != m_size.y) {
                m_size = { viewportPanelSize.x, viewportPanelSize.y };
                if (m_onResize) {
                    m_onResize((uint32_t)m_size.x, (uint32_t)m_size.y);
                }
            }
        }

        if (m_viewportDescriptorSet) {
            ImGui::Image((ImTextureID)m_viewportDescriptorSet, viewportPanelSize);
        } else {
            ImGui::Dummy(viewportPanelSize);
            ImGui::Text("No Viewport Texture Available");
        }

        // 3. Handle Input if focused/hovered
        HandleInput();
        // ... existing rendering code eventually shows the image ...
        // For now, let's just add the overlay at the end of the window content
        if (m_isDraggingFile && m_isHovered) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 min = ImGui::GetItemRectMin(); // If we just drew the image, this is the viewport image rect
            ImVec2 max = ImGui::GetItemRectMax();
            
            // If we haven't drawn the image yet or it's not the last item, we can use window bounds
            if (max.x - min.x < 10.0f) { // fallback
                min = ImGui::GetWindowPos();
                max = ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
            }

            drawList->AddRectFilled(min, max, IM_COL32(0, 120, 215, 100)); // Blue tint
            drawList->AddRect(min, max, IM_COL32(0, 120, 215, 255), 0.0f, 0, 4.0f); // Thick border

            const char* text = "DROP TO IMPORT ASSET";
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImVec2 textPos = ImVec2(min.x + (max.x - min.x - textSize.x) * 0.5f, min.y + (max.y - min.y - textSize.y) * 0.5f);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text);
        }

    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::DrawToolbar() {
    // Positioning the toolbar at the top left of the viewport content area
    ImGui::SetCursorPos(ImVec2(10, 30));
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.6f));
    ImGui::BeginChild("ViewportToolbar", ImVec2(400, 30), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Transform Tools
    if (ImGui::Button("M")) { /* Move Mode */ } ImGui::SameLine();
    if (ImGui::Button("R")) { /* Rotate Mode */ } ImGui::SameLine();
    if (ImGui::Button("S")) { /* Scale Mode */ } 
    
    ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
    
    // View Mode
    const char* viewModes[] = { "Lit", "Wireframe", "Unlit" };
    int currentMode = (int)m_viewMode;
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##ViewMode", &currentMode, viewModes, IM_ARRAYSIZE(viewModes))) {
        m_viewMode = (ViewMode)currentMode;
    }
    
    ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();

    // Camera Speed
    ImGui::SetNextItemWidth(60);
    ImGui::SliderFloat("Speed", &m_cameraSpeed, 0.1f, 10.0f, "%.1fx");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ViewportPanel::HandleInput() {
    if (!m_isHovered) return;
    
    // Navigation logic (ASDF/WASD) usually happens in SceneEditorSubsystem or delegated to a Controller
    // For now we just mark it.
}

bool ViewportPanel::IsPointOverViewport(float x, float y) const {
    return x >= m_windowPos.x && x <= (m_windowPos.x + m_windowSize.x) &&
           y >= m_windowPos.y && y <= (m_windowPos.y + m_windowSize.y);
}

} // namespace AstralEngine
