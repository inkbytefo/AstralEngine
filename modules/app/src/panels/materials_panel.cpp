#include "debug_panel.hpp"
#include "../material_library.hpp"

#include <astral/app/app.hpp>

#include <imgui.h>

#include <string_view>
#include <vector>

namespace astral {

namespace {

bool edit_material_parameter(const MaterialLibrary::Parameter& p, float* values) {
    const ShaderUniform& u = *p.uniform;
    float* v = values + p.offset;
    const char* label = u.name.c_str();
    switch (u.type) {
    case UniformType::Float:
        return u.hints.range ? ImGui::SliderFloat(label, v, u.hints.range->min, u.hints.range->max)
                             : ImGui::DragFloat(label, v, 0.01f);
    case UniformType::Vec2: return ImGui::DragFloat2(label, v, 0.01f);
    case UniformType::Vec3:
        return u.hints.source_color ? ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
                                    : ImGui::DragFloat3(label, v, 0.01f);
    case UniformType::Vec4:
        return u.hints.source_color ? ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
                                    : ImGui::DragFloat4(label, v, 0.01f);
    case UniformType::Int: {
        int n = static_cast<int>(*v);
        if (!ImGui::DragInt(label, &n)) return false;
        *v = static_cast<float>(n);
        return true;
    }
    case UniformType::Bool: {
        bool b = *v != 0.0f;
        if (!ImGui::Checkbox(label, &b)) return false;
        *v = b ? 1.0f : 0.0f;
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
