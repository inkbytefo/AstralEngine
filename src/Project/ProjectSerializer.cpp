#include "Astral/Project/ProjectSerializer.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <system_error>

namespace Astral {

namespace {

// Yardımcı dize temizleme fonksiyonları
std::string Trim(const std::string& str) {
    auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string ExtractJsonStringValue(const std::string& line) {
    auto colonPos = line.find(':');
    if (colonPos == std::string::npos) return "";

    std::string val = line.substr(colonPos + 1);
    val = Trim(val);

    // Sondaki virgülü kaldır
    if (!val.empty() && val.back() == ',') {
        val.pop_back();
        val = Trim(val);
    }

    // Tırnak işaretlerini kaldır ("...")
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        return val.substr(1, val.size() - 2);
    }
    return val;
}

} // namespace

bool ProjectSerializer::Serialize(const std::filesystem::path& filepath) {
    return Serialize(m_Project, filepath);
}

bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath) {
    return Deserialize(m_Project, filepath);
}

bool ProjectSerializer::Serialize(const std::shared_ptr<Project>& project, const std::filesystem::path& filepath) {
    if (!project) {
        std::cerr << "[Astral::ProjectSerializer] Gecersiz proje pointer'i!\n";
        return false;
    }

    // Hedef klasörün var olduğundan emin ol
    std::error_code ec;
    std::filesystem::path parentDir = filepath.parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        std::filesystem::create_directories(parentDir, ec);
    }

    std::ofstream out(filepath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[Astral::ProjectSerializer] Dosya yazmak icin acilamadi: " << filepath.string() << "\n";
        return false;
    }

    const auto& config = project->GetConfig();

    out << "{\n";
    out << "  \"Project\": {\n";
    out << "    \"Name\": \"" << config.name << "\",\n";
    out << "    \"EngineVersion\": \"1.0.0\",\n";
    out << "    \"AssetDirectory\": \"" << config.assetDirectory.generic_string() << "\",\n";
    out << "    \"StartScene\": \"" << config.startScene.generic_string() << "\"\n";
    out << "  }\n";
    out << "}\n";

    return true;
}

bool ProjectSerializer::Deserialize(std::shared_ptr<Project>& project, const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "[Astral::ProjectSerializer] Proje dosyasi bulunamadi: " << filepath.string() << "\n";
        return false;
    }

    std::ifstream in(filepath);
    if (!in.is_open()) {
        std::cerr << "[Astral::ProjectSerializer] Dosya okunamadi: " << filepath.string() << "\n";
        return false;
    }

    if (!project) {
        project = std::make_shared<Project>();
    }

    ProjectConfig config;
    std::error_code ec;
    config.projectFileName = filepath.filename();
    config.projectDirectory = std::filesystem::canonical(filepath.parent_path(), ec);
    if (ec) {
        config.projectDirectory = std::filesystem::absolute(filepath.parent_path());
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.find("\"Name\"") != std::string::npos) {
            config.name = ExtractJsonStringValue(trimmed);
        } else if (trimmed.find("\"AssetDirectory\"") != std::string::npos) {
            config.assetDirectory = ExtractJsonStringValue(trimmed);
        } else if (trimmed.find("\"StartScene\"") != std::string::npos) {
            config.startScene = ExtractJsonStringValue(trimmed);
        }
    }

    if (config.name.empty()) {
        config.name = filepath.stem().string();
    }
    if (config.assetDirectory.empty()) {
        config.assetDirectory = "assets";
    }

    project->GetConfig() = std::move(config);
    return true;
}

} // namespace Astral
