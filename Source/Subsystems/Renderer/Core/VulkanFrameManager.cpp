#include "VulkanFrameManager.h"
#include "../../../Core/Logger.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "../Buffers/VulkanBuffer.h"
#include <glm/glm.hpp>

namespace AstralEngine {

VulkanFrameManager::VulkanFrameManager()
    : m_device(nullptr)
    , m_swapchain(nullptr)
    , m_maxFramesInFlight(2)
    , m_initialized(false)
    , m_currentFrameIndex(0)
    , m_imageIndex(0)
    , m_frameStarted(false)
    , m_commandPool(VK_NULL_HANDLE)
    , m_descriptorPool(VK_NULL_HANDLE)
    , m_descriptorSetLayout(VK_NULL_HANDLE) {
    
    Logger::Debug("VulkanFrameManager", "VulkanFrameManager created");
}

VulkanFrameManager::~VulkanFrameManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("VulkanFrameManager", "VulkanFrameManager destroyed");
}

bool VulkanFrameManager::Initialize(VulkanDevice* device, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout, uint32_t maxFramesInFlight) {
    if (m_initialized) {
        Logger::Error("VulkanFrameManager", "VulkanFrameManager already initialized");
        return false;
    }

    Logger::Info("VulkanFrameManager", "Initializing VulkanFrameManager...");

    try {
        m_device = device;
        m_swapchain = swapchain;
        m_descriptorSetLayout = descriptorSetLayout;
        m_maxFramesInFlight = maxFramesInFlight;

        if (!m_device || !m_swapchain) {
            SetError("Invalid device or swapchain");
            return false;
        }

        // Frame kaynaklarını oluştur
        if (!CreateSynchronizationObjects()) {
            Logger::Error("VulkanFrameManager", "Failed to create synchronization objects");
            return false;
        }

        if (!CreateCommandBuffers()) {
            Logger::Error("VulkanFrameManager", "Failed to create command buffers");
            return false;
        }

        if (!CreateDescriptorPool()) {
            Logger::Error("VulkanFrameManager", "Failed to create descriptor pool");
            return false;
        }

        if (!CreateDescriptorSets()) {
            Logger::Error("VulkanFrameManager", "Failed to create descriptor sets");
            return false;
        }

        if (!CreateUniformBuffers()) {
            Logger::Error("VulkanFrameManager", "Failed to create uniform buffers");
            return false;
        }

        m_initialized = true;
        Logger::Info("VulkanFrameManager", "VulkanFrameManager initialized successfully with {} frames in flight", maxFramesInFlight);
        return true;

    } catch (const std::exception& e) {
        SetError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanFrameManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("VulkanFrameManager", "Shutting down VulkanFrameManager...");

    // Cihazı bekle
    if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device->GetDevice());
    }

    CleanupFrameResources();
    ClearError();
    m_initialized = false;

    Logger::Info("VulkanFrameManager", "VulkanFrameManager shutdown complete");
}

