#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <array>

namespace AstralEngine {

// Forward declarations
class Camera;

/**
 * @struct LightData
 * @brief Işık verilerini içeren yapı - sahne UBO'su için kullanılır
 */
struct LightData {
    glm::vec4 position;      // Işık pozisyonu (w = 1.0 for point light, 0.0 for directional)
    glm::vec4 color;         // Işık rengi ve yoğunluğu (RGB + intensity)
    glm::vec4 direction;     // Işık yönü (directional ışıklar için)
    float intensity;         // Işık yoğunluğu
    float range;             // Işık menzili (point ışıklar için)
    float innerConeAngle;    // Spot ışık iç açısı
    float outerConeAngle;    // Spot ışık dış açısı
    uint32_t type;           // Işık tipi (0: directional, 1: point, 2: spot)
    uint32_t enabled;        // Işık aktif mi?
};

class VulkanDevice;
class VulkanSwapchain;
class VulkanBuffer;

/**
 * @class VulkanFrameManager
 * @brief Frame bazlı Vulkan kaynaklarını yöneten sınıf
 *
 * Bu sınıf, GraphicsDevice'dan ayrıştırılmış frame bazlı kaynakları yönetir:
 * - Command buffer'lar ve pool
 * - Descriptor set'ler, pool ve layout
 * - Uniform buffer'lar
 * - Frame senkronizasyon nesneleri (semaphore, fence)
 *
 * Yeni merkezi descriptor pool yönetimi:
 * - Her frame için ayrı descriptor pool kullanır
 * - Memory fragmentation'ı önler ve performansı optimize eder
 * - Material sınıfı için merkezi descriptor set allocation sağlar
 *
 * Tek Sorumluluk Prensibi'ne göre tasarlanmıştır.
 */
class VulkanFrameManager {
public:
    VulkanFrameManager();
    ~VulkanFrameManager();

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout, uint32_t maxFramesInFlight);
    void Shutdown();

    // Frame yönetimi
    bool BeginFrame();
    bool EndFrame();
    bool WaitForFrame();

    // Resource erişimi
    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkDescriptorSet GetCurrentDescriptorSet(uint32_t frameIndex) const;
    VkDescriptorPool GetDescriptorPool() const; // Mevcut frame için descriptor pool döndürür
    VkDescriptorPool GetDescriptorPool(uint32_t frameIndex) const; // Belirtilen frame için descriptor pool döndürür
    VkBuffer GetCurrentUniformBuffer(uint32_t frameIndex) const;
    VulkanBuffer* GetCurrentUniformBufferWrapper(uint32_t frameIndex) const;
    
    // Merkezi descriptor pool yönetimi
    bool InitializeDescriptorPools();
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout, uint32_t frameIndex);
    
    // Scene UBO yönetimi
    /**
     * @brief Ana sahne UBO'sunu günceller
     *
     * Bu metot, kamera ve ışık verilerini kullanarak uniform buffer'ı günceller.
     * Her frame başında çağrılarak sahne verilerinin GPU'ya gönderilmesini sağlar.
     *
     * @param camera Güncel kamera verileri (view ve projection matrisleri)
     * @param lights Sahnedeki ışık verileri
     * @return true Başarılı olursa
     * @return false Hata olursa
     */
    bool UpdateSceneUBO(const Camera& camera, const LightData& lights);
    
    // Frame bilgileri
    uint32_t GetCurrentFrameIndex() const { return m_currentFrameIndex; }
    uint32_t GetCurrentImageIndex() const { return m_imageIndex; }
    uint32_t GetMaxFramesInFlight() const { return m_maxFramesInFlight; }

    // Swapchain recreation
    void RecreateSwapchain(VulkanSwapchain* newSwapchain);

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Resource oluşturma ve temizleme
    bool CreateCommandBuffers();
    bool CreateDescriptorPool(); // Legacy metod, InitializeDescriptorPools() ile değiştirilecek
    bool CreateDescriptorSets();
    bool CreateUniformBuffers();
    bool CreateSynchronizationObjects();
    
    void CleanupFrameResources();
    void CleanupSwapchainResources();

    // Yardımcı metodlar
    void SetError(const std::string& error);
    void ClearError();
    bool AcquireNextImage();
    bool PresentImage();

    // Member değişkenler
    VulkanDevice* m_device;
    VulkanSwapchain* m_swapchain;
    uint32_t m_maxFramesInFlight;
    std::string m_lastError;
    bool m_initialized;

    // Frame yönetimi
    uint32_t m_currentFrameIndex;
    uint32_t m_imageIndex;
    bool m_frameStarted;

    // Command buffer yönetimi
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Descriptor yönetimi
    VkDescriptorPool m_descriptorPool; // Legacy, geriye uyumluluk için tutuluyor
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    
    // Merkezi descriptor pool yönetimi
    std::vector<VkDescriptorPool> m_descriptorPools; // Her frame için ayrı pool
    static const uint32_t DESCRIPTOR_TYPE_COUNT = 2; // Uniform buffer ve combined image sampler
    std::array<VkDescriptorPoolSize, DESCRIPTOR_TYPE_COUNT> m_poolSizes; // Descriptor tipleri ve sayıları
    uint32_t m_maxSets; // Her pool için maksimum descriptor set sayısı

    // Uniform buffer yönetimi
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;

    // Frame senkronizasyon nesneleri
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
};

} // namespace AstralEngine
