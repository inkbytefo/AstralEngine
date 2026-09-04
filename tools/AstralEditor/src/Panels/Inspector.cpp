#include "Astral/Editor/Panels/Inspector.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Astral {
namespace {

constexpr const char* PrimitiveNames[] = {
    "Kure (Sphere)", "Kutu (Box)", "Torus (Simit)", "Zemin (Plane)", "Kapsul (Capsule)", "Silindir (Cylinder)"
};
constexpr const char* OperationNames[] = {
    "Birlestir (Union)", "Cikar (Subtract)", "Kesisim (Intersect)", "Yumusak Birlestir (Smooth Union)", "Yumusak Cikar (Smooth Sub)"
};

// ── Yardımcı: Görsel 1'deki gibi modern 3 parçalı XYZ Kontrolü ────────────────
bool DrawModernVector3Field(const char* label, const char* strId, glm::vec3& values, float speed,
                           const glm::vec3& resetValue = glm::vec3(0.0f), float minVal = 0.0f, float maxVal = 0.0f) {
    bool changed = false;
    ImGui::PushID(strId);

    // Sol sütun: Küçük radio/hedef ikonu + Özellik Adı
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(o)");
    ImGui::SameLine(0, 5.0f);
    ImGui::TextUnformatted(label);

    // Sağ sütun hesaplama (genişliğin yaklaşık %62'si)
    const float labelWidth = 105.0f;
    ImGui::SameLine(labelWidth);

    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = 4.0f;
    const float itemWidth = (availWidth - spacing * 2.0f) / 3.0f;

    struct AxisConfig {
        const char* id;
        const char* name;
        float* val;
        float reset;
        ImVec4 col;
    };

    AxisConfig axes[3] = {
        { "##X", "X", &values.x, resetValue.x, ImVec4(0.85f, 0.35f, 0.35f, 1.0f) }, // Kırmızımsı X
        { "##Y", "Y", &values.y, resetValue.y, ImVec4(0.40f, 0.80f, 0.40f, 1.0f) }, // Yeşilimsi Y
        { "##Z", "Z", &values.z, resetValue.z, ImVec4(0.35f, 0.60f, 0.95f, 1.0f) }  // Mavimsi Z
    };

    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine(0, spacing);

        ImGui::PushID(i);
        ImGui::BeginGroup();

        // Arka plan koyu kutu için frame style
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.13f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.22f, 0.25f, 1.0f));

        // Özel kutu: Sol tarafta soluk harf, yanında DragFloat
        ImGui::SetNextItemWidth(itemWidth);

        // Kutu içi harf ve drag
        char formatBuf[32];
        std::snprintf(formatBuf, sizeof(formatBuf), " %s  %%.2f", axes[i].name);

        if (ImGui::DragFloat(axes[i].id, axes[i].val, speed, minVal, maxVal, formatBuf)) {
            changed = true;
        }

        // Çift tıklamayla sıfırlama (Reset)
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
            *axes[i].val = axes[i].reset;
            changed = true;
        }

        ImGui::PopStyleColor(3);
        ImGui::EndGroup();
        ImGui::PopID();
    }

    ImGui::PopID();
    return changed;
}

// ── Yardımcı: Görsel 1'deki gibi Sayı Kutusu + Slider Çubuğu ─────────────────
bool DrawModernSliderProperty(const char* label, const char* strId, float* value, float minVal, float maxVal, const char* format = "%.2f") {
    bool changed = false;
    ImGui::PushID(strId);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(o)");
    ImGui::SameLine(0, 5.0f);
    ImGui::TextUnformatted(label);

    const float labelWidth = 110.0f;
    ImGui::SameLine(labelWidth);

    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float inputWidth = 52.0f;
    const float spacing = 6.0f;
    const float sliderWidth = availWidth - inputWidth - spacing;

    // 1. Sayısal Input Kutusu
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::DragFloat("##Num", value, 0.01f, minVal, maxVal, format)) {
        changed = true;
    }

    // 2. Yanında Slider Çubuğu
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(sliderWidth);
    if (ImGui::SliderFloat("##Slide", value, minVal, maxVal, "")) {
        changed = true;
    }
    ImGui::PopStyleColor();

    ImGui::PopID();
    return changed;
}

