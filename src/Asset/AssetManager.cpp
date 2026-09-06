#include "Astral/Asset/AssetManager.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Astral {

    std::unordered_map<UUID, AssetMetadata> AssetManager::s_Registry;
    std::filesystem::path AssetManager::s_AssetDirectory;

    void AssetManager::Init(const std::filesystem::path& assetDirectory) {
        if (!std::filesystem::exists(assetDirectory) && std::filesystem::exists(std::filesystem::path("..") / assetDirectory)) {
            s_AssetDirectory = std::filesystem::path("..") / assetDirectory;
        } else {
            s_AssetDirectory = assetDirectory;
        }
        RefreshRegistry();
        std::cout << "[AssetManager] Baslatildi. Toplam " << s_Registry.size() << " asset kaydedildi.\n";
    }

    void AssetManager::Shutdown() {
        s_Registry.clear();
    }

    void AssetManager::RefreshRegistry() {
        if (!std::filesystem::exists(s_AssetDirectory)) {
            std::filesystem::create_directories(s_AssetDirectory);
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(s_AssetDirectory)) {
            if (entry.is_directory()) continue;

            std::filesystem::path filepath = entry.path();
            
            // Eğer dosya zaten bir .meta dosyasıysa atla, onu asıl dosyayı işlerken okuyacağız
            if (filepath.extension() == ".meta") continue;

            // Gizli dosyaları (.gitignore vb.) atla
            if (!filepath.filename().empty() && filepath.filename().string()[0] == '.') continue;

            std::filesystem::path metaPath = filepath.string() + ".meta";

            if (std::filesystem::exists(metaPath)) {
                // Meta dosyası var, oku ve Registry'e ekle
                AssetMetadata metadata;
                if (ReadMetaFile(metaPath, metadata)) {
                    metadata.FilePath = filepath;
                    s_Registry[metadata.Handle] = metadata;
                } else {
                    std::cerr << "[AssetManager] UYARI: Bozuk meta dosyasi: " << metaPath.string() << "\n";
                }
            } else {
                // Meta dosyası yok, bu yeni bir asset! İçe aktar.
                ImportAsset(filepath);
            }
        }
    }

    UUID AssetManager::ImportAsset(const std::filesystem::path& filepath) {
        AssetMetadata metadata;
        metadata.Handle = UUID(); // Yeni ID oluştur
        metadata.Type = GetAssetTypeFromFileExtension(filepath.extension());
        metadata.FilePath = filepath;

        std::filesystem::path metaPath = filepath.string() + ".meta";
        WriteMetaFile(metaPath, metadata);

        s_Registry[metadata.Handle] = metadata;
        std::cout << "[AssetManager] Yeni Asset import edildi: " << filepath.filename().string() << " (ID: " << metadata.Handle << ")\n";

        return metadata.Handle;
    }

    void AssetManager::WriteMetaFile(const std::filesystem::path& metaPath, const AssetMetadata& metadata) {
        std::ofstream out(metaPath);
        if (out.is_open()) {
            out << "UUID: " << metadata.Handle << "\n";
            out << "Type: " << static_cast<uint16_t>(metadata.Type) << "\n";
            out.close();
        }
    }

    bool AssetManager::ReadMetaFile(const std::filesystem::path& metaPath, AssetMetadata& outMetadata) {
        std::ifstream in(metaPath);
        if (!in.is_open()) return false;

        std::string line;
        while (std::getline(in, line)) {
            auto uuidPos = line.find("UUID: ");
            if (uuidPos != std::string::npos) {
                outMetadata.Handle = std::stoull(line.substr(uuidPos + 6));
            } else {
                auto typePos = line.find("Type: ");
                if (typePos != std::string::npos) {
                    outMetadata.Type = static_cast<AssetType>(std::stoi(line.substr(typePos + 6)));
                }
            }
        }
        return outMetadata.Handle.IsValid(); 
    }

    AssetType AssetManager::GetAssetTypeFromFileExtension(const std::filesystem::path& extension) {
        std::string ext = extension.string();
        for (auto& c : ext) c = tolower(c); // Küçük harfe çevir

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr") return AssetType::Texture2D;
        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") return AssetType::Mesh;
        if (ext == ".astral") return AssetType::Scene;
        if (ext == ".spv" || ext == ".glsl") return AssetType::Shader;
        
        return AssetType::None;
    }

    const AssetMetadata& AssetManager::GetMetadata(UUID handle) {
        static AssetMetadata s_NullMetadata;
        auto it = s_Registry.find(handle);
        if (it != s_Registry.end())
            return it->second;
        return s_NullMetadata;
    }

    std::filesystem::path AssetManager::GetFileSystemPath(UUID handle) {
        return GetMetadata(handle).FilePath;
    }

    UUID AssetManager::GetHandleFromFilePath(const std::filesystem::path& filepath) {
        std::error_code ec;
        for (const auto& [uuid, metadata] : s_Registry) {
            if (metadata.FilePath == filepath) {
                return uuid;
            }
            if (std::filesystem::equivalent(metadata.FilePath, filepath, ec)) {
                return uuid;
            }
        }
        return 0; // Geçersiz UUID
    }

} // namespace Astral