bool VulkanFrameManager::BeginFrame() {
    if (!m_initialized || m_frameStarted) {
        return false;
    }

    try {
        // Bir sonraki image'i al
        if (!AcquireNextImage()) {
            return false;
        }

        m_frameStarted = true;
        return true;

    } catch (const std::exception& e) {
        SetError("BeginFrame failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanFrameManager::EndFrame() {
    if (!m_initialized || !m_frameStarted) {
        return false;
    }

    try {
        // Komutları GPU'ya gönder
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Beklenecek semaphore'lar (image'in hazır olmasını bekle)
        VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrameIndex]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        // Çalıştırılacak command buffer
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrameIndex];

        // Sinyallenecek semaphore'lar (render'ın bittiğini bildir)
        VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrameIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // Fence'i resetle ve submit et
        vkResetFences(m_device->GetDevice(), 1, &m_inFlightFences[m_currentFrameIndex]);
        VkResult result = vkQueueSubmit(m_device->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrameIndex]);
        if (result != VK_SUCCESS) {
            SetError("Failed to submit draw command buffer: " + std::to_string(result));
            return false;
        }

        // Frame'i present et
        if (!PresentImage()) {
            // RecreateSwapchain gibi durumlar burada handle edilebilir
            return false;
        }

        m_frameStarted = false;
        return true;

    } catch (const std::exception& e) {
        SetError("EndFrame failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanFrameManager::WaitForFrame() {
    if (!m_initialized) {
        return false;
    }

    try {
        // Mevcut frame için fence'i bekle
        VkFence fence = m_inFlightFences[m_currentFrameIndex];
        vkWaitForFences(m_device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_device->GetDevice(), 1, &fence);
        return true;

    } catch (const std::exception& e) {
        SetError("WaitForFrame failed: " + std::string(e.what()));
        return false;
    }
}

VkCommandBuffer VulkanFrameManager::GetCurrentCommandBuffer() const {
    if (!m_initialized || m_currentFrameIndex >= m_commandBuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_commandBuffers[m_currentFrameIndex];
}

VkDescriptorSet VulkanFrameManager::GetCurrentDescriptorSet(uint32_t frameIndex) const {
    if (!m_initialized || frameIndex >= m_descriptorSets.size()) {
        return VK_NULL_HANDLE;
    }
    return m_descriptorSets[frameIndex];
}

VkBuffer VulkanFrameManager::GetCurrentUniformBuffer(uint32_t frameIndex) const {
    if (!m_initialized || frameIndex >= m_uniformBuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_uniformBuffers[frameIndex]->GetBuffer();
}

VulkanBuffer* VulkanFrameManager::GetCurrentUniformBufferWrapper(uint32_t frameIndex) const {
    if (!m_initialized || frameIndex >= m_uniformBuffers.size()) {
        return nullptr;
    }
    return m_uniformBuffers[frameIndex].get();
}

void VulkanFrameManager::RecreateSwapchain(VulkanSwapchain* newSwapchain) {
    Logger::Info("VulkanFrameManager", "Recreating swapchain-dependent resources...");

    if (!m_initialized) {
        return;
    }

    // Cihazı bekle
    if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device->GetDevice());
    }

    // Eski swapchain kaynaklarını temizle
    CleanupSwapchainResources();

    // Yeni swapchain'i ayarla
    m_swapchain = newSwapchain;

    // Swapchain'e bağlı kaynakları yeniden oluştur
    if (!CreateCommandBuffers() || !CreateDescriptorSets() || !CreateUniformBuffers()) {
        Logger::Error("VulkanFrameManager", "Failed to recreate swapchain resources");
    }

    Logger::Info("VulkanFrameManager", "Swapchain recreation complete");
}

bool VulkanFrameManager::CreateCommandBuffers() {
    Logger::Info("VulkanFrameManager", "Creating command buffers...");

    try {
        // Command pool oluştur
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_device->GetGraphicsQueueFamily();

        VkResult result = vkCreateCommandPool(m_device->GetDevice(), &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            SetError("Failed to create command pool: " + std::to_string(result));
            return false;
        }

        // Command buffer'ları oluştur
        m_commandBuffers.resize(m_maxFramesInFlight);
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

        result = vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, m_commandBuffers.data());
        if (result != VK_SUCCESS) {
            SetError("Failed to allocate command buffers: " + std::to_string(result));
            return false;
        }

        Logger::Info("VulkanFrameManager", "Command buffers created successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("CreateCommandBuffers failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanFrameManager::CreateDescriptorPool() {
    Logger::Info("VulkanFrameManager", "Creating descriptor pool...");

    try {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(m_maxFramesInFlight) },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(m_maxFramesInFlight) }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = static_cast<uint32_t>(m_maxFramesInFlight);
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VkResult result = vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool);
        if (result != VK_SUCCESS) {
            SetError("Failed to create descriptor pool: " + std::to_string(result));
            return false;
        }

        Logger::Info("VulkanFrameManager", "Descriptor pool created successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("CreateDescriptorPool failed: " + std::string(e.what()));
        return false;
    }
}


bool VulkanFrameManager::CreateDescriptorSets() {
    Logger::Info("VulkanFrameManager", "Creating descriptor sets...");

    try {
        // Layout bilgilerini hazırla
        std::vector<VkDescriptorSetLayout> layouts(m_maxFramesInFlight, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(m_maxFramesInFlight);
        allocInfo.pSetLayouts = layouts.data();

        m_descriptorSets.resize(m_maxFramesInFlight);
        VkResult result = vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data());
        if (result != VK_SUCCESS) {
            SetError("Failed to allocate descriptor sets: " + std::to_string(result));
            return false;
        }

        Logger::Info("VulkanFrameManager", "Descriptor sets created successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("CreateDescriptorSets failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanFrameManager::CreateUniformBuffers() {
    Logger::Info("VulkanFrameManager", "Creating uniform buffers...");

    try {
        // Her frame için bir uniform buffer oluştur
        m_uniformBuffers.resize(m_maxFramesInFlight);
        
        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i) {
            m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
            
            // UBO için buffer oluştur
            VulkanBuffer::Config bufferConfig;
            bufferConfig.size = sizeof(glm::mat4) * 2; // model + view + projection matrisleri için
            bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            if (!m_uniformBuffers[i]->Initialize(m_device, bufferConfig)) {
                SetError("Failed to create uniform buffer for frame " + std::to_string(i));
                return false;
            }
        }

        Logger::Info("VulkanFrameManager", "Uniform buffers created successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("CreateUniformBuffers failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanFrameManager::CreateSynchronizationObjects() {
    Logger::Info("VulkanFrameManager", "Creating synchronization objects...");

    try {
        m_imageAvailableSemaphores.resize(m_maxFramesInFlight);
        m_renderFinishedSemaphores.resize(m_maxFramesInFlight);
        m_inFlightFences.resize(m_maxFramesInFlight);
        m_imagesInFlight.resize(m_swapchain->GetImageCount(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i) {
            VkResult result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
            if (result != VK_SUCCESS) {
                SetError("Failed to create image available semaphore: " + std::to_string(result));
                return false;
            }

            result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
            if (result != VK_SUCCESS) {
                SetError("Failed to create render finished semaphore: " + std::to_string(result));
                return false;
            }

            result = vkCreateFence(m_device->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]);
            if (result != VK_SUCCESS) {
                SetError("Failed to create fence: " + std::to_string(result));
                return false;
            }
        }

        Logger::Info("VulkanFrameManager", "Synchronization objects created successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("CreateSynchronizationObjects failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanFrameManager::CleanupFrameResources() {
    Logger::Info("VulkanFrameManager", "Cleaning up frame resources...");

    if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
        // Uniform buffer'ları temizle
        for (auto& buffer : m_uniformBuffers) {
            if (buffer) {
                buffer->Shutdown();
                buffer.reset();
            }
        }
        m_uniformBuffers.clear();

        // Descriptor set'leri temizle
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        // Descriptor set layout'ı temizleme - artık GraphicsDevice tarafından yönetiliyor
        // m_descriptorSetLayout dışarıdan alındığı için burada temizlemiyoruz

        // Command buffer'ları temizle
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device->GetDevice(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        m_commandBuffers.clear();

        // Senkronizasyon nesnelerini temizle
        for (auto& semaphore : m_imageAvailableSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device->GetDevice(), semaphore, nullptr);
            }
        }
        m_imageAvailableSemaphores.clear();

        for (auto& semaphore : m_renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device->GetDevice(), semaphore, nullptr);
            }
        }
        m_renderFinishedSemaphores.clear();

        for (auto& fence : m_inFlightFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device->GetDevice(), fence, nullptr);
            }
        }
        m_inFlightFences.clear();

        m_imagesInFlight.clear();
    }

    Logger::Info("VulkanFrameManager", "Frame resources cleanup complete");
}

void VulkanFrameManager::CleanupSwapchainResources() {
    Logger::Info("VulkanFrameManager", "Cleaning up swapchain-dependent resources...");

    if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
        // Sadece swapchain'e bağlı kaynakları temizle
        vkDeviceWaitIdle(m_device->GetDevice());

        // Command buffer'ları temizle (swapchain boyu değişebilir)
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device->GetDevice(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        m_commandBuffers.clear();

        // Descriptor set'leri yeniden oluştur (swapchain değişebilir)
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_device->GetDevice(), m_descriptorPool, 0);
        }

        // Uniform buffer'ları temizle ve yeniden oluştur
        for (auto& buffer : m_uniformBuffers) {
            if (buffer) {
                buffer->Shutdown();
                buffer.reset();
            }
        }
        m_uniformBuffers.clear();
    }

    Logger::Info("VulkanFrameManager", "Swapchain-dependent resources cleanup complete");
}

void VulkanFrameManager::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanFrameManager", "{}", error);
}

