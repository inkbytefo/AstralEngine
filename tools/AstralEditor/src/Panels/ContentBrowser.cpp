#include "Astral/Editor/Panels/ContentBrowser.hpp"
#include "Astral/Project/Project.hpp"
#include "Astral/Asset/AssetManager.hpp"

#include <imgui.h>
#include "Astral/Editor/EditorTheme.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace Astral {

// Dosya uzantısına göre renk ve etiket belirleyici
struct FileTypeInfo {
    const char* label;
    ImVec4 color;
};

static FileTypeInfo GetFileTypeInfo(const std::filesystem::path& path, bool isDirectory) {
    if (isDirectory) {
        return { "[DIR]", ImVec4(0.88f, 0.68f, 0.28f, 1.0f) }; // Klasör Sarı/Turuncu
    }

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".spv" || ext == ".comp" || ext == ".vert" || ext == ".frag" || ext == ".glsl") {
        return { "[SHDR]", ImVec4(0.72f, 0.48f, 0.98f, 1.0f) }; // Mor/Eflatun
    }
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
        return { "[MESH]", ImVec4(0.34f, 0.65f, 1.00f, 1.0f) }; // Mavi
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".hdr") {
        return { "[TEX]", ImVec4(0.24f, 0.78f, 0.45f, 1.0f) };  // Yeşil
    }
    if (ext == ".scene" || ext == ".astral" || ext == ".json" || ext == ".astralproj") {
        return { "[SCENE]", ImVec4(0.95f, 0.60f, 0.20f, 1.0f) }; // Altın
    }
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
        return { "[CODE]", ImVec4(0.92f, 0.35f, 0.35f, 1.0f) }; // Kırmızı
    }

    return { "[FILE]", ImVec4(0.60f, 0.60f, 0.65f, 1.0f) };    // Nötr Gri
}

ContentBrowser::ContentBrowser() {
    RefreshFromProject();
}

void ContentBrowser::RefreshFromProject() {
    auto activeProj = Project::GetActive();
    if (activeProj && std::filesystem::exists(activeProj->GetAssetDirectory())) {
        std::error_code ec;
        m_BaseDirectory = std::filesystem::canonical(activeProj->GetAssetDirectory(), ec);
        if (ec) {
            m_BaseDirectory = activeProj->GetAssetDirectory();
        }
    } else if (std::filesystem::exists("assets")) {
        std::error_code ec;
        m_BaseDirectory = std::filesystem::canonical("assets", ec);
        if (ec) m_BaseDirectory = "assets";
    } else {
        m_BaseDirectory = std::filesystem::current_path();
    }
    m_CurrentDirectory = m_BaseDirectory;
    m_History.clear();
    m_History.push_back(m_CurrentDirectory);
    m_HistoryIndex = 0;
    m_SelectedItem.clear();
}

void ContentBrowser::SetBaseDirectory(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        m_BaseDirectory = std::filesystem::canonical(path, ec);
        if (ec) m_BaseDirectory = path;
        NavigateTo(m_BaseDirectory);
    }
}

void ContentBrowser::NavigateTo(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return;

    // Eğer yeni bir yola gidiliyorsa ileri geçmişi kes ve ekle
    if (m_HistoryIndex >= 0 && m_HistoryIndex < static_cast<int>(m_History.size()) - 1) {
        m_History.erase(m_History.begin() + m_HistoryIndex + 1, m_History.end());
    }
    m_History.push_back(path);
    m_HistoryIndex = static_cast<int>(m_History.size()) - 1;
    m_CurrentDirectory = path;
    m_SelectedItem.clear();
}

void ContentBrowser::NavigateBack() {
    if (m_HistoryIndex > 0) {
        m_HistoryIndex--;
        m_CurrentDirectory = m_History[m_HistoryIndex];
        m_SelectedItem.clear();
    }
}

void ContentBrowser::NavigateForward() {
    if (m_HistoryIndex < static_cast<int>(m_History.size()) - 1) {
        m_HistoryIndex++;
        m_CurrentDirectory = m_History[m_HistoryIndex];
        m_SelectedItem.clear();
    }
}

