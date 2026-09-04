#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Astral {

/// Unreal / Unity tarzı iki bölmeli ve grid kart görünümlü Varlık Tarayıcısı (Content Browser)
class ContentBrowser {
public:
    ContentBrowser();
    ~ContentBrowser() = default;

    /// Paneli ImGui arayüzünde çizer
    void Draw();

    /// Göz atılacak kök dizini değiştirir
    void SetBaseDirectory(const std::filesystem::path& path);

    /// Seçili öğenin yolunu döndürür
    [[nodiscard]] const std::filesystem::path& GetSelectedItem() const { return m_SelectedItem; }

private:
    void DrawToolbar();
    void DrawDirectoryTree(const std::filesystem::path& path);
    void DrawContentGrid();
    void DrawFooter();

    void NavigateTo(const std::filesystem::path& path);
    void NavigateBack();
    void NavigateForward();

private:
    std::filesystem::path m_BaseDirectory;
    std::filesystem::path m_CurrentDirectory;
    std::filesystem::path m_SelectedItem;

    // Navigasyon geçmişi
    std::vector<std::filesystem::path> m_History;
    int m_HistoryIndex = -1;

    // Arama ve filtreleme
    char m_SearchFilter[128] = "";

    // Kart / Thumbnail boyutu
    float m_ThumbnailSize = 78.0f;
    float m_Padding = 12.0f;

    // Yeni klasör oluşturma durumu
    bool m_CreatingNewFolder = false;
    char m_NewFolderName[64] = "YeniKlasor";
};

} // namespace Astral
