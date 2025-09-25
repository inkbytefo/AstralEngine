#pragma once

#include "../Core/VulkanDevice.h"
#include "IShader.h"
#include "../RendererTypes.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AstralEngine {

/**
 * @class VulkanShader
 * @brief Vulkan shader modülü yönetimi için sınıf
 *
 * Bu sınıf, SPIR-V formatında derlenmiş shader dosyalarını yükler,
 * VkShaderModule oluşturur ve yönetir. RAII prensibine uygun olarak
 * kaynakları temizler.
 *
 * IShader arayüzünü implement eder ve Material sistemi tarafından
 * polimorfik olarak kullanılabilir.
 */
class VulkanShader : public IShader {
public:
    VulkanShader();
    ~VulkanShader();

    // Non-copyable
    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const std::vector<uint32_t>& spirvCode, VkShaderStageFlagBits stage);
    void Shutdown();

    // Getter'lar
    VkShaderModule GetModule() const { return m_shaderModule; }
    VkShaderStageFlagBits GetStage() const { return m_stage; }


    // IShader interface implementations
private:
    /**
     * @brief DEPRECATED: Unsafe shader initialization without VulkanDevice
     * @deprecated Use Initialize(VulkanDevice*, const std::vector<uint32_t>&, VkShaderStageFlagBits) instead
     * @note This method is kept private to prevent unsafe usage. Always use the VulkanDevice-based initialization.
     */
    virtual bool Initialize(ShaderStage stage, const std::vector<uint32_t>& shaderCode) override;
    virtual ShaderStage GetShaderStage() const override;
    virtual VkShaderStageFlagBits GetVulkanShaderStage() const override;
    virtual bool IsInitialized() const override;
    virtual const std::string& GetLastError() const override;
    virtual const std::vector<uint32_t>& GetShaderCode() const override;
    virtual VkShaderModule GetShaderModule() const override;
    virtual uint64_t GetShaderHash() const override;
    virtual bool IsCompatibleWithAPI(RendererAPI api) const override;
    virtual size_t GetShaderCodeSize() const override;
    virtual bool Validate() const override;
    virtual void Shutdown() override;

private:
    // Yardımcı metotlar
    bool CreateShaderModule(const std::vector<uint32_t>& spirvCode);
    virtual void SetError(const std::string& error) override;

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_stage;
    std::string m_lastError;
    bool m_isInitialized = false;
    std::vector<uint32_t> m_shaderCode;
    uint64_t m_shaderHash = 0;
};

} // namespace AstralEngine
