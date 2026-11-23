#include "editor_state.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace astral::editor {

namespace {

bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
    return it != hay.end();
}

const char* asset_badge(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    if (ext == ".scene") return "SCENE";
    if (ext == ".prefab") return "PREFAB";
    if (ext == ".world") return "WORLD";
    if (ext == ".material") return "MAT";
    if (ext == ".ashader") return "SHADER";
    return "ASSET";
}

class ContentShelfPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "content"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Content"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.content_shelf; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Content", &ec.state.vis.content_shelf)) {
            ImGui::End();
            return;
        }
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s", ec.state.ui.content_search.c_str());
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::InputTextWithHint("##AssetSearch", "Search assets...", buf, sizeof buf)) {
            ec.state.ui.content_search = buf;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) ec.workspace.refresh_asset_list();
        ImGui::SameLine();
        ImGui::TextDisabled("Root: %s", ec.state.desc.asset_root.filename().string().c_str());
        ImGui::Separator();

        if (ImGui::BeginTable("##AssetsTbl", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableHeadersRow();

            for (const std::filesystem::path& p : ec.state.ui.asset_files) {
                std::error_code ec_rel;
                const std::filesystem::path rel = std::filesystem::relative(p, ec.state.desc.asset_root, ec_rel);
                const std::string label = (!ec_rel && !rel.empty()) ? rel.generic_string() : p.filename().string();
                if (!icontains(label, ec.state.ui.content_search)) continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", asset_badge(p));
                ImGui::TableNextColumn();
                const bool clicked = ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ASTRAL_ASSET_PATH", label.c_str(), label.size() + 1);
                    ImGui::Text("Place %s", label.c_str());
                    ImGui::EndDragDropSource();
                }
                const std::string ext = p.extension().string();
                if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                    (ext == ".scene" || ext == ".world" || ext == ".prefab")) {
                    std::string err;
                    (void)ec.workspace.open_document(ec.app, p, &err);
                }
                ImGui::TableNextColumn();
                ImGui::PushID(label.c_str());
                if ((ext == ".scene" || ext == ".world") && ImGui::SmallButton("Open")) {
                    std::string err;
                    (void)ec.workspace.open_document(ec.app, p, &err);
                } else if (ext == ".prefab" && ec.state.docs.session && ec.state.docs.session->is_open() &&
                           ImGui::SmallButton("Spawn")) {
                    Transaction tx;
                    tx.summary = "Spawn " + label;
                    tx.expected_revision = ec.state.docs.session->document().revision;
                    tx.commands.push_back({.kind = CommandKind::InstantiatePrefab, .asset_path = label});
                    if (ec.state.docs.session->apply(tx).ok) sync_scene_to_world_live(ec);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<EditorPanel> make_content_shelf_panel() { return std::make_unique<ContentShelfPanel>(); }

} // namespace astral::editor
