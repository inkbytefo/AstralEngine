#pragma once

#include "../../Core/Logger.h"
#include "../Core/VulkanDevice.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <vulkan/vulkan.h>

namespace AstralEngine {

/**
 * @enum ShaderStage
 * @brief Shader aşamaları
 */
enum class ShaderStage {
    Vertex = 0,         ///< Vertex shader
    TessellationControl, ///< Tessellation control shader
    TessellationEvaluation, ///< Tessellation evaluation shader
    Geometry,           ///< Geometry shader
    Fragment,           ///< Fragment shader
    Compute            ///< Compute shader
};

/**
 * @enum ShaderLanguage
 * @brief Desteklenen shader dilleri
 */
enum class ShaderLanguage {
    GLSL = 0,           ///< OpenGL Shading Language
    HLSL,              ///< High-Level Shading Language
    SLANG,             ///< Slang Shading Language
    SPIRV              ///< SPIR-V (derlenmiş)
};

/**
 * @struct ShaderCompileOptions
 * @brief Shader derleme seçenekleri
 */
struct ShaderCompileOptions {
    ShaderLanguage language = ShaderLanguage::GLSL; ///< Kaynak dil
    bool optimize = true;                          ///< Optimizasyon yap
    bool debugInfo = false;                        ///< Debug bilgisi ekle
    bool validate = true;                          ///< Shader'ı doğrula
    std::vector<std::string> defines;            ///< Preprocessor tanımlamaları
    std::vector<std::string> includePaths;       ///< Include yolları
    std::string entryPoint = "main";             ///< Entry point fonksiyonu
    int optimizationLevel = 3;                    ///< Optimizasyon seviyesi (0-3)
};

/**
 * @struct ShaderCompileResult
 * @brief Shader derleme sonucu
 */
struct ShaderCompileResult {
    bool success = false;                         ///< Derleme başarılı mı?
    std::vector<uint32_t> spirvCode;            ///< Derlenmiş SPIR-V kodu
    std::string errorMessage;                    ///< Hata mesajı
    std::string warningMessage;                  ///< Uyarı mesajı
    uint64_t compileTimeMs = 0;                 ///< Derleme süresi (ms)
    size_t codeSize = 0;                        ///< Kod boyutu (bytes)
    
    explicit operator bool() const { return success; }
};

/**
 * @struct ShaderSourceInfo
 * @brief Shader kaynak kodu bilgileri
 */
struct ShaderSourceInfo {
    std::string filePath;                        ///< Dosya yolu
    ShaderStage stage;                           ///< Shader aşaması
    ShaderLanguage language;                     ///< Shader dili
    std::string sourceCode;                      ///< Kaynak kod
    std::filesystem::file_time_type lastModified; ///< Son değiştirilme zamanı
    bool isValid = false;                        ///< Geçerli mi?
    
    explicit operator bool() const { return isValid; }
};

/**
 * @class ShaderCompiler
 * @brief Shader derleme ve yönetim sistemi
 * 
 * Bu sınıf, GLSL/HLSL shader'larını SPIR-V'e derler,
 * shader include'larını yönetir ve derleme cache'i tutar.
 */
class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Non-copyable
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device);
    void Shutdown();

    // Shader derleme
    ShaderCompileResult CompileShader(const std::string& filePath, ShaderStage stage, 
                                   const ShaderCompileOptions& options = ShaderCompileOptions());
    ShaderCompileResult CompileShaderFromSource(const std::string& sourceCode, ShaderStage stage,
                                               const ShaderCompileOptions& options = ShaderCompileOptions());
    ShaderCompileResult CompileShaderFromSource(const std::string& sourceCode, ShaderStage stage,
                                               const std::string& filePath, 
                                               const ShaderCompileOptions& options = ShaderCompileOptions());

    // Shader program derleme (birden fazla aşama)
    std::vector<ShaderCompileResult> CompileShaderProgram(
        const std::unordered_map<ShaderStage, std::string>& shaderFiles,
        const ShaderCompileOptions& options = ShaderCompileOptions());

    // Include yönetimi
    void AddIncludePath(const std::string& path);
    void RemoveIncludePath(const std::string& path);
    std::vector<std::string> GetIncludePaths() const;
    
    // Macro tanımlama
    void DefineMacro(const std::string& name, const std::string& value = "");
    void UndefineMacro(const std::string& name);
    void ClearMacros();
    std::unordered_map<std::string, std::string> GetMacros() const;

    // Cache yönetimi
    void EnableCache(bool enable);
    void ClearCache();
    void SetCacheDirectory(const std::string& path);
    std::string GetCacheDirectory() const;
    bool IsCacheEnabled() const { return m_cacheEnabled; }

    // Shader doğrulama
    bool ValidateShader(const std::vector<uint32_t>& spirvCode, ShaderStage stage);
    std::string GetShaderReflection(const std::vector<uint32_t>& spirvCode);

    // Kaynak kodu yönetimi
    ShaderSourceInfo LoadShaderSource(const std::string& filePath);
    bool SaveShaderSource(const std::string& filePath, const std::string& sourceCode);
    std::string PreprocessShader(const std::string& sourceCode, const std::string& filePath,
                                const ShaderCompileOptions& options);

    // Hata ayıklama ve bilgi
    std::string GetShaderStageString(ShaderStage stage) const;
    std::string GetShaderLanguageString(ShaderLanguage language) const;
    std::string GetLastErrorMessage() const { return m_lastError; }
    std::string GetLastWarningMessage() const { return m_lastWarning; }

    // İstatistikler
    size_t GetCacheSize() const { return m_cache.size(); }
    uint64_t GetTotalCompileTime() const { return m_totalCompileTime; }
    size_t GetTotalCompiles() const { return m_totalCompiles; }
    void PrintStatistics() const;

