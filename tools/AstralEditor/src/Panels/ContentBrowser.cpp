#include "Astral/Editor/Panels/ContentBrowser.hpp"
#include "Astral/Project/Project.hpp"

#include <imgui.h>
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    ImGui::Begin("Varlik Tarayicisi (Content Browser)");

    DrawToolbar();
    ImGui::Separator();

    // İki bölmeli yerleşim: Sol Klasör Ağacı / Sağ Varlık Grid Görünümü
    if (ImGui::BeginTable("ContentBrowserLayoutTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("TreeColumn", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("GridColumn", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();

        // ── Sol Sütun: Klasör Ağacı ve Hızlı Erişim ─────────────────
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("DirectoryTreeChild", ImVec2(0, -28), false);

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
        ImGui::BeginChild("ContentGridChild", ImVec2(0, -28), false);
        DrawContentGrid();
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::Separator();
    DrawFooter();

    ImGui::End();
    ImGui::PopStyleVar();
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
    ImGui::Spacing();
    ImGui::SameLine();

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

    // Sağ tarafa Arama ve Zoom kaydırıcı
    float rightSideWidth = 320.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    if (availWidth > rightSideWidth) {
        ImGui::SameLine(ImGui::GetWindowWidth() - rightSideWidth - 16.0f);
    } else {
        ImGui::SameLine();
    }

    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##SearchFilter", "Ara...", m_SearchFilter, sizeof(m_SearchFilter));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##Zoom", &m_ThumbnailSize, 56.0f, 110.0f, "%.0fpx");

    ImGui::SameLine();
    if (ImGui::Button("+ Klasor")) {
        m_CreatingNewFolder = true;
    }
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

    // Grid düzeni hesaplamaları
    float cellSize = m_ThumbnailSize + m_Padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = static_cast<int>(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, nullptr, false);

    auto renderItem = [this](const std::filesystem::directory_entry& entry, bool isDir) {
        const auto& path = entry.path();
        std::string filename = path.filename().string();
        FileTypeInfo typeInfo = GetFileTypeInfo(path, isDir);

        ImGui::PushID(filename.c_str());

        bool isSelected = (m_SelectedItem == path);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.36f, 0.67f, 0.60f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 0.40f));
        }

        // Kart/Kutu düğmesi
        ImVec2 buttonSize = ImVec2(m_ThumbnailSize, m_ThumbnailSize * 0.70f);
        if (ImGui::Button("##ItemCard", buttonSize)) {
            m_SelectedItem = path;
        }

        // Çift tıklama algılama
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (isDir) {
                NavigateTo(path);
                ImGui::PopStyleColor();
                ImGui::PopID();
                return;
            }
        }

        // Kart üstüne özel çizim (Tür rozeti ve renk)
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 btnMin = ImGui::GetItemRectMin();

        ImU32 badgeColor = IM_COL32(
            static_cast<int>(typeInfo.color.x * 255),
            static_cast<int>(typeInfo.color.y * 255),
            static_cast<int>(typeInfo.color.z * 255),
            220
        );

        // Ortalanmış rozet metni
        ImVec2 textSize = ImGui::CalcTextSize(typeInfo.label);
        ImVec2 textPos = ImVec2(
            btnMin.x + (buttonSize.x - textSize.x) * 0.5f,
            btnMin.y + (buttonSize.y - textSize.y) * 0.5f - 4.0f
        );
        drawList->AddText(textPos, badgeColor, typeInfo.label);

        // Kart altı dosya adı (Kırpılmış)
        std::string displayName = filename;
        if (displayName.length() > 14) {
            displayName = displayName.substr(0, 11) + "...";
        }
        ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (buttonSize.x - nameSize.x) * 0.5f);
        ImGui::TextUnformatted(displayName.c_str());

        // Sağ tık bağlam menüsü (Context Menu)
        if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
            m_SelectedItem = path;
            ImGui::TextDisabled("%s", filename.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Explorer'da Goster")) {
                std::string cmd = "explorer.exe /select,\"" + path.string() + "\"";
                system(cmd.c_str());
            }

            if (ImGui::MenuItem("Sil")) {
                try {
                    std::filesystem::remove_all(path);
                } catch (...) {}
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor();
        ImGui::PopID();
        ImGui::NextColumn();
    };

    // Önce klasörleri, sonra dosyaları çiz
    for (const auto& dir : directories) {
        renderItem(dir, true);
    }
    for (const auto& file : files) {
        renderItem(file, false);
    }

    ImGui::Columns(1);

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
