#include "json_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace astral {

namespace {

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

bool edit_bool(const char* label, Json& j) {
    bool b = j.as_bool();
    if (!ImGui::Checkbox(label, &b)) return false;
    j = b;
    return true;
}

bool edit_number(const char* label, Json& j) {
    float f = static_cast<float>(j.as_number());
    if (!ImGui::DragFloat(label, &f, 0.01f, 0.0f, 0.0f, "%.3f")) return false;
    j = static_cast<double>(f);
    return true;
}

bool edit_string(const char* label, Json& j) {
    char buffer[256];
    std::snprintf(buffer, sizeof buffer, "%s", j.as_string().c_str());
    if (!ImGui::InputText(label, buffer, sizeof buffer, ImGuiInputTextFlags_EnterReturnsTrue)) return false;
    j = std::string(buffer);
    return true;
}

bool edit_numeric_array(const char* label, Json& j) {
    const auto& a = j.as_array();
    float v[4] = {};
    for (std::size_t i = 0; i < a.size(); ++i) v[i] = static_cast<float>(a[i].as_number());
    const bool colour = contains(label, "color") || contains(label, "colour") || contains(label, "emissive") ||
                        contains(label, "sky") || contains(label, "ground");
    bool edited = false;
    if (colour && a.size() == 3) edited = ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    else if (colour && a.size() == 4) edited = ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    else if (a.size() == 2) edited = ImGui::DragFloat2(label, v, 0.01f);
    else if (a.size() == 3) edited = ImGui::DragFloat3(label, v, 0.01f);
    else edited = ImGui::DragFloat4(label, v, 0.01f);
    if (!edited) return false;
    Json out = Json::array();
    for (std::size_t i = 0; i < a.size(); ++i) out.push_back(static_cast<double>(v[i]));
    j = std::move(out);
    return true;
}

bool edit_array(const char* label, Json& j, int depth) {
    const auto& a = j.as_array();
    const bool numbers = a.size() >= 2 && a.size() <= 4 &&
                         std::all_of(a.begin(), a.end(), [](const Json& v) { return v.is_number(); });
    if (numbers) return edit_numeric_array(label, j);
    if (!ImGui::TreeNode(label, "%s [%zu]", label, a.size())) return false;
    bool changed = false;
    for (std::size_t i = 0; i < j.as_array().size(); ++i) {
        Json item = j.as_array()[i];
        const std::string name = "[" + std::to_string(i) + "]";
        if (edit_json(name.c_str(), item, depth + 1)) {
            Json out = Json::array();
            const auto& cur = j.as_array();
            for (std::size_t k = 0; k < cur.size(); ++k) out.push_back(k == i ? std::move(item) : cur[k]);
            j = std::move(out);
            changed = true;
        }
    }
    ImGui::TreePop();
    return changed;
}

bool edit_object(const char* label, Json& j, int depth) {
    const bool open = depth == 0 || ImGui::TreeNode(label);
    if (!open) return false;
    bool changed = false;
    const std::size_t count = j.as_object().size();
    for (std::size_t i = 0; i < count; ++i) {
        const std::string key = j.as_object()[i].first;
        Json v = j.as_object()[i].second;
        if (edit_json(key.c_str(), v, depth + 1)) {
            j[key] = std::move(v);
            changed = true;
        }
    }
    if (depth > 0) ImGui::TreePop();
    return changed;
}

} // namespace

bool edit_json(const char* label, Json& j, int depth) {
    bool changed = false;
    ImGui::PushID(label);
    switch (j.type()) {
    case Json::Type::Null: ImGui::TextDisabled("%s: none", label); break;
    case Json::Type::Bool: changed = edit_bool(label, j); break;
    case Json::Type::Number: changed = edit_number(label, j); break;
    case Json::Type::String: changed = edit_string(label, j); break;
    case Json::Type::Array: changed = edit_array(label, j, depth); break;
    case Json::Type::Object: changed = edit_object(label, j, depth); break;
    }
    ImGui::PopID();
    return changed;
}

} // namespace astral
