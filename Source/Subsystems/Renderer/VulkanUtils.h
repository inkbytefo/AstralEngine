#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace AstralEngine {

/**
 * @class VulkanUtils
 * @brief Vulkan API için yardımcı fonksiyonlar ve hata yönetimi
 * 
 * Bu sınıf, Vulkan API çağrıları için hata kontrolü, format dönüşümleri,
 * debug loglama ve diğer yardımcı işlevleri sağlar. Modern C++20/23
 * standartlarına göre tasarlanmıştır.
 */
class VulkanUtils {
public:
    /**
     * @brief VkResult kontrolü ve hata yönetimi
     * @param result Vulkan API çağrısının sonucu
     * @param operation İşlem adı (loglama için)
     * @param throwOnError Hata durumunda exception fırlat
     * @return true if successful, false if failed and throwOnError is false
     */
    static bool CheckVkResult(VkResult result, const std::string& operation, bool throwOnError = true);
    
    /**
     * @brief VkResult'i okunabilir string'e dönüştürür
     * @param result Vulkan sonuç kodu
     * @return Okunabilir hata mesajı
     */
    static std::string GetVkResultString(VkResult result);
    
    /**
     * @brief Format için destekliğini kontrol et
     * @param format Kontrol edilecek format
     * @param tiling Image tiling tipi
     * @param features Gerekli format özellikleri
     * @return true if format is supported
     */
    static bool IsFormatSupported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);
    
    /**
     * @brief Desteklenen formatı bul
     * @param candidates Öncelikli format listesi
     * @param tiling Image tiling tipi
     * @param features Gerekli format özellikleri
     * @return Desteklenen format veya VK_FORMAT_UNDEFINED
     */
    static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    
    /**
     * @brief Derinlik formatını otomatik seç
     * @param physicalDevice Fiziksel cihaz
     * @return Desteklenen derinlik formatı
     */
    static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);
    
    /**
     * @brief Image aspect flag'lerini format'a göre belirle
     * @param format Image formatı
     * @return Aspect flag'leri
     */
    static VkImageAspectFlags GetImageAspectFlags(VkFormat format);
    
    /**
     * @brief Shader stage flag'lerini string'e dönüştür
     * @param stage Shader stage flag'leri
     * @return Okunabilir string
     */
    static std::string GetShaderStageString(VkShaderStageFlags stage);
    
    /**
     * @brief Buffer usage flag'lerini string'e dönüştür
     * @param usage Buffer usage flag'leri
     * @return Okunabilir string
     */
    static std::string GetBufferUsageString(VkBufferUsageFlags usage);
    
    /**
     * @brief Image usage flag'lerini string'e dönüştür
     * @param usage Image usage flag'leri
     * @return Okunabilir string
     */
    static std::string GetImageUsageString(VkImageUsageFlags usage);
    
    /**
     * @brief Extension'ların desteklendiğini kontrol et
     * @param instance Vulkan instance
     * @param extensions Kontrol edilecek extension'lar
     * @return true if all extensions are supported
     */
    static bool AreExtensionsSupported(VkInstance instance, const std::vector<const char*>& extensions);
    
    /**
     * @brief Validation layer'ların desteklendiğini kontrol et
     * @param layers Kontrol edilecek layer'lar
     * @return true if all layers are supported
     */
    static bool AreValidationLayersSupported(const std::vector<const char*>& layers);
    
    /**
     * @brief Gerekli instance extension'larını al
     * @param enableValidationLayers Validation layer'lar etkin mi
     * @return Gerekli extension'lar
     */
    static std::vector<const char*> GetRequiredInstanceExtensions(bool enableValidationLayers);
    
    /**
     * @brief Gerekli device extension'larını al
     * @return Gerekli extension'lar
     */
    static std::vector<const char*> GetRequiredDeviceExtensions();
    
    /**
     * @brief Debug messenger için callback fonksiyonu
     * @param messageSeverity Mesaj şiddeti
     * @param messageType Mesaj tipi
     * @param pCallbackData Callback verisi
     * @param pUserData Kullanıcı verisi
     * @return VK_FALSE (hata durumunda uygulama devam etsin)
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
    
    /**
     * @brief Debug messenger oluşturma bilgisi
     * @param info Oluşturma bilgisi
     */
    static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info);
    
    /**
     * @brief Hata mesajını logla
     * @param message Hata mesajı
     * @param file Dosya adı
     * @param line Satır numarası
     */
    static void LogError(const std::string& message, const std::string& file = "", int line = 0);
    
    /**
     * @brief Uyarı mesajını logla
     * @param message Uyarı mesajı
     * @param file Dosya adı
     * @param line Satır numarası
     */
    static void LogWarning(const std::string& message, const std::string& file = "", int line = 0);
    
    /**
     * @brief Bilgi mesajını logla
     * @param message Bilgi mesajı
     * @param file Dosya adı
     * @param line Satır numarası
     */
    static void LogInfo(const std::string& message, const std::string& file = "", int line = 0);
    
    /**
     * @brief Debug mesajını logla
     * @param message Debug mesajı
     * @param file Dosya adı
     * @param line Satır numarası
     */
    static void LogDebug(const std::string& message, const std::string& file = "", int line = 0);
    
    /**
     * @brief Bellek boyutını okunabilir formata dönüştür
     * @param bytes Byte cinsinden boyut
     * @return Okunabilir string (KB, MB, GB)
     */
    static std::string FormatMemorySize(VkDeviceSize bytes);
    
    /**
     * @brief Versiyon numarasını string'e dönüştür
     * @param version Versiyon numarası
     * @return "X.Y.Z" formatında string
     */
    static std::string FormatVersion(uint32_t version);

