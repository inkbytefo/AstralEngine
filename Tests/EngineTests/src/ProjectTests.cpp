#include "TestFramework.hpp"
#include "Astral/Project/Project.hpp"
#include "Astral/Project/ProjectSerializer.hpp"

#include <iostream>
#include <filesystem>

namespace Astral::Test {

void RunProjectTests() {
    const std::string suite = "ProjectSuite";
    std::cout << "  [INFO] Project & ProjectSerializer testleri baslatiliyor...\n";

    std::filesystem::path testDir = std::filesystem::temp_directory_path() / "astral_test_project";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    // 1. Yeni Proje Olusturma (NewProject)
    {
        auto project = Project::NewProject(testDir, "AstroTestProject");
        TEST_CHECK(suite, "NewProject return non-null", project != nullptr);
        TEST_CHECK(suite, "NewProject sets active", Project::GetActive() == project);
        TEST_CHECK(suite, "Project name matches", project->GetConfig().name == "AstroTestProject");
        TEST_CHECK(suite, "Project manifest file exists", std::filesystem::exists(testDir / "AstroTestProject.astralproj"));
        TEST_CHECK(suite, "Project assets dir created", std::filesystem::exists(testDir / "assets"));
        TEST_CHECK(suite, "Project scenes dir created", std::filesystem::exists(testDir / "assets" / "scenes"));
    }

    // 2. Yol Cozumleme Yardimcilari (Path Resolvers)
    {
        auto project = Project::GetActive();
        TEST_CHECK(suite, "Active project available", project != nullptr);

        auto assetDir = project->GetAssetDirectory();
        TEST_CHECK(suite, "Asset directory resolves correctly", assetDir == (testDir / "assets"));

        auto fullMeshPath = project->GetAssetFileSystemPath("models/spaceship.obj");
        TEST_CHECK(suite, "GetAssetFileSystemPath resolves correctly", fullMeshPath == (testDir / "assets" / "models" / "spaceship.obj"));

        auto relative = project->GetRelativePath(fullMeshPath);
        // "models/spaceship.obj" veya Windows slash uyumluluğu
        bool isExpectedRel = (relative.generic_string() == "models/spaceship.obj");
        TEST_CHECK(suite, "GetRelativePath converts absolute to asset relative", isExpectedRel);
    }

    // 3. Serilestirme ve Yeniden Yukleme (Save & Load)
    {
        auto project = Project::GetActive();
        project->GetConfig().startScene = "assets/scenes/Level1.astral";
        bool saved = Project::SaveActive();
        TEST_CHECK(suite, "SaveActive returns true", saved);

        // Aktif projeyi sıfırla ve dosyadan geri yükle
        Project::SetActive(nullptr);
        TEST_CHECK(suite, "Project reset to null", Project::GetActive() == nullptr);

        auto loadedProj = Project::LoadProject(testDir / "AstroTestProject.astralproj");
        TEST_CHECK(suite, "LoadProject succeeds", loadedProj != nullptr);
        TEST_CHECK(suite, "LoadProject sets active", Project::GetActive() == loadedProj);
        TEST_CHECK(suite, "Loaded name matches", loadedProj->GetConfig().name == "AstroTestProject");
        TEST_CHECK(suite, "Loaded startScene matches", loadedProj->GetConfig().startScene.generic_string() == "assets/scenes/Level1.astral");
    }

    // 4. Hata Yonetimi (Error Handling)
    {
        auto invalidProj = Project::LoadProject(testDir / "non_existent_file_9999.astralproj");
        TEST_CHECK(suite, "Load invalid project returns nullptr", invalidProj == nullptr);
    }

    // Test sonrasi temizlik
    std::filesystem::remove_all(testDir, ec);
    std::cout << "  [INFO] Project testleri basariyla tamamlandi.\n";
}

} // namespace Astral::Test
