#pragma once

#include "../../Asset/AssetManager.h"
#include "../../Asset/AssetData.h"
#include "../Buffers/VulkanTexture.h"
#include "../Material/Material.h"
#include "../Core/VulkanDevice.h"
#include "../GraphicsDevice.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <glm/glm.hpp>

namespace AstralEngine {

// Forward declarations
class Texture;

/**
 * @enum TextureFormat
 * @brief Desteklenen texture formatları
 */
enum class TextureFormat {
    R8_UNORM = 0,        ///< 8-bit single channel
    R8G8_UNORM,          ///< 8-bit two channel
    R8G8B8_UNORM,        ///< 8-bit RGB
    R8G8B8A8_UNORM,      ///< 8-bit RGBA
    R8G8B8A8_SRGB,       ///< 8-bit RGBA sRGB
    R32_SFLOAT,          ///< 32-bit float single channel
    R32G32_SFLOAT,       ///< 32-bit float two channel
    R32G32B32_SFLOAT,    ///< 32-bit float RGB
    R32G32B32A32_SFLOAT, ///< 32-bit float RGBA
    BC1_UNORM,           ///< DXT1/BC1 compressed
    BC3_UNORM,           ///< DXT5/BC3 compressed
    BC4_UNORM,           ///< BC4 compressed (single channel)
    BC5_UNORM,           ///< BC5 compressed (two channel)
    BC7_UNORM            ///< BC7 compressed (high quality)
};

/**
 * @enum TextureFilter
 * @brief Texture filtreleme modları
 */
enum class TextureFilter {
    Nearest = 0,         ///< Nearest neighbor filtering
    Linear,              ///< Linear filtering
    Bilinear,            ///< Bilinear filtering
    Trilinear            ///< Trilinear filtering
};

/**
 * @enum TextureWrap
 * @brief Texture wrapping modları
 */
enum class TextureWrap {
    Repeat = 0,          ///< Repeat texture
    MirroredRepeat,      ///< Repeat with mirroring
    ClampToEdge,         ///< Clamp to edge
    ClampToBorder,       ///< Clamp to border color
    MirrorClampToEdge    ///< Mirror once then clamp to edge
};

/**
 * @enum TextureType
 * @brief Texture kullanım amaçları
 */
enum class TextureUsageType {
    Albedo = 0,          ///< Base color/diffuse texture
    Normal,              ///< Normal map
    Metallic,            ///< Metallic/roughness map
    Roughness,           ///< Roughness map
    AO,                  ///< Ambient occlusion map
    Emissive,            ///< Emissive/light map
    Height,              ///< Height/displacement map
    Opacity,             ///< Opacity/alpha map
    Environment,         ///< Environment/cubemap texture
    Irradiance,          ///< Irradiance map (for PBR)
    Prefilter,           ///< Prefiltered environment map
    BRDF,                ///< BRDF lookup texture
    Custom               ///< Custom usage
};

/**
 * @struct TextureInfo
 * @brief Texture hakkında meta bilgiler
 */
struct TextureInfo {
    std::string name;                    ///< Texture adı
    std::string filePath;                ///< Dosya yolu
    uint32_t width = 0;                  ///< Genişlik
    uint32_t height = 0;                 ///< Yükseklik
    uint32_t channels = 0;               ///< Kanal sayısı
    uint32_t mipLevels = 1;              ///< Mipmap seviyeleri
    TextureFormat format = TextureFormat::R8G8B8A8_SRGB; ///< Format
    TextureFilter minFilter = TextureFilter::Linear;     ///< Minification filter
    TextureFilter magFilter = TextureFilter::Linear;     ///< Magnification filter
    TextureWrap wrapU = TextureWrap::Repeat;             ///< U axis wrapping
    TextureWrap wrapV = TextureWrap::Repeat;             ///< V axis wrapping
    TextureWrap wrapW = TextureWrap::Repeat;             ///< W axis wrapping
    TextureUsageType usage = TextureUsageType::Custom;   ///< Kullanım amacı
    bool generateMipmaps = true;          ///< Mipmap otomatik oluştur
    bool sRGB = true;                     ///< sRGB color space
    float anisotropy = 16.0f;             ///< Anisotropic filtering level
    glm::vec4 borderColor = glm::vec4(0.0f); ///< Border color for clamp mode
    size_t memorySize = 0;                ///< Bellek kullanımı (bytes)
    uint64_t lastAccessTime = 0;          ///< Son erişim zamanı
};


/**
 * @class TextureManager
 * @brief Texture yönetimi ve önbellekleme sistemi
 * 
 * Bu sınıf, tüm texture'ları yönetir, önbellekte tutar ve
 * texture asset'lerinin yüklenmesini sağlar.
 */
class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    // Non-copyable
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(GraphicsDevice* graphicsDevice, AssetManager* assetManager);
    void Shutdown();
    void Update();

