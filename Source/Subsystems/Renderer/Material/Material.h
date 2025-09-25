#pragma once

#include "../../Asset/AssetHandle.h"
#include "../RendererTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <mutex>
#include "../Shaders/IShader.h"

namespace AstralEngine {

// Forward declarations
class ITexture;         // Generic texture interface
class IShader;          // Generic shader interface
class AssetManager;     // Asset management system
class MaterialManager;  // Material management system

/**
 * @enum MaterialType
 * @brief Desteklenen materyal tipleri
 */
enum class MaterialType {
    PBR = 0,        ///< Physically Based Rendering
    Unlit,          ///< Işıklandırma hesaplaması olmayan
    Skybox,         ///< Skybox materyali
    Custom          ///< Özel shader kullanan materyal
};

/**
 * @enum TextureType
 * @brief Materyalde kullanılan texture tipleri
 */
enum class TextureType {
    Albedo = 0,     ///< Base color texture
    Normal,         ///< Normal map
    Metallic,       ///< Metallic map
    Roughness,      ///< Roughness map
    AO,             ///< Ambient Occlusion map
    Emissive,       ///< Emissive map
    Opacity,        ///< Opacity/Alpha map
    Displacement,   ///< Displacement/Height map
    Custom          ///< Özel texture
};

/**
 * @struct MaterialProperties
 * @brief Materyalin fiziksel özellikleri
 */
struct MaterialProperties {
    // PBR properties
    glm::vec3 baseColor = glm::vec3(1.0f);      ///< Base color (RGB)
    float metallic = 0.0f;                      ///< Metallic value (0.0 - 1.0)
    float roughness = 0.5f;                     ///< Roughness value (0.0 - 1.0)
    float ao = 1.0f;                            ///< Ambient occlusion (0.0 - 1.0)
    
    // Emissive properties
    glm::vec3 emissiveColor = glm::vec3(0.0f);  ///< Emissive color (RGB)
    float emissiveIntensity = 0.0f;             ///< Emissive intensity
    
    // Transparency
    float opacity = 1.0f;                       ///< Opacity/Alpha (0.0 - 1.0)
    bool transparent = false;                   ///< Transparent materyal mi?
    
    // Rendering flags
    bool doubleSided = false;                   ///< Double-sided rendering
    bool wireframe = false;                     ///< Wireframe rendering
    
    // UV properties
    float tilingX = 1.0f;                       ///< X axis tiling
    float tilingY = 1.0f;                       ///< Y axis tiling
    float offsetX = 0.0f;                       ///< X axis offset
    float offsetY = 0.0f;                       ///< Y axis offset
};

/**
 * @struct TextureSlot
 * @brief Materyaldeki texture slot bilgisi
 */
struct TextureSlot {
    std::shared_ptr<ITexture> texture;         ///< Generic texture interface
    TextureType type;                           ///< Texture tipi
    std::string name;                           ///< Texture adı
    uint32_t binding = 0;                       ///< Shader binding index
    bool enabled = true;                        ///< Texture aktif mi?
};

/**
 * @class Material
 * @brief Pure data container for material properties and resources
 *
 * This class serves as a pure data container that holds material properties,
 * textures, and shader information. It contains no rendering logic or
 * Vulkan-specific dependencies, making it a clean data structure that
 * can be used by various rendering backends.
 *
 * The Material class stores:
 * - Material properties (colors, PBR parameters, transparency, etc.)
 * - Texture slots with their types and binding information
 * - Shader asset handles for vertex and fragment shaders
 * - Material metadata (name, type, initialization state)
 */
class Material {
public:
    /**
     * @brief Materyal yapılandırma parametreleri
     */
    struct Config {
        MaterialType type = MaterialType::PBR;  ///< Materyal tipi
        std::string name = "UnnamedMaterial";    ///< Materyal adı
        AssetHandle vertexShaderHandle;          ///< Vertex shader handle
        AssetHandle fragmentShaderHandle;        ///< Fragment shader handle
    };

    Material();
    ~Material();