void ContentBrowser::Draw() {
    auto activeProj = Project::GetActive();
    if (activeProj && std::filesystem::exists(activeProj->GetAssetDirectory())) {
        std::error_code ec;
        auto canonicalAssetDir = std::filesystem::canonical(activeProj->GetAssetDirectory(), ec);
        if (!ec && canonicalAssetDir != m_BaseDirectory) {
            SetBaseDirectory(canonicalAssetDir);
        }
    }



    ImGui::BeginChild("ContentBrowserBody", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    DrawToolbar();
    ImGui::Separator();

    // İki bölmeli yerleşim: Sol Klasör Ağacı / Sağ Varlık Grid Görünümü
    if (ImGui::BeginTable("ContentBrowserLayoutTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("TreeColumn", ImGuiTableColumnFlags_WidthFixed, std::clamp(ImGui::GetContentRegionAvail().x * 0.24f, 110.0f, 180.0f));
        ImGui::TableSetupColumn("GridColumn", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();

        // ── Sol Sütun: Klasör Ağacı ve Hızlı Erişim ─────────────────
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("DirectoryTreeChild", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);

        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "HIZLI ERISIM");
        ImGui::Spacing();

        auto drawQuickLink = [this](const char* label, const std::filesystem::path& target) {
            bool isSelected = (m_CurrentDirectory == target);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.34f, 0.65f, 1.0f, 1.0f));
            }
            if (ImGui::Selectable(label, isSelected)) {
                if (std::filesystem::exists(target)) {
                    NavigateTo(target);
                }
            }
            if (isSelected) {
                ImGui::PopStyleColor();
            }
        };

        drawQuickLink(" > assets", m_BaseDirectory);
        if (std::filesystem::exists("assets/models"))   drawQuickLink("   - models", "assets/models");
        if (std::filesystem::exists("assets/textures")) drawQuickLink("   - textures", "assets/textures");
        if (std::filesystem::exists("assets/scenes"))   drawQuickLink("   - scenes", "assets/scenes");
        if (std::filesystem::exists("assets/shaders"))  drawQuickLink("   - shaders", "assets/shaders");
        if (std::filesystem::exists("shaders"))         drawQuickLink(" > shaders (root)", "shaders");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "KLASOR AGACI");
        ImGui::Spacing();
        DrawDirectoryTree(m_BaseDirectory);

        ImGui::EndChild();

        // ── Sağ Sütun: Dosya Grid Kart Görünümü ─────────────────────
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("ContentGridChild", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);
        DrawContentGrid();
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::Separator();
    DrawFooter();

    ImGui::EndChild();
}

void ContentBrowser::DrawToolbar() {
    // Geri / İleri / Yukarı butonları
    bool canGoBack = (m_HistoryIndex > 0);
    if (!canGoBack) ImGui::BeginDisabled();
    if (ImGui::Button(" < ")) NavigateBack();
    if (!canGoBack) ImGui::EndDisabled();

    ImGui::SameLine();
    bool canGoForward = (m_HistoryIndex < static_cast<int>(m_History.size()) - 1);
    if (!canGoForward) ImGui::BeginDisabled();
    if (ImGui::Button(" > ")) NavigateForward();
    if (!canGoForward) ImGui::EndDisabled();

    ImGui::SameLine();
    bool canGoUp = (m_CurrentDirectory != m_BaseDirectory && m_CurrentDirectory.has_parent_path());
    if (!canGoUp) ImGui::BeginDisabled();
    if (ImGui::Button(" ^ ")) {
        NavigateTo(m_CurrentDirectory.parent_path());
    }
    if (!canGoUp) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("+ Klasor")) m_CreatingNewFolder = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(72.0f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##SearchFilter", "Varlik ara...", m_SearchFilter, sizeof(m_SearchFilter));

    ImGui::BeginChild("Breadcrumbs", ImVec2(0, ImGui::GetFrameHeightWithSpacing() + 4.0f),
                      ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    // Ekmek kırıntısı (Breadcrumbs)
    std::filesystem::path relPath;
    try {
        relPath = std::filesystem::relative(m_CurrentDirectory, m_BaseDirectory);
    } catch (...) {
        relPath = m_CurrentDirectory;
    }

    if (ImGui::Button("assets")) {
        NavigateTo(m_BaseDirectory);
    }

    if (relPath != "." && !relPath.empty()) {
        std::filesystem::path accumulated = m_BaseDirectory;
        for (const auto& part : relPath) {
            accumulated /= part;
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            if (ImGui::Button(part.string().c_str())) {
                NavigateTo(accumulated);
                break;
            }
        }
    }

    ImGui::EndChild();
}

void ContentBrowser::DrawDirectoryTree(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) return;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_directory()) continue;

        const auto& p = entry.path();
        std::string name = p.filename().string();

        // Gizli klasörleri (.git vb.) atla
        if (!name.empty() && name[0] == '.') continue;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (m_CurrentDirectory == p) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool hasSubDirs = false;
        try {
            for (const auto& sub : std::filesystem::directory_iterator(p)) {
                if (sub.is_directory()) { hasSubDirs = true; break; }
            }
        } catch (...) {}

        if (!hasSubDirs) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool open = ImGui::TreeNodeEx(name.c_str(), flags);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            NavigateTo(p);
        }

        if (open) {
            DrawDirectoryTree(p);
            ImGui::TreePop();
        }
    }
}

