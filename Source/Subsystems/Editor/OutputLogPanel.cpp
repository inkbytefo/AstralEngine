#include "OutputLogPanel.h"
#include <imgui.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace AstralEngine {

OutputLogPanel::OutputLogPanel() : EditorPanel("Output Log") {
    // Register interest in global logs if possible, or wait for manual push
}

OutputLogPanel::~OutputLogPanel() {}

void OutputLogPanel::OnDraw() {
    if (!m_isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_name.c_str(), &m_isOpen)) {
        ImGui::End();
        return;
    }

    // Toolbar area
    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    ImGui::InputText("Filter", m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::Separator();

    // Log display area
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

    for (const auto& entry : m_logs) {
        // Simple search filter
        if (m_searchBuffer[0] != '\0' && entry.message.find(m_searchBuffer) == std::string::npos)
            continue;

        ImVec4 color;
        bool has_color = false;
        if (entry.level == Logger::LogLevel::Error || entry.level == Logger::LogLevel::Critical) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
        else if (entry.level == Logger::LogLevel::Warning) { color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f); has_color = true; }
        else if (entry.level == Logger::LogLevel::Debug) { color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); has_color = true; }

        if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
        
        ImGui::TextUnformatted(entry.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.message.c_str());

        if (has_color) ImGui::PopStyleColor();
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::End();
}

void OutputLogPanel::AddLog(Logger::LogLevel level, const std::string& message) {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "[%H:%M:%S]");
    
    m_logs.push_back({level, message, oss.str()});
    
    if (m_logs.size() > 1000) {
        m_logs.erase(m_logs.begin());
    }
}

void OutputLogPanel::Clear() {
    m_logs.clear();
}

} // namespace AstralEngine