    // Texture oluşturma ve yükleme
    std::shared_ptr<Texture> CreateTexture(const TextureInfo& info);
    std::shared_ptr<Texture> LoadTexture(const std::string& texturePath, TextureUsageType usage = TextureUsageType::Custom);
    std::shared_ptr<Texture> LoadCubemap(const std::vector<std::string>& facePaths);
    std::shared_ptr<Texture> GetTexture(const std::string& textureName) const;
    
    // Procedural texture oluşturma
    std::shared_ptr<Texture> CreateCheckerboard(const std::string& name, uint32_t width, uint32_t height,
                                               uint32_t squareSize, const glm::vec3& color1, const glm::vec3& color2);
    std::shared_ptr<Texture> CreateGradient(const std::string& name, uint32_t width, uint32_t height,
                                           const glm::vec3& startColor, const glm::vec3& endColor, bool horizontal = true);
    std::shared_ptr<Texture> CreateNoise(const std::string& name, uint32_t width, uint32_t height,
                                        float scale, float persistence);
    
    // Texture yönetimi
    void RegisterTexture(const std::string& name, std::shared_ptr<Texture> texture);
    void UnregisterTexture(const std::string& name);
    bool HasTexture(const std::string& name) const;
    void ReloadTexture(const std::string& name);
    
    // Texture atlas yönetimi
    std::shared_ptr<Texture> CreateTextureAtlas(const std::string& name, 
                                               const std::vector<std::shared_ptr<Texture>>& textures,
                                               uint32_t padding = 2);
    bool ExtractFromAtlas(const std::string& atlasName, const std::string& textureName,
                         uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // Varsayılan texture'lar
    std::shared_ptr<Texture> GetDefaultWhiteTexture() const { return m_defaultWhiteTexture; }
    std::shared_ptr<Texture> GetDefaultBlackTexture() const { return m_defaultBlackTexture; }
    std::shared_ptr<Texture> GetDefaultNormalTexture() const { return m_defaultNormalTexture; }
    std::shared_ptr<Texture> GetDefaultCheckerboardTexture() const { return m_defaultCheckerboardTexture; }
    
    // Batch işlemler
    void PreloadTextures(const std::vector<std::string>& texturePaths);
    void UnloadUnusedTextures();
    void OptimizeTextures();
    
    // İstatistikler
    size_t GetTextureCount() const { return m_textures.size(); }
    size_t GetTotalMemoryUsage() const { return m_totalMemoryUsage; }
    std::vector<std::string> GetLoadedTextureNames() const;
    void PrintMemoryUsage() const;

private:
    // Yardımcı metotlar
    bool CreateDefaultTextures();
    void CleanupUnusedTextures();
    void UpdateMemoryUsage();
    std::string GenerateUniqueName(const std::string& baseName) const;
    bool IsTextureExpired(const std::shared_ptr<Texture>& texture, uint64_t currentTime) const;
    
    // Member değişkenler
    GraphicsDevice* m_graphicsDevice = nullptr;
    VulkanDevice* m_device = nullptr; // Keep for backward compatibility during transition
    AssetManager* m_assetManager = nullptr;
    
    // Texture önbelleği
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    
    // Varsayılan texture'lar
    std::shared_ptr<Texture> m_defaultWhiteTexture;
    std::shared_ptr<Texture> m_defaultBlackTexture;
    std::shared_ptr<Texture> m_defaultNormalTexture;
    std::shared_ptr<Texture> m_defaultCheckerboardTexture;
    
    // Bellek yönetimi
    size_t m_totalMemoryUsage = 0;
    uint64_t m_lastCleanupTime = 0;
    static const uint64_t CLEANUP_INTERVAL = 30000; // 30 saniye
    
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

// Global texture manager erişimi
TextureManager* GetTextureManager();

} // namespace AstralEngine
