#pragma once

#include "Astral/Project/Project.hpp"
#include <filesystem>
#include <memory>

namespace Astral {

/**
 * @brief .astralproj manifestolarini diske yazan ve diskten yukleyen serilestirme modulu.
 *
 * Standart, temiz ve tasinabilir JSON benzeri metin formatinda proje yapilandirmasini yonetir.
 */
class ProjectSerializer {
public:
    explicit ProjectSerializer(std::shared_ptr<Project> project)
        : m_Project(std::move(project)) {}

    [[nodiscard]] bool Serialize(const std::filesystem::path& filepath);
    [[nodiscard]] bool Deserialize(const std::filesystem::path& filepath);

    [[nodiscard]] static bool Serialize(const std::shared_ptr<Project>& project, const std::filesystem::path& filepath);
    [[nodiscard]] static bool Deserialize(std::shared_ptr<Project>& project, const std::filesystem::path& filepath);

private:
    std::shared_ptr<Project> m_Project;
};

} // namespace Astral