// ── Yardımcı: Görsel 1'deki gibi Renk Kutusu + Hex Kodu ─────────────────────
bool DrawModernColorProperty(const char* label, const char* strId, glm::vec3& color) {
    bool changed = false;
    ImGui::PushID(strId);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(o)");
    ImGui::SameLine(0, 5.0f);
    ImGui::TextUnformatted(label);

    const float labelWidth = 110.0f;
    ImGui::SameLine(labelWidth);

    // Renk kutusu
    ImVec4 colVec4(color.r, color.g, color.b, 1.0f);
    if (ImGui::ColorEdit3("##ColorPick", glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        changed = true;
    }

    ImGui::SameLine(0, 8.0f);

    // Hex formatı (#FFFFFF)
    char hexBuf[16];
    int r = static_cast<int>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    int g = static_cast<int>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
    int b = static_cast<int>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));
    std::snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", r, g, b);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
    ImGui::SetNextItemWidth(75.0f);
    ImGui::InputText("##Hex", hexBuf, sizeof(hexBuf), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();

    ImGui::PopID();
    return changed;
}

// ── Yardımcı: Görsel 1'deki gibi Kart Başlığı ve Context Menu ────────────────
bool BeginModernComponentCard(const char* title, const char* strId, bool* isExpanded, bool* shouldRemove = nullptr) {
    ImGui::Spacing();

    // Kart başlığı arka planı
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                               ImGuiTreeNodeFlags_Framed |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_AllowOverlap;

    bool open = ImGui::TreeNodeEx(strId, flags, "%s", title);
    *isExpanded = open;

    // Başlığın en sağına: `...` kebab context menüsü butonu
    const float buttonWidth = 24.0f;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttonWidth - 4.0f);

    char btnId[64];
    std::snprintf(btnId, sizeof(btnId), "...##btn_%s", strId);
    char popupId[64];
    std::snprintf(popupId, sizeof(popupId), "SettingsPopup_%s", strId);

    if (ImGui::SmallButton(btnId)) {
        ImGui::OpenPopup(popupId);
    }

    if (ImGui::BeginPopup(popupId)) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s Ayarlari", title);
        ImGui::Separator();
        if (shouldRemove && ImGui::MenuItem("Bileseni Kaldir")) {
            *shouldRemove = true;
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(3);

    if (open) {
        ImGui::Dummy(ImVec2(0, 3.0f));
    }
    return open;
}

void EndModernComponentCard(bool isExpanded) {
    if (isExpanded) {
        ImGui::TreePop();
        ImGui::Dummy(ImVec2(0, 4.0f));
    }
}

void DrawEmptyState() {
    ImGui::Dummy(ImVec2(0.0f, std::max(20.0f, ImGui::GetContentRegionAvail().y * 0.20f)));
    const char* title = "Nesne Secilmedi";
    ImGui::SetCursorPosX(std::max(8.0f, (ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) * 0.5f));
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.70f, 1.0f), "%s", title);
    ImGui::Spacing();
    ImGui::SetCursorPosX(16.0f);
    ImGui::TextWrapped("Ozellikleri denetlemek ve duzenlemek icin Sahne Agaci uzerinden bir varlik secin.");
}

bool IsEntitySelfVisible(Entity entity) {
    if (!entity.IsValid()) return false;
    if (entity.HasComponent<VisibilityComponent>()) {
        return entity.GetComponent<VisibilityComponent>().isVisible;
    }
    if (entity.HasComponent<SDFComponent>()) {
        return entity.GetComponent<SDFComponent>().isVisible != 0;
    }
    return true;
}

