#include "Astral/Project/Project.hpp"
#include "Astral/Project/ProjectSerializer.hpp"

#include <iostream>
#include <system_error>

namespace Astral {

std::shared_ptr<Project> Project::NewProject(const std::filesystem::path& directory, const std::string& name) {
    std::error_code ec;
    std::filesystem::path absDir = std::filesystem::absolute(directory, ec);
    if (ec) {
        absDir = directory;
    }

    // Proje ve varlik klasorlerini olustur
    std::filesystem::create_directories(absDir / "assets" / "scenes", ec);

    ProjectConfig config;
    config.name = name.empty() ? "Untitled Project" : name;
    config.projectFileName = config.name + ".astralproj";
    config.projectDirectory = absDir;
    config.assetDirectory = "assets";
    config.startScene = "assets/scenes/untitled.astral";

    auto project = std::make_shared<Project>(std::move(config));

    std::filesystem::path manifestPath = absDir / project->GetConfig().projectFileName;
    if (ProjectSerializer::Serialize(project, manifestPath)) {
        std::cout << "[Astral::Project] Yeni proje olusturuldu ve kaydedildi: " << manifestPath.string() << "\n";
    } else {
        std::cerr << "[Astral::Project] UYARI: Yeni proje dosyasi yazilamadi: " << manifestPath.string() << "\n";
    }

    SetActive(project);
    return project;
}

std::shared_ptr<Project> Project::LoadProject(const std::filesystem::path& projectFilePath) {
    auto project = std::make_shared<Project>();
    if (!ProjectSerializer::Deserialize(project, projectFilePath)) {
        std::cerr << "[Astral::Project] HATA: Proje yuklenemedi: " << projectFilePath.string() << "\n";
        return nullptr;
    }

    std::cout << "[Astral::Project] Proje basariyla yuklendi: " << project->GetConfig().name 
              << " (" << projectFilePath.string() << ")\n";

    SetActive(project);
    return project;
}

bool Project::SaveActive(const std::filesystem::path& path) {
    if (!s_ActiveProject) {
        std::cerr << "[Astral::Project] Kaydedilecek aktif proje yok!\n";
        return false;
    }

    std::filesystem::path savePath = path;
    if (savePath.empty()) {
        savePath = s_ActiveProject->GetProjectDirectory() / s_ActiveProject->GetConfig().projectFileName;
    }

    return ProjectSerializer::Serialize(s_ActiveProject, savePath);
}

std::filesystem::path Project::GetRelativePath(const std::filesystem::path& absolutePath) const {
    std::error_code ec;
    auto rel = std::filesystem::relative(absolutePath, GetAssetDirectory(), ec);
    if (ec) {
        return absolutePath;
    }
    return rel;
}

} // namespace Astral