    // Non-copyable
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    // Yaşam döngüsü
    /**
     * @brief Materyali başlatır (basitleştirilmiş versiyon)
     *
     * Bu metot, materyali saf bir veri konteyneri olarak başlatır:
     * - Materyal tipini ve adını ayarlar
     * - Shader handle'larını ayarlar
     * - Texture slot'larını ve map'lerini başlatır
     * - Materyal özelliklerini varsayılan değerlere ayarlar
     *
     * @param config Materyal yapılandırma parametreleri
     * @return true Başarılı olursa
     * @return false Hata olursa
     */
    bool Initialize(const Config& config);
    void Shutdown();

    // Materyal özellikleri
    void SetProperties(const MaterialProperties& props);
    const MaterialProperties& GetProperties() const { return m_properties; }
    MaterialProperties& GetProperties() { return m_properties; }

    // Texture yönetimi
    void SetTexture(TextureType type, std::shared_ptr<ITexture> texture);
    std::shared_ptr<ITexture> GetTexture(TextureType type) const;
    void RemoveTexture(TextureType type);
    bool HasTexture(TextureType type) const;
    
    // Texture slot erişimi
    const std::vector<TextureSlot>& GetTextureSlots() const { return m_textureSlots; }
    const TextureSlot* GetTextureSlot(TextureType type) const;

    // Materyal bilgileri
    MaterialType GetType() const { return m_type; }
    const std::string& GetName() const { return m_name; }
    bool IsInitialized() const { return m_isInitialized; }
    bool IsTransparent() const { return m_properties.transparent; }
    
    // Shader handle erişimi
    AssetHandle GetVertexShaderHandle() const { return m_vertexShaderHandle; }
    AssetHandle GetFragmentShaderHandle() const { return m_fragmentShaderHandle; }

    // Shader caching and validation methods
    /**
     * @brief Get the cached shader hash for comparison and validation
     * @return 64-bit hash of the current shader combination
     */
    uint64_t GetShaderHash() const;

    /**
     * @brief Get the cached vertex shader object
     * @return Pointer to the vertex shader, nullptr if not loaded
     */
    std::shared_ptr<IShader> GetVertexShader() const;

    /**
     * @brief Get the cached fragment shader object
     * @return Pointer to the fragment shader, nullptr if not loaded
     */
    std::shared_ptr<IShader> GetFragmentShader() const;

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

    // MaterialManager erişimi
    void SetMaterialManager(MaterialManager* manager) { m_materialManager = manager; }
    MaterialManager* GetMaterialManager() const { return m_materialManager; }

private:
    // Helper methods
    uint32_t GetTextureBinding(TextureType type) const;
    std::string GetTextureName(TextureType type) const;
    void SetError(const std::string& error);

    /**
     * @brief Lazy loading method for shaders to improve performance
     *
     * This method loads and caches the vertex and fragment shaders when they are
     * first requested, rather than loading them during material initialization.
     * This approach improves startup time and memory usage by only loading
     * shaders when they are actually needed.
     *
     * The method will:
     * - Load shaders from asset handles if not already loaded
     * - Cache the loaded shader objects for future use
     * - Calculate and cache the combined shader hash
     * - Set the m_shadersLoaded flag to true on success
     *
     * @note This method is thread-safe and can be called from const methods
     * @note If shader loading fails, the cached pointers remain null
     */
    void LoadShadersIfNeeded() const;

    // Member değişkenler
    
    // Materyal özellikleri
    MaterialType m_type;
    std::string m_name;
    MaterialProperties m_properties;
    
    // Shader Handles
    AssetHandle m_vertexShaderHandle;
    AssetHandle m_fragmentShaderHandle;
    
    // Texture'lar
    std::vector<TextureSlot> m_textureSlots;
    std::unordered_map<TextureType, size_t> m_textureMap;

    // Shader caching system
    mutable std::shared_ptr<IShader> m_vertexShader;    ///< Cached vertex shader object
    mutable std::shared_ptr<IShader> m_fragmentShader;  ///< Cached fragment shader object
    mutable uint64_t m_shaderHash = 0;                   ///< Cached shader hash for validation
    mutable bool m_shadersLoaded = false;               ///< Flag indicating if shaders are loaded
    mutable std::mutex m_shaderLoadMutex;               ///< Mutex for thread-safe shader loading

    bool m_isInitialized = false;
    std::string m_lastError;

    // MaterialManager reference for shader loading
    MaterialManager* m_materialManager = nullptr;
};


} // namespace AstralEngine