void ToggleEntityVisibility(Entity entity) {
    if (!entity.IsValid()) return;
    bool current = IsEntitySelfVisible(entity);
    bool next = !current;

    if (entity.HasComponent<SDFComponent>()) {
        entity.GetComponent<SDFComponent>().isVisible = next ? 1 : 0;
    }
    if (entity.HasComponent<VisibilityComponent>()) {
        entity.GetComponent<VisibilityComponent>().isVisible = next;
    } else {
        entity.AddComponent<VisibilityComponent>(VisibilityComponent{ next });
    }
}

} // namespace

void Inspector::Draw(Scene& scene, Entity& selectedEntity) {
    (void)scene;
    ImGui::Begin("Bilesen Denetcisi");

    if (!selectedEntity.IsValid()) {
        m_NameEntity = NullEntityHandle;
        DrawEmptyState();
        ImGui::End();
        return;
    }

    // ── 1. Üst Başlık: INSPECTOR Etiketi ve Varlık Adı (Görsel 1) ───────────
    ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.48f, 1.0f), "INSPECTOR");

    if (m_NameEntity != selectedEntity.GetHandle()) {
        m_NameEntity = selectedEntity.GetHandle();
        const std::string name = selectedEntity.HasComponent<TagComponent>()
            ? selectedEntity.GetComponent<TagComponent>().tag
            : "Entity " + std::to_string(selectedEntity.GetIndex());
        std::snprintf(m_NameBuffer.data(), m_NameBuffer.size(), "%s", name.c_str());
    }

    // Büyük Varlık Adı Kutusu + Sağında Görünürlük Toggle (o)
    const float toggleBtnWidth = 28.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - toggleBtnWidth - 6.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
    ImGui::InputText("##EntityNameInput", m_NameBuffer.data(), m_NameBuffer.size());
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (!selectedEntity.HasComponent<TagComponent>()) {
            selectedEntity.AddComponent<TagComponent>();
        }
        selectedEntity.GetComponent<TagComponent>().tag = m_NameBuffer.data();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 6.0f);

    // Varlık genel görünürlük durumu
    const bool isVisible = IsEntitySelfVisible(selectedEntity);
    if (ImGui::Button(isVisible ? "[V]" : "[-]", ImVec2(toggleBtnWidth, 0))) {
        ToggleEntityVisibility(selectedEntity);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Varligi Gizle" : "Varligi Goster");
    }

    ImGui::Separator();

    // ── 2. TRANSFORM Bileşen Kartı (Görsel 1) ────────────────────────────────
    if (selectedEntity.HasComponent<TransformComponent>()) {
        bool expanded = false;
        if (BeginModernComponentCard("Transform", "TransformCard", &expanded)) {
            auto& transform = selectedEntity.GetComponent<TransformComponent>();

            // Position (X, Y, Z)
            DrawModernVector3Field("Position", "Pos", transform.position, 0.05f, glm::vec3(0.0f));

            // Rotation (X, Y, Z - Euler acilari)
            glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
            if (DrawModernVector3Field("Rotation", "Rot", euler, 0.5f, glm::vec3(0.0f))) {
                transform.rotation = glm::normalize(glm::quat(glm::radians(euler)));
            }

            // Scale (X, Y, Z)
            DrawModernVector3Field("Scale", "Scl", transform.scale, 0.02f, glm::vec3(1.0f), 0.001f, 1000.0f);

            EndModernComponentCard(expanded);
        }
    }

    // ── 3. SDF GEOMETRI Bileşen Kartı (Görsel 1) ─────────────────────────────
    if (selectedEntity.HasComponent<SDFComponent>()) {
        bool expanded = false;
        bool removeSDF = false;
        if (BeginModernComponentCard("SDF Geometri", "SDFCard", &expanded, &removeSDF)) {
            auto& sdf = selectedEntity.GetComponent<SDFComponent>();

            // Primitif Tipi (Combo)
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(o)");
            ImGui::SameLine(0, 5.0f);
            ImGui::TextUnformatted("Primitive");
            ImGui::SameLine(110.0f);
            int primitive = static_cast<int>(std::min<uint32_t>(sdf.primitiveType, IM_ARRAYSIZE(PrimitiveNames) - 1));
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##PrimCombo", &primitive, PrimitiveNames, IM_ARRAYSIZE(PrimitiveNames))) {
                sdf.primitiveType = static_cast<uint32_t>(primitive);
            }

            // CSG İşlemi (Combo)
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(o)");
            ImGui::SameLine(0, 5.0f);
            ImGui::TextUnformatted("Operation");
            ImGui::SameLine(110.0f);
            int operation = static_cast<int>(std::min<uint32_t>(sdf.operation, IM_ARRAYSIZE(OperationNames) - 1));
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##OpCombo", &operation, OperationNames, IM_ARRAYSIZE(OperationNames))) {
                sdf.operation = static_cast<uint32_t>(operation);
            }

            // Blend Factor (Sayı + Slider)
            DrawModernSliderProperty("Blend Radius", "Blend", &sdf.blendFactor, 0.0f, 1.5f, "%.2f m");

            EndModernComponentCard(expanded);
        }
        if (removeSDF) {
            selectedEntity.RemoveComponent<SDFComponent>();
        }
    }

    // ── 4. MATERYAL Bileşen Kartı (Görsel 1) ─────────────────────────────────
    if (selectedEntity.HasComponent<SDFComponent>()) {
        bool expanded = false;
        if (BeginModernComponentCard("Materyal (PBR)", "MaterialCard", &expanded)) {
            auto& sdf = selectedEntity.GetComponent<SDFComponent>();

            // Albedo Renk (Kutu + Hex)
            DrawModernColorProperty("Albedo Color", "Albedo", sdf.albedo);

            // Roughness (Sayı + Slider)
            DrawModernSliderProperty("Roughness", "Rough", &sdf.roughness, 0.0f, 1.0f, "%.2f");

            // Metallic (Sayı + Slider)
            DrawModernSliderProperty("Metallic", "Metal", &sdf.metallic, 0.0f, 1.0f, "%.2f");

            EndModernComponentCard(expanded);
        }
    }

    // ── 5. FİZİK VE HIZ Bileşen Kartı (Görsel 1) ─────────────────────────────
    if (selectedEntity.HasComponent<VelocityComponent>()) {
        bool expanded = false;
        bool removeVel = false;
        if (BeginModernComponentCard("Fizik & Hiz", "VelocityCard", &expanded, &removeVel)) {
            auto& vel = selectedEntity.GetComponent<VelocityComponent>();

            DrawModernVector3Field("Lineer Hiz", "LinVel", vel.linear, 0.1f, glm::vec3(0.0f));
            DrawModernVector3Field("Acisal Hiz", "AngVel", vel.angular, 0.1f, glm::vec3(0.0f));

            EndModernComponentCard(expanded);
        }
        if (removeVel) {
            selectedEntity.RemoveComponent<VelocityComponent>();
        }
    }

    // ── 6. Alt Eylem Butonu: + Add Component (Görsel 1) ───────────────────────
    ImGui::Dummy(ImVec2(0, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6.0f));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.24f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));

    if (ImGui::Button("+ Add component", ImVec2(-1, 32))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Bilesen Secin");
        ImGui::Separator();

        if (!selectedEntity.HasComponent<SDFComponent>()) {
            if (ImGui::MenuItem("SDF Geometri (SDFComponent)")) {
                selectedEntity.AddComponent<SDFComponent>();
            }
        }
        if (!selectedEntity.HasComponent<VelocityComponent>()) {
            if (ImGui::MenuItem("Fizik ve Hiz (VelocityComponent)")) {
                selectedEntity.AddComponent<VelocityComponent>();
            }
        }
        if (!selectedEntity.HasComponent<HealthComponent>()) {
            if (ImGui::MenuItem("Can / Saglik (HealthComponent)")) {
                selectedEntity.AddComponent<HealthComponent>();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(3);

    ImGui::End();
}

} // namespace Astral