private:
    /**
     * @brief Format özelliklerini string'e dönüştür
     * @param features Format özellikleri
     * @return Okunabilir string
     */
    static std::string GetFormatFeatureString(VkFormatFeatureFlags features);
    
    /**
     * @brief Flag'leri string'e dönüştür (yardımcı template)
     * @param flags Flag değeri
     * @param flagNames Flag isimleri ve değerleri
     * @return Okunabilir string
     */
    template<typename T, typename F>
    static std::string FlagsToString(T flags, const std::vector<std::pair<F, const char*>>& flagNames);
};

/**
 * @brief VulkanResultException sınıfı
 * Vulkan hataları için özel exception
 */
class VulkanResultException : public std::runtime_error {
public:
    VulkanResultException(VkResult result, const std::string& operation)
        : std::runtime_error("Vulkan Error: " + VulkanUtils::GetVkResultString(result) + 
                           " during operation: " + operation), m_result(result) {}
    
    VkResult GetResult() const { return m_result; }

private:
    VkResult m_result;
};

/**
 * @brief RAII tabanlı Vulkan nesne deleter'ları
 */
template<typename T>
struct VulkanDeleter {
    void operator()(T* obj) const {
        if (obj) {
            // Özel deleter implementasyonu türe göre değişir
            // Bu template özel olarak özelleştirilmeli
        }
    }
};

