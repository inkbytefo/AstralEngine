#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace Astral {

/**
 * @brief Projenin temel yapilandirma parametrelerini saklayan veri yapisi.
 */
struct ProjectConfig {
    std::string name = "Untitled Project";
    std::filesystem::path projectFileName;                  // or. "Sandbox.astralproj"
    std::filesystem::path projectDirectory;                 // Mutlak kok dizin
    std::filesystem::path assetDirectory = "assets";         // Koke goreceli varlik dizini
    std::filesystem::path startScene = "assets/scenes/untitled.astral"; // Baslangic sahnesi
};

/**
 * @brief Motor ve Editor duzeyinde aktif projeyi yoneten cekirdek sinif.
 *
 * Unreal Engine (.uproject) ve Godot (project.godot) standartlarinda oldugu gibi
 * varlik dizini cozumleme, sahne yollari ve proje ayarlarini tek bir merkezden yonetir.
 */
class Project {
public:
    Project() = default;
    explicit Project(ProjectConfig config) : m_Config(std::move(config)) {}
    ~Project() = default;

    /// Aktif projeyi dondurur
    [[nodiscard]] static std::shared_ptr<Project> GetActive() noexcept {
        return s_ActiveProject;
    }

    /// Aktif projeyi ayarlar
    static void SetActive(std::shared_ptr<Project> project) noexcept {
        s_ActiveProject = std::move(project);
    }

    /// Belirtilen dizinde yeni bir proje olusturur ve aktif yapar
    static std::shared_ptr<Project> NewProject(const std::filesystem::path& directory, const std::string& name = "Untitled");

    /// Diskteki bir .astralproj dosyasini yukler ve aktif yapar
    static std::shared_ptr<Project> LoadProject(const std::filesystem::path& projectFilePath);

    /// Aktif projeyi diske kaydeder (path bos ise mevcut proje dosyasina yazar)
    static bool SaveActive(const std::filesystem::path& path = "");

    /// Proje yapilandirmasina erisim
    [[nodiscard]] const ProjectConfig& GetConfig() const noexcept { return m_Config; }
    [[nodiscard]] ProjectConfig& GetConfig() noexcept { return m_Config; }

    /// Projenin mutlak kok dizinini dondurur
    [[nodiscard]] std::filesystem::path GetProjectDirectory() const {
        return m_Config.projectDirectory;
    }

    /// Projenin mutlak varlik (assets) dizinini dondurur
    [[nodiscard]] std::filesystem::path GetAssetDirectory() const {
        return m_Config.projectDirectory / m_Config.assetDirectory;
    }

    /// Goreceli bir varlik yolunu mutlak dosya sistemi yoluna cevirir
    [[nodiscard]] std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& relativePath) const {
        return GetAssetDirectory() / relativePath;
    }

    /// Mutlak bir dosya yolunu varlik dizinine gore goreceli yola cevirir
    [[nodiscard]] std::filesystem::path GetRelativePath(const std::filesystem::path& absolutePath) const;

private:
    ProjectConfig m_Config;
    inline static std::shared_ptr<Project> s_ActiveProject = nullptr;
};

} // namespace Astral