void VulkanFrameManager::ClearError() {
    m_lastError.clear();
}

bool VulkanFrameManager::AcquireNextImage() {
    if (!m_device || !m_swapchain) {
        SetError("Device or swapchain is null");
        return false;
    }

    // Mevcut frame için fence'i bekle
    VkFence fence = m_inFlightFences[m_currentFrameIndex];
    vkWaitForFences(m_device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->GetDevice(), 1, &fence);

    // Bir sonraki image'i al
    VkResult result = vkAcquireNextImageKHR(
        m_device->GetDevice(),
        m_swapchain->GetSwapchain(),
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrameIndex],
        VK_NULL_HANDLE,
        &m_imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        Logger::Warning("VulkanFrameManager", "Swapchain is out of date, needs recreation");
        // Swapchain yeniden oluşturulmalı, bunu bildir
        return false;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        Logger::Warning("VulkanFrameManager", "Swapchain is suboptimal, continuing");
        // Suboptimal devam edebilir
    } else if (result != VK_SUCCESS) {
        SetError("Failed to acquire next image: " + std::to_string(result));
        return false;
    }

    // Boundary check: m_imageIndex değerini doğrula
    if (m_imageIndex >= m_imagesInFlight.size()) {
        Logger::Error("VulkanFrameManager", "Image index {} out of range for imagesInFlight vector (size: {})", 
                    m_imageIndex, m_imagesInFlight.size());
        return false;
    }

    // Eğer bu image için zaten bir fence varsa bekle
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_device->GetDevice(), 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Mevcut frame'in fence'ini işaretle
    m_imagesInFlight[m_imageIndex] = m_inFlightFences[m_currentFrameIndex];

    return true;
}

bool VulkanFrameManager::PresentImage() {
    if (!m_device || !m_swapchain) {
        SetError("Device or swapchain is null");
        return false;
    }

    // Present bilgilerini hazırla
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = m_renderFinishedSemaphores.data() + m_currentFrameIndex;
    presentInfo.swapchainCount = 1;
    
    // Get swapchain handle and store it for address operation
    VkSwapchainKHR swapchainHandle = m_swapchain->GetSwapchain();
    presentInfo.pSwapchains = &swapchainHandle;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_device->GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        Logger::Warning("VulkanFrameManager", "Swapchain is out of date after present, needs recreation");
        // Swapchain yeniden oluşturulmalı, bunu bildir
        return false;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        Logger::Warning("VulkanFrameManager", "Swapchain is suboptimal after present, continuing");
        // Suboptimal durumda devam edebiliriz
    } else if (result != VK_SUCCESS) {
        SetError("Failed to present image: " + std::to_string(result));
        return false;
    }

    // Sonraki frame'e geç
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_maxFramesInFlight;

    return true;
}

} // namespace AstralEngine
