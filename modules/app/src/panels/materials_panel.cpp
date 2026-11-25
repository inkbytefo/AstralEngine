#include "debug_panel.hpp"
#include "../material_library.hpp"

#include <astral/app/app.hpp>

#include <imgui.h>

#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>

namespace astral {

namespace {

// Edits float components in place on a copy of the words, then stores their bits back (ADR-0042).
template <class Edit> bool edit_floats(std::uint32_t* words, int n, Edit&& edit) {
    float f[4] = {};
    for (int i = 0; i < n; ++i) f[i] = std::bit_cast<float>(words[i]);
    if (!edit(f)) return false;
    for (int i = 0; i < n; ++i) words[i] = std::bit_cast<std::uint32_t>(f[i]);
    return true;
}

bool edit_material_parameter(const MaterialLibrary::Parameter& p, std::uint32_t* values) {
    const ShaderUniform& u = *p.uniform;
    std::uint32_t* w = values + p.offset;
    const char* label = u.name.c_str();
    switch (u.type) {
    case UniformType::Float:
        return edit_floats(w, 1, [&](float* v) {
            return u.hints.range ? ImGui::SliderFloat(label, v, u.hints.range->min, u.hints.range->max)
                                 : ImGui::DragFloat(label, v, 0.01f);
        });
    case UniformType::Vec2: return edit_floats(w, 2, [&](float* v) { return ImGui::DragFloat2(label, v, 0.01f); });
    case UniformType::Vec3:
        return edit_floats(w, 3, [&](float* v) {
            return u.hints.source_color ? ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
                                        : ImGui::DragFloat3(label, v, 0.01f);
        });
    case UniformType::Vec4:
        return edit_floats(w, 4, [&](float* v) {
            return u.hints.source_color ? ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
                                        : ImGui::DragFloat4(label, v, 0.01f);
        });
    case UniformType::Int: {
        int n = std::bit_cast<std::int32_t>(*w);
        if (!ImGui::DragInt(label, &n)) return false;
        *w = std::bit_cast<std::uint32_t>(n);
        return true;
    }
    case UniformType::Bool: {
        bool b = *w != 0u;
        if (!ImGui::Checkbox(label, &b)) return false;
        *w = b ? 1u : 0u;
        return true;
    }
    case UniformType::Sampler2D: ImGui::TextDisabled("%s (texture, set in the file)", label); return false;
    }
    return false;
}

class MaterialsPanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Materials"; }

    void draw(Context& ctx, DebugPanelState& state, bool& open) override {
        MaterialLibrary* materials = state.materials;
        ImGui::SetNextWindowSize(ImVec2(340, 360), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Materials", &open)) {
            if (const std::string_view rt = ctx.materials().shader_error(); !rt.empty()) {
                ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Ray-traced shaders: not built");
                ImGui::TextWrapped("%.*s", static_cast<int>(rt.size()), rt.data());
                ImGui::Separator();
            }
            if (!materials) ImGui::TextDisabled("No renderer: no materials.");
            else ImGui::TextWrapped("Live edits affect every user of the material for this session.");
            for (MaterialLibrary::Info& info : materials ? materials->loaded() : std::vector<MaterialLibrary::Info>{}) {
                if (!ImGui::CollapsingHeader(info.path.c_str())) continue;
                ImGui::PushID(info.path.c_str());
                ImGui::TextDisabled("%s", info.shader.c_str());
                bool changed = false;
                // The widgets come from the uniform hints: range -> slider, source_color -> colour picker.
                for (const MaterialLibrary::Parameter& p : info.parameters) {
                    changed |= edit_material_parameter(p, info.values->data());
                }
                if (changed) materials->apply(info.path); // live only: the .material file is not written
                ImGui::PopID();
            }
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<DebugPanel> make_materials_panel() { return std::make_unique<MaterialsPanel>(); }

} // namespace astral