private:
    // Yardımcı metotlar
    ShaderCompileResult CompileWithGlslang(const std::string& sourceCode, ShaderStage stage,
                                           const std::string& filePath, 
                                           const ShaderCompileOptions& options);
    ShaderCompileResult CompileWithShaderc(const std::string& sourceCode, ShaderStage stage,
                                           const std::string& filePath,
                                           const ShaderCompileOptions& options);
    ShaderCompileResult CompileWithSlang(const std::string& sourceCode, ShaderStage stage,
                                          const std::string& filePath,
                                          const ShaderCompileOptions& options);
    ShaderCompileResult LoadFromCache(const std::string& cacheKey);
    void SaveToCache(const std::string& cacheKey, const ShaderCompileResult& result);
    std::string GenerateCacheKey(const std::string& filePath, const ShaderCompileOptions& options) const;
    std::string GenerateCacheKey(const std::string& sourceCode, ShaderStage stage,
                                const ShaderCompileOptions& options) const;
    
    VkShaderStageFlagBits ConvertToVkShaderStage(ShaderStage stage) const;
    EShLanguage ConvertToGlslangStage(ShaderStage stage) const;
    shaderc_shader_kind ConvertToShadercKind(ShaderStage stage) const;
    
    std::string ReadFile(const std::string& filePath) const;
    bool WriteFile(const std::string& filePath, const std::vector<uint32_t>& data) const;
    std::string GetFileHash(const std::string& filePath) const;
    std::string GetSourceHash(const std::string& sourceCode, const ShaderCompileOptions& options) const;
    
    void ProcessIncludes(std::string& sourceCode, const std::string& filePath,
                       const ShaderCompileOptions& options, std::vector<std::string>& includedFiles);
    void ProcessMacros(std::string& sourceCode, const ShaderCompileOptions& options);
    
    void SetError(const std::string& error);
    void SetWarning(const std::string& warning);
    void ClearMessages();

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    
    // Include yolları
    std::vector<std::string> m_includePaths;
    
    // Macro tanımlamaları
    std::unordered_map<std::string, std::string> m_macros;
    
    // Cache sistemi
    std::unordered_map<std::string, ShaderCompileResult> m_cache;
    std::string m_cacheDirectory;
    bool m_cacheEnabled = true;
    
    // İstatistikler
    uint64_t m_totalCompileTime = 0;
    size_t m_totalCompiles = 0;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
    
    // Hata mesajları
    std::string m_lastError;
    std::string m_lastWarning;
    
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

/**
 * @class ShaderPreprocessor
 * @brief Shader preprocessor işlemleri
 * 
 * Bu sınıf, shader kaynak kodu üzerinde preprocessor işlemleri yapar:
 * - #include direktiflerini işler
 * - #define macro'larını genişletir
 * - #ifdef/#ifndef koşullarını değerlendirir
 */
class ShaderPreprocessor {
public:
    ShaderPreprocessor();
    ~ShaderPreprocessor();

    // Preprocessing
    std::string Process(const std::string& sourceCode, const std::string& filePath,
                       const std::vector<std::string>& includePaths,
                       const std::unordered_map<std::string, std::string>& macros,
                       std::vector<std::string>& includedFiles);

    // Macro yönetimi
    void DefineMacro(const std::string& name, const std::string& value = "");
    void UndefineMacro(const std::string& name);
    void ClearMacros();

