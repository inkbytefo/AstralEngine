#pragma once
#include "Astral/Asset/Asset.hpp"
#include <filesystem>
#include <unordered_map>

namespace Astral {

    // Bir asset'in bellekteki durumu ve dosya bilgileri
    struct AssetMetadata {
        UUID Handle = 0;
        AssetType Type = AssetType::None;
        std::filesystem::path FilePath;
        bool IsLoaded = false;
    };

    class AssetManager {
    public:
        // Motor başlarken klasörleri tarar ve registry'yi doldurur
        static void Init(const std::filesystem::path& assetDirectory);
        static void Shutdown();

        // Belirli bir klasörü tarayıp .meta dosyalarını okur/oluşturur
        static void RefreshRegistry();

        // Yeni bir dosyayı motora kaydeder ve .meta dosyasını oluşturur
        static UUID ImportAsset(const std::filesystem::path& filepath);

        // Registry Sorguları
        static const AssetMetadata& GetMetadata(UUID handle);
        static std::filesystem::path GetFileSystemPath(UUID handle);
        static UUID GetHandleFromFilePath(const std::filesystem::path& filepath);

        // Şimdilik sadece registry'i dışarıya açıyoruz, ilerde Content Browser kullanacak
        static const std::unordered_map<UUID, AssetMetadata>& GetRegistry() { return s_Registry; }

    private:
        static void WriteMetaFile(const std::filesystem::path& metaPath, const AssetMetadata& metadata);
        static bool ReadMetaFile(const std::filesystem::path& metaPath, AssetMetadata& outMetadata);
        static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension);

    private:
        static std::unordered_map<UUID, AssetMetadata> s_Registry;
        static std::filesystem::path s_AssetDirectory;
    };

} // namespace Astral