void ContentBrowser::DrawContentGrid() {
    if (!std::filesystem::exists(m_CurrentDirectory)) {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Dizin bulunamadi!");
        return;
    }

    // Yeni klasör oluşturma modal/inline penceresi
    if (m_CreatingNewFolder) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::BeginChild("NewFolderBar", ImVec2(0, 36), true);
        ImGui::Text("Yeni Klasor Adi:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputText("##NewFolderName", m_NewFolderName, sizeof(m_NewFolderName));
        ImGui::SameLine();
        if (ImGui::Button("Olustur")) {
            std::filesystem::create_directory(m_CurrentDirectory / m_NewFolderName);
            m_CreatingNewFolder = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Iptal")) {
            m_CreatingNewFolder = false;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Klasör içeriklerini topla ve sırala (Önce klasörler, sonra alfabetik dosyalar)
    std::vector<std::filesystem::directory_entry> directories;
    std::vector<std::filesystem::directory_entry> files;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
            std::string filename = entry.path().filename().string();
            if (!filename.empty() && filename[0] == '.') continue; // Gizli dosyaları atla

            // Arama filtresi
            if (m_SearchFilter[0] != '\0') {
                std::string lowerFilter = m_SearchFilter;
                std::string lowerName = filename;
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                if (lowerName.find(lowerFilter) == std::string::npos) continue;
            }

            if (entry.is_directory()) {
                directories.push_back(entry);
            } else {
                files.push_back(entry);
            }
        }
    } catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Dizin okuma hatasi: %s", e.what());
        return;
    }

    std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b) {
        return a.path().filename().string() < b.path().filename().string();
    });
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.path().filename().string() < b.path().filename().string();
    });

    const float width = ImGui::GetContentRegionAvail().x;
    const int columns = std::clamp(static_cast<int>(width / (m_ThumbnailSize + m_Padding)), 1, 32);
    std::filesystem::path navigateAfterGrid;
    if (directories.empty() && files.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted(m_SearchFilter[0] ? "Eslesen varlik bulunamadi" : "Bu klasor bos");
        ImGui::TextDisabled("Yeni bir klasor olusturabilir veya baska bir konum secebilirsiniz.");
    }
    if (ImGui::BeginTable("AssetCards", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
        auto renderItem = [this, &navigateAfterGrid](const std::filesystem::directory_entry& entry, bool isDir) {
            ImGui::TableNextColumn();
            const auto& path = entry.path();
            const std::string filename = path.filename().string();
            const FileTypeInfo type = GetFileTypeInfo(path, isDir);
            ImGui::PushID(filename.c_str());
            const bool selected = m_SelectedItem == path;
            const ImVec2 size(std::max(20.0f, ImGui::GetContentRegionAvail().x), m_ThumbnailSize * 0.72f + 42.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? EditorPalette::Selection : EditorPalette::Raised);
            if (ImGui::Button("##AssetCard", size)) m_SelectedItem = path;
            ImGui::PopStyleColor();
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && isDir) navigateAfterGrid = path;
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 tint = ImGui::ColorConvertFloat4ToU32(type.color);
            const float cx = (min.x + max.x) * 0.5f;
            const float cy = min.y + m_ThumbnailSize * 0.34f;
            draw->PushClipRect(min, max, true);
            if (isDir) {
                draw->AddRectFilled({cx - 17, cy - 12}, {cx - 2, cy - 5}, tint, 3);
                draw->AddRectFilled({cx - 17, cy - 7}, {cx + 17, cy + 13}, tint, 3);
                draw->AddLine({cx - 12, cy - 3}, {cx + 12, cy - 3}, IM_COL32(255,255,255,75));
            } else {
                draw->AddRect({cx - 13, cy - 17}, {cx + 13, cy + 17}, tint, 3, 0, 1.5f);
                draw->AddLine({cx - 7, cy - 5}, {cx + 7, cy - 5}, tint, 1.5f);
                draw->AddLine({cx - 7, cy + 2}, {cx + 7, cy + 2}, tint, 1.5f);
                draw->AddLine({cx - 7, cy + 9}, {cx + 3, cy + 9}, tint, 1.5f);
            }
            // Clip by measured pixels, retaining UTF-8 code point boundaries.
            std::string name = filename;
            bool shortened = false;
            while (!name.empty() && ImGui::CalcTextSize((name + (shortened ? "..." : "")).c_str()).x > size.x - 16) {
                size_t pos = name.size() - 1;
                while (pos > 0 && (static_cast<unsigned char>(name[pos]) & 0xC0) == 0x80) --pos;
                name.resize(pos);
                shortened = true;
            }
            if (shortened) name += "...";
            draw->AddText({min.x + 8, max.y - 38}, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            std::string label = type.label;
            if (label.size() > 2) label = label.substr(1, label.size() - 2);
            draw->AddText({min.x + 8, max.y - 19}, ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
            draw->AddRect(min, max, ImGui::GetColorU32(selected ? EditorPalette::AccentHover : EditorPalette::Border), 4);
            draw->PopClipRect();
            if (hovered) ImGui::SetTooltip("%s\n%s", filename.c_str(), path.string().c_str());

            // Keep payload and context actions attached to the whole card.
            if (!isDir && ImGui::BeginDragDropSource()) {
                UUID assetHandle = AssetManager::GetHandleFromFilePath(path);
                if (assetHandle.IsValid()) {
                    ImGui::SetDragDropPayload("ASTRAL_ASSET", &assetHandle, sizeof(UUID));
                    ImGui::TextUnformatted(filename.c_str());
                } else ImGui::TextDisabled("Kayitsiz Asset");
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
                m_SelectedItem = path;
                ImGui::TextDisabled("%s", filename.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Explorer'da Goster")) {
                    std::string cmd = "explorer.exe /select,\"" + path.string() + "\"";
                    system(cmd.c_str());
                }
                if (ImGui::MenuItem("Sil")) {
                    try { std::filesystem::remove_all(path); } catch (...) {}
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        };
        for (const auto& entry : directories) renderItem(entry, true);
        for (const auto& entry : files) renderItem(entry, false);
        ImGui::EndTable();
    }
    if (!navigateAfterGrid.empty()) NavigateTo(navigateAfterGrid);
    // Boş alana sağ tık bağlam menüsü
    if (ImGui::BeginPopupContextWindow("ContentGridBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Yeni Klasor")) {
            m_CreatingNewFolder = true;
        }
        if (ImGui::MenuItem("Explorer'da Ac")) {
            std::string cmd = "explorer.exe \"" + m_CurrentDirectory.string() + "\"";
            system(cmd.c_str());
        }
        if (ImGui::MenuItem("Yenile")) {
            // Döngü bir sonraki karede otomatik yeniler
        }
        ImGui::EndPopup();
    }
}

void ContentBrowser::DrawFooter() {
    ImGui::SetNextItemWidth(90.0f);
    ImGui::SliderFloat("##Zoom", &m_ThumbnailSize, 72.0f, 140.0f, "%.0f px");
    ImGui::SameLine();
    size_t count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
            (void)entry;
            count++;
        }
    } catch (...) {}

    if (m_SelectedItem.empty()) {
        ImGui::TextDisabled("Toplam: %zu oge", count);
    } else {
        std::string selectedName = m_SelectedItem.filename().string();
        ImGui::TextDisabled("Toplam: %zu oge  |  Secili: %s", count, selectedName.c_str());
    }
}

} // namespace Astral