    // Include yönetimi
    void AddIncludePath(const std::string& path);
    void RemoveIncludePath(const std::string& path);
    void ClearIncludePaths();

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    const std::vector<std::string>& GetWarnings() const { return m_warnings; }
    void ClearMessages();

private:
    // Preprocessing yardımcıları
    std::string ProcessIncludes(const std::string& sourceCode, const std::string& filePath,
                               std::vector<std::string>& includedFiles);
    std::string ProcessMacros(const std::string& sourceCode);
    std::string ProcessConditionals(const std::string& sourceCode);
    
    std::string ExpandMacro(const std::string& macroName, 
                           const std::vector<std::string>& arguments = std::vector<std::string>());
    std::string FindIncludeFile(const std::string& includePath, const std::string& currentFilePath);
    std::string ReadIncludeFile(const std::string& filePath);
    
    bool EvaluateCondition(const std::string& condition);
    bool IsMacroDefined(const std::string& macroName) const;
    std::string GetMacroValue(const std::string& macroName) const;
    
    void SetError(const std::string& error);
    void AddWarning(const std::string& warning);

    // Member değişkenler
    std::unordered_map<std::string, std::string> m_macros;
    std::vector<std::string> m_includePaths;
    std::vector<std::string> m_warnings;
    std::string m_lastError;
    
    mutable std::mutex m_mutex;
};

/**
 * @class ShaderReflection
 * @brief Shader reflection bilgileri
 * 
 * Bu sınıf, derlenmiş SPIR-V kodundan shader bilgilerini çıkarır:
 * - Uniform buffer'lar
 * - Texture sampler'lar
 * - Input/output değişkenleri
 * - Push constant'lar
 */
class ShaderReflection {
public:
    /**
     * @struct UniformBufferInfo
     * @brief Uniform buffer bilgisi
     */
    struct UniformBufferInfo {
        std::string name;                           ///< Buffer adı
        uint32_t set = 0;                          ///< Descriptor set index
        uint32_t binding = 0;                      ///< Binding index
        uint32_t size = 0;                         ///< Buffer boyutu
        std::vector<std::pair<std::string, uint32_t>> members; ///< Üye değişkenler (isim, offset)
    };

    /**
     * @struct TextureInfo
     * @brief Texture bilgisi
     */
    struct TextureInfo {
        std::string name;                           ///< Texture adı
        uint32_t set = 0;                          ///< Descriptor set index
        uint32_t binding = 0;                      ///< Binding index
        VkFormat format = VK_FORMAT_UNDEFINED;     ///< Format
    };

    /**
     * @struct InputAttributeInfo
     * @brief Input attribute bilgisi
     */
    struct InputAttributeInfo {
        std::string name;                           ///< Attribute adı
        uint32_t location = 0;                     ///< Location index
        VkFormat format = VK_FORMAT_UNDEFINED;     ///< Format
        uint32_t offset = 0;                        ///< Offset
    };

    /**
     * @struct PushConstantInfo
     * @brief Push constant bilgisi
     */
    struct PushConstantInfo {
        std::string name;                           ///< Push constant adı
        uint32_t offset = 0;                        ///< Offset
        uint32_t size = 0;                         ///< Boyut
        VkShaderStageFlags stageFlags = 0;         ///< Kullanılan shader aşamaları
    };

    // Reflection işlemleri
    bool Reflect(const std::vector<uint32_t>& spirvCode, ShaderStage stage);
    void Clear();

    // Bilgi erişimi
    const std::vector<UniformBufferInfo>& GetUniformBuffers() const { return m_uniformBuffers; }
    const std::vector<TextureInfo>& GetTextures() const { return m_textures; }
    const std::vector<InputAttributeInfo>& GetInputAttributes() const { return m_inputAttributes; }
    const std::vector<PushConstantInfo>& GetPushConstants() const { return m_pushConstants; }
    
    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    bool HasError() const { return !m_lastError.empty(); }

private:
    // Reflection yardımcıları
    bool ParseDescriptorSets(const std::vector<uint32_t>& spirvCode);
    bool ParseInputVariables(const std::vector<uint32_t>& spirvCode);
    bool ParsePushConstants(const std::vector<uint32_t>& spirvCode);
    
    VkFormat GetVkFormatFromSpvType(uint32_t spvType) const;
    std::string GetStringFromId(const std::vector<uint32_t>& spirvCode, uint32_t id) const;
    
    void SetError(const std::string& error);

    // Member değişkenler
    std::vector<UniformBufferInfo> m_uniformBuffers;
    std::vector<TextureInfo> m_textures;
    std::vector<InputAttributeInfo> m_inputAttributes;
    std::vector<PushConstantInfo> m_pushConstants;
    
    std::string m_lastError;
    ShaderStage m_stage;
    
    mutable std::mutex m_mutex;
};

// Global shader compiler erişimi
ShaderCompiler* GetShaderCompiler();

} // namespace AstralEngine