// Özel deleter özelleştirmeleri
template<>
struct VulkanDeleter<VkInstance> {
    void operator()(VkInstance* instance) const {
        if (*instance != VK_NULL_HANDLE) {
            vkDestroyInstance(*instance, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkDevice> {
    void operator()(VkDevice* device) const {
        if (*device != VK_NULL_HANDLE) {
            vkDestroyDevice(*device, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkSwapchainKHR> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkSwapchainKHR* swapchain) const {
        if (*swapchain != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, *swapchain, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkImage> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkImage* image) const {
        if (*image != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyImage(device, *image, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkImageView> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkImageView* imageView) const {
        if (*imageView != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyImageView(device, *imageView, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkBuffer> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkBuffer* buffer) const {
        if (*buffer != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, *buffer, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkDeviceMemory> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkDeviceMemory* memory) const {
        if (*memory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkFreeMemory(device, *memory, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkSemaphore> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkSemaphore* semaphore) const {
        if (*semaphore != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, *semaphore, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkFence> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkFence* fence) const {
        if (*fence != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyFence(device, *fence, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkCommandPool> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkCommandPool* commandPool) const {
        if (*commandPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, *commandPool, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkPipeline> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkPipeline* pipeline) const {
        if (*pipeline != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, *pipeline, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkPipelineLayout> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkPipelineLayout* pipelineLayout) const {
        if (*pipelineLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, *pipelineLayout, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkDescriptorSetLayout> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkDescriptorSetLayout* descriptorSetLayout) const {
        if (*descriptorSetLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, *descriptorSetLayout, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkDescriptorPool> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkDescriptorPool* descriptorPool) const {
        if (*descriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, *descriptorPool, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkRenderPass> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkRenderPass* renderPass) const {
        if (*renderPass != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, *renderPass, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkFramebuffer> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkFramebuffer* framebuffer) const {
        if (*framebuffer != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, *framebuffer, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkShaderModule> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkShaderModule* shaderModule) const {
        if (*shaderModule != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, *shaderModule, nullptr);
        }
    }
};

template<>
struct VulkanDeleter<VkSampler> {
    VkDevice device;
    
    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    
    void operator()(VkSampler* sampler) const {
        if (*sampler != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroySampler(device, *sampler, nullptr);
        }
    }
};

// RAII için smart pointer typedef'leri
using VkInstancePtr = std::unique_ptr<VkInstance, VulkanDeleter<VkInstance>>;
using VkDevicePtr = std::unique_ptr<VkDevice, VulkanDeleter<VkDevice>>;
using VkSwapchainKHRPtr = std::unique_ptr<VkSwapchainKHR, VulkanDeleter<VkSwapchainKHR>>;
using VkImagePtr = std::unique_ptr<VkImage, VulkanDeleter<VkImage>>;
using VkImageViewPtr = std::unique_ptr<VkImageView, VulkanDeleter<VkImageView>>;
using VkBufferPtr = std::unique_ptr<VkBuffer, VulkanDeleter<VkBuffer>>;
using VkDeviceMemoryPtr = std::unique_ptr<VkDeviceMemory, VulkanDeleter<VkDeviceMemory>>;
using VkSemaphorePtr = std::unique_ptr<VkSemaphore, VulkanDeleter<VkSemaphore>>;
using VkFencePtr = std::unique_ptr<VkFence, VulkanDeleter<VkFence>>;
using VkCommandPoolPtr = std::unique_ptr<VkCommandPool, VulkanDeleter<VkCommandPool>>;
using VkPipelinePtr = std::unique_ptr<VkPipeline, VulkanDeleter<VkPipeline>>;
using VkPipelineLayoutPtr = std::unique_ptr<VkPipelineLayout, VulkanDeleter<VkPipelineLayout>>;
using VkDescriptorSetLayoutPtr = std::unique_ptr<VkDescriptorSetLayout, VulkanDeleter<VkDescriptorSetLayout>>;
using VkDescriptorPoolPtr = std::unique_ptr<VkDescriptorPool, VulkanDeleter<VkDescriptorPool>>;
using VkRenderPassPtr = std::unique_ptr<VkRenderPass, VulkanDeleter<VkRenderPass>>;
using VkFramebufferPtr = std::unique_ptr<VkFramebuffer, VulkanDeleter<VkFramebuffer>>;
using VkShaderModulePtr = std::unique_ptr<VkShaderModule, VulkanDeleter<VkShaderModule>>;
using VkSamplerPtr = std::unique_ptr<VkSampler, VulkanDeleter<VkSampler>>;

} // namespace AstralEngine