#include "VulkanSynchronization.h"
#include "../../Core/Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace AstralEngine {

// VulkanSemaphore implementasyonu
VulkanSemaphore::VulkanSemaphore() = default;

VulkanSemaphore::~VulkanSemaphore() {
    Shutdown();
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept {
    m_semaphore = other.m_semaphore;
    m_device = other.m_device;
    m_type = other.m_type;
    m_debugName = std::move(other.m_debugName);
    m_initialized = other.m_initialized;
    
    // Diğer nesneyi geçersiz kıl
    other.m_semaphore = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_initialized = false;
}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
    if (this != &other) {
        // Mevcut semaphore'ı temizle
        Shutdown();
        
        // Diğer nesneden taşı
        m_semaphore = other.m_semaphore;
        m_device = other.m_device;
        m_type = other.m_type;
        m_debugName = std::move(other.m_debugName);
        m_initialized = other.m_initialized;
        
        // Diğer nesneyi geçersiz kıl
        other.m_semaphore = VK_NULL_HANDLE;
        other.m_device = nullptr;
        other.m_initialized = false;
    }
    
    return *this;
}

bool VulkanSemaphore::Initialize(VulkanDevice* device, const SemaphoreCreateInfo& createInfo) {
    if (m_initialized) {
        SetError("Semaphore already initialized");
        return false;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = device;
    m_type = createInfo.type;
    m_debugName = createInfo.debugName;
    
    VkDevice vkDevice = device->GetDevice();
    
    switch (m_type) {
        case SemaphoreType::Timeline:
            return InitializeTimelineSemaphore(vkDevice, createInfo.initialValue);
        case SemaphoreType::Binary:
            return InitializeBinarySemaphore(vkDevice);
        default:
            SetError("Unknown semaphore type");
            return false;
    }
}

void VulkanSemaphore::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    if (m_semaphore != VK_NULL_HANDLE && m_device) {
        vkDestroySemaphore(m_device->GetDevice(), m_semaphore, nullptr);
    }
    
    m_semaphore = VK_NULL_HANDLE;
    m_device = nullptr;
    m_initialized = false;
}

bool VulkanSemaphore::Signal(uint64_t value) {
    if (!m_initialized || m_type != SemaphoreType::Timeline) {
        SetError("Timeline semaphore not initialized");
        return false;
    }
    
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;
    
    VkResult result = vkSignalSemaphore(m_device->GetDevice(), &signalInfo);
    if (result != VK_SUCCESS) {
        SetError("Failed to signal timeline semaphore: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

bool VulkanSemaphore::Wait(uint64_t value, uint64_t timeout) {
    if (!m_initialized || m_type != SemaphoreType::Timeline) {
        SetError("Timeline semaphore not initialized");
        return false;
    }
    
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;
    waitInfo.semaphoreCount = 1;
    
    VkResult result = vkWaitSemaphores(m_device->GetDevice(), &waitInfo, timeout);
    if (result != VK_SUCCESS) {
        SetError("Failed to wait for timeline semaphore: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

uint64_t VulkanSemaphore::GetCurrentValue() const {
    if (!m_initialized || m_type != SemaphoreType::Timeline) {
        return 0;
    }
    
    uint64_t value = 0;
    VkResult result = vkGetSemaphoreCounterValue(m_device->GetDevice(), m_semaphore, &value);
    if (result != VK_SUCCESS) {
        return 0;
    }
    
    return value;
}

bool VulkanSemaphore::SignalBinary() {
    if (!m_initialized || m_type != SemaphoreType::Binary) {
        SetError("Binary semaphore not initialized");
        return false;
    }
    
    // Binary semaphore için signal işlemi doğrudan desteklenmez
    // Bunun yerine submit info içinde kullanılır
    return true;
}

bool VulkanSemaphore::WaitBinary(uint64_t timeout) {
    if (!m_initialized || m_type != SemaphoreType::Binary) {
        SetError("Binary semaphore not initialized");
        return false;
    }
    
    // Binary semaphore için wait işlemi doğrudan desteklenmez
    // Bunun yerine submit info içinde kullanılır
    return true;
}

VkSemaphoreSubmitInfo VulkanSemaphore::GetSubmitInfo(VkPipelineStageFlags2 stageMask, uint64_t value) const {
    VkSemaphoreSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submitInfo.semaphore = m_semaphore;
    submitInfo.stageMask = stageMask;
    
    if (m_type == SemaphoreType::Timeline) {
        submitInfo.value = value;
    }
    
    return submitInfo;
}

bool VulkanSemaphore::InitializeTimelineSemaphore(VkDevice device, uint64_t initialValue) {
    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = initialValue;
    
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &timelineCreateInfo;
    
    VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &m_semaphore);
    if (result != VK_SUCCESS) {
        SetError("Failed to create timeline semaphore: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool VulkanSemaphore::InitializeBinarySemaphore(VkDevice device) {
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &m_semaphore);
    if (result != VK_SUCCESS) {
        SetError("Failed to create binary semaphore: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    m_initialized = true;
    return true;
}

void VulkanSemaphore::SetError(const std::string& error) {
    m_lastError = error;
    VulkanUtils::LogError(error, __FILE__, __LINE__);
}

// VulkanFence implementasyonu
VulkanFence::VulkanFence() = default;

VulkanFence::~VulkanFence() {
    Shutdown();
}

VulkanFence::VulkanFence(VulkanFence&& other) noexcept {
    m_fence = other.m_fence;
    m_device = other.m_device;
    m_debugName = std::move(other.m_debugName);
    m_initialized = other.m_initialized;
    
    // Diğer nesneyi geçersiz kıl
    other.m_fence = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_initialized = false;
}

VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept {
    if (this != &other) {
        // Mevcut fence'i temizle
        Shutdown();
        
        // Diğer nesneden taşı
        m_fence = other.m_fence;
        m_device = other.m_device;
        m_debugName = std::move(other.m_debugName);
        m_initialized = other.m_initialized;
        
        // Diğer nesneyi geçersiz kıl
        other.m_fence = VK_NULL_HANDLE;
        other.m_device = nullptr;
        other.m_initialized = false;
    }
    
    return *this;
}

bool VulkanFence::Initialize(VulkanDevice* device, const FenceCreateInfo& createInfo) {
    if (m_initialized) {
        SetError("Fence already initialized");
        return false;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = device;
    m_debugName = createInfo.debugName;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = createInfo.signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    
    VkResult result = vkCreateFence(device->GetDevice(), &fenceInfo, nullptr, &m_fence);
    if (result != VK_SUCCESS) {
        SetError("Failed to create fence: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    m_initialized = true;
    return true;
}

void VulkanFence::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    if (m_fence != VK_NULL_HANDLE && m_device) {
        vkDestroyFence(m_device->GetDevice(), m_fence, nullptr);
    }
    
    m_fence = VK_NULL_HANDLE;
    m_device = nullptr;
    m_initialized = false;
}

bool VulkanFence::Reset() {
    if (!m_initialized) {
        SetError("Fence not initialized");
        return false;
    }
    
    VkResult result = vkResetFences(m_device->GetDevice(), 1, &m_fence);
    if (result != VK_SUCCESS) {
        SetError("Failed to reset fence: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

bool VulkanFence::Wait(uint64_t timeout) {
    if (!m_initialized) {
        SetError("Fence not initialized");
        return false;
    }
    
    VkResult result = vkWaitForFences(m_device->GetDevice(), 1, &m_fence, VK_TRUE, timeout);
    if (result != VK_SUCCESS) {
        SetError("Failed to wait for fence: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

bool VulkanFence::IsSignaled() const {
    if (!m_initialized) {
        return false;
    }
    
    VkResult result = vkGetFenceStatus(m_device->GetDevice(), m_fence);
    return result == VK_SUCCESS;
}


void VulkanFence::SetError(const std::string& error) {
    m_lastError = error;
    VulkanUtils::LogError(error, __FILE__, __LINE__);
}

// VulkanSynchronization implementasyonu
VulkanSynchronization::VulkanSynchronization() = default;

VulkanSynchronization::~VulkanSynchronization() {
    Shutdown();
}

bool VulkanSynchronization::Initialize(VulkanDevice* device, const Config& config) {
    if (m_initialized) {
        SetError("Synchronization manager already initialized");
        return false;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = device;
    m_config = config;
    
    // Konfigürasyonu doğrula
    if (!ValidateConfig(config)) {
        return false;
    }
    
    // Timeline semaphore desteğini kontrol et
    m_timelineSemaphoreSupported = CheckTimelineSemaphoreSupport();
    
    if (!m_timelineSemaphoreSupported && config.enableTimelineSemaphores) {
        VulkanUtils::LogWarning("Timeline semaphores not supported, falling back to binary semaphores", __FILE__, __LINE__);
    }
    
    m_initialized = true;
    VulkanUtils::LogInfo("VulkanSynchronization initialized successfully", __FILE__, __LINE__);
    
    return true;
}

void VulkanSynchronization::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // Tüm semaphore'lar ve fence'ler RAII ile otomatik temizlenir
    // Burada ek temizleme gerekmiyor
    
    m_device = nullptr;
    m_initialized = false;
    
    VulkanUtils::LogInfo("VulkanSynchronization shutdown completed", __FILE__, __LINE__);
}

std::unique_ptr<VulkanSemaphore> VulkanSynchronization::CreateSemaphore(const SemaphoreCreateInfo& createInfo) {
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return nullptr;
    }
    
    auto semaphore = std::make_unique<VulkanSemaphore>();
    if (!semaphore->Initialize(m_device, createInfo)) {
        return nullptr;
    }
    
    m_semaphoreCount.fetch_add(1);
    LogSemaphoreCreation(*semaphore);
    
    return semaphore;
}

void VulkanSynchronization::DestroySemaphore(std::unique_ptr<VulkanSemaphore> semaphore) {
    if (semaphore) {
        semaphore->Shutdown();
        m_semaphoreCount--;
    }
}

std::unique_ptr<VulkanFence> VulkanSynchronization::CreateFence(const FenceCreateInfo& createInfo) {
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return nullptr;
    }
    
    auto fence = std::make_unique<VulkanFence>();
    if (!fence->Initialize(m_device, createInfo)) {
        return nullptr;
    }
    
    m_fenceCount.fetch_add(1);
    LogFenceCreation(*fence);
    
    return fence;
}

void VulkanSynchronization::DestroyFence(std::unique_ptr<VulkanFence> fence) {
    if (fence) {
        fence->Shutdown();
        m_fenceCount--;
    }
}

VkMemoryBarrier2 VulkanSynchronization::CreateMemoryBarrier(const BarrierInfo& info) const {
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = info.srcStageMask;
    barrier.srcAccessMask = info.srcAccessMask;
    barrier.dstStageMask = info.dstStageMask;
    barrier.dstAccessMask = info.dstAccessMask;
    
    m_barrierCount.fetch_add(1);
    LogBarrierCreation(info);
    
    return barrier;
}

VkBufferMemoryBarrier2 VulkanSynchronization::CreateBufferMemoryBarrier(const BarrierInfo& info) const {
    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcStageMask = info.srcStageMask;
    barrier.srcAccessMask = info.srcAccessMask;
    barrier.dstStageMask = info.dstStageMask;
    barrier.dstAccessMask = info.dstAccessMask;
    barrier.buffer = info.buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    
    m_barrierCount++;
    LogBarrierCreation(info);
    
    return barrier;
}

VkImageMemoryBarrier2 VulkanSynchronization::CreateImageMemoryBarrier(const BarrierInfo& info) const {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = info.srcStageMask;
    barrier.srcAccessMask = info.srcAccessMask;
    barrier.dstStageMask = info.dstStageMask;
    barrier.dstAccessMask = info.dstAccessMask;
    barrier.oldLayout = info.oldLayout;
    barrier.newLayout = info.newLayout;
    barrier.image = info.image;
    barrier.subresourceRange = info.subresourceRange;
    
    m_barrierCount++;
    LogBarrierCreation(info);
    
    return barrier;
}

void VulkanSynchronization::PipelineBarrier(
    VkCommandBuffer commandBuffer,
    const std::vector<VkMemoryBarrier2>& memoryBarriers,
    const std::vector<VkBufferMemoryBarrier2>& bufferBarriers,
    const std::vector<VkImageMemoryBarrier2>& imageBarriers,
    VkDependencyFlags dependencyFlags) {
    
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return;
    }
    
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.dependencyFlags = dependencyFlags;
    
    dependencyInfo.memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size());
    dependencyInfo.pMemoryBarriers = memoryBarriers.data();
    
    dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
    
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
    
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

VkSubmitInfo2 VulkanSynchronization::CreateSubmitInfo(
    const std::vector<VkCommandBufferSubmitInfo>& commandBufferInfos,
    const std::vector<VkSemaphoreSubmitInfo>& waitSemaphoreInfos,
    const std::vector<VkSemaphoreSubmitInfo>& signalSemaphoreInfos) {
    
    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    
    submitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfos.size());
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreInfos.data();
    
    submitInfo.commandBufferInfoCount = static_cast<uint32_t>(commandBufferInfos.size());
    submitInfo.pCommandBufferInfos = commandBufferInfos.data();
    
    submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfos.data();
    
    return submitInfo;
}

bool VulkanSynchronization::QueueSubmit2(
    VkQueue queue,
    const VkSubmitInfo2& submitInfo,
    VkFence fence) {
    
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return false;
    }
    
    VkResult result = vkQueueSubmit2(queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        SetError("Failed to submit queue: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    m_submitCount++;
    LogSubmit(submitInfo);
    
    return true;
}

bool VulkanSynchronization::WaitSemaphores(
    const std::vector<VkSemaphore>& semaphores,
    const std::vector<uint64_t>& values,
    [[maybe_unused]] uint64_t timeout) {
    
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return false;
    }
    
    if (semaphores.size() != values.size()) {
        SetError("Semaphore and value count mismatch");
        return false;
    }
    
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.pSemaphores = semaphores.data();
    waitInfo.pValues = values.data();
    waitInfo.semaphoreCount = static_cast<uint32_t>(semaphores.size());
    
    auto startTime = GetTimeNanoseconds();
    VkResult result = vkWaitSemaphores(m_device->GetDevice(), &waitInfo, timeout);
    auto endTime = GetTimeNanoseconds();
    
    UpdateWaitTime(endTime - startTime);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to wait for semaphores: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

bool VulkanSynchronization::SignalSemaphores(
    const std::vector<VkSemaphore>& semaphores,
    const std::vector<uint64_t>& values) {
    
    if (!m_initialized) {
        SetError("Synchronization manager not initialized");
        return false;
    }
    
    if (semaphores.size() != values.size()) {
        SetError("Semaphore and value count mismatch");
        return false;
    }
    
    auto totalStartTime = GetTimeNanoseconds();
    
    for (size_t i = 0; i < semaphores.size(); ++i) {
        VkSemaphoreSignalInfo signalInfo{};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.semaphore = semaphores[i];
        signalInfo.value = values[i];
        
        auto startTime = GetTimeNanoseconds();
        VkResult result = vkSignalSemaphore(m_device->GetDevice(), &signalInfo);
        auto endTime = GetTimeNanoseconds();
        
        UpdateSignalTime(endTime - startTime);
        
        if (result != VK_SUCCESS) {
            SetError("Failed to signal semaphore: " + VulkanUtils::GetVkResultString(result));
            return false;
        }
    }
    
    auto totalEndTime = GetTimeNanoseconds();
    UpdateSignalTime(totalEndTime - totalStartTime);
    
    return true;
}

VulkanSynchronization::Statistics VulkanSynchronization::GetStatistics() const {
    Statistics stats;
    stats.semaphoreCount = m_semaphoreCount.load();
    stats.fenceCount = m_fenceCount.load();
    stats.barrierCount = m_barrierCount.load();
    stats.submitCount = m_submitCount.load();
    stats.totalWaitTime = m_totalWaitTime.load();
    stats.totalSignalTime = m_totalSignalTime.load();
    return stats;
}

std::string VulkanSynchronization::GetDebugReport() const {
    Statistics stats = GetStatistics();
    
    std::ostringstream oss;
    oss << "=== VulkanSynchronization Debug Report ===\n";
    oss << "Semaphore Count: " << stats.semaphoreCount << "\n";
    oss << "Fence Count: " << stats.fenceCount << "\n";
    oss << "Barrier Count: " << stats.barrierCount << "\n";
    oss << "Submit Count: " << stats.submitCount << "\n";
    oss << "Total Wait Time: " << SyncUtils::FormatTimeNanoseconds(stats.totalWaitTime) << "\n";
    oss << "Total Signal Time: " << SyncUtils::FormatTimeNanoseconds(stats.totalSignalTime) << "\n";
    oss << "Timeline Semaphore Supported: " << (m_timelineSemaphoreSupported ? "Yes" : "No") << "\n";
    
    return oss.str();
}

// Private metod implementasyonları
bool VulkanSynchronization::CheckTimelineSemaphoreSupport() {
    if (!m_device) {
        return false;
    }
    
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{};
    timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    deviceFeatures2.pNext = &timelineSemaphoreFeatures;
    
    VkPhysicalDevice physicalDevice = m_device->GetPhysicalDevice();
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);
    
    return timelineSemaphoreFeatures.timelineSemaphore == VK_TRUE;
}

bool VulkanSynchronization::ValidateConfig(const Config& config) {
    if (config.maxSemaphores == 0) {
        SetError("Max semaphores cannot be zero");
        return false;
    }
    
    if (config.maxFences == 0) {
        SetError("Max fences cannot be zero");
        return false;
    }
    
    if (config.defaultTimeout == 0) {
        SetError("Default timeout cannot be zero");
        return false;
    }
    
    return true;
}

void VulkanSynchronization::LogSemaphoreCreation(const VulkanSemaphore& semaphore) const {
    std::ostringstream oss;
    oss << "Created semaphore: " << semaphore.GetDebugName() 
        << " [Type: " << (semaphore.GetType() == SemaphoreType::Timeline ? "Timeline" : "Binary") << "]";
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanSynchronization::LogFenceCreation(const VulkanFence& fence) const {
    std::ostringstream oss;
    oss << "Created fence: " << fence.GetDebugName();
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanSynchronization::LogBarrierCreation(const BarrierInfo& info) const {
    std::ostringstream oss;
    oss << "Created barrier: " << info.debugName 
        << " [Src: " << SyncUtils::PipelineStageFlagsToString(info.srcStageMask)
        << " -> Dst: " << SyncUtils::PipelineStageFlagsToString(info.dstStageMask) << "]";
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanSynchronization::LogSubmit(const VkSubmitInfo2& submitInfo) const {
    std::ostringstream oss;
    oss << "Queue submit: " << SyncUtils::FormatSubmitInfo(submitInfo);
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanSynchronization::SetError(const std::string& error) {
    m_lastError = error;
    VulkanUtils::LogError(error, __FILE__, __LINE__);
}

void VulkanSynchronization::ClearError() {
    m_lastError.clear();
}

uint64_t VulkanSynchronization::GetTimeNanoseconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

void VulkanSynchronization::UpdateWaitTime(uint64_t duration) {
    m_totalWaitTime += duration;
}

void VulkanSynchronization::UpdateSignalTime(uint64_t duration) {
    m_totalSignalTime += duration;
}

// SyncUtils namespace implementasyonu
namespace SyncUtils {

std::string PipelineStageFlagsToString(VkPipelineStageFlags2 flags) {
    std::vector<std::pair<VkPipelineStageFlagBits2, const char*>> stageNames = {
        {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, "TOP_OF_PIPE"},
        {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, "DRAW_INDIRECT"},
        {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, "VERTEX_INPUT"},
        {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, "VERTEX_SHADER"},
        {VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT, "TESSELLATION_CONTROL_SHADER"},
        {VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT, "TESSELLATION_EVALUATION_SHADER"},
        {VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT, "GEOMETRY_SHADER"},
        {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, "FRAGMENT_SHADER"},
        {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, "EARLY_FRAGMENT_TESTS"},
        {VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, "LATE_FRAGMENT_TESTS"},
        {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, "COLOR_ATTACHMENT_OUTPUT"},
        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, "COMPUTE_SHADER"},
        {VK_PIPELINE_STAGE_2_TRANSFER_BIT, "TRANSFER"},
        {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, "BOTTOM_OF_PIPE"},
        {VK_PIPELINE_STAGE_2_HOST_BIT, "HOST"},
        {VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, "ALL_GRAPHICS"},
        {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "ALL_COMMANDS"},
        {VK_PIPELINE_STAGE_2_COPY_BIT, "COPY"},
        {VK_PIPELINE_STAGE_2_RESOLVE_BIT, "RESOLVE"},
        {VK_PIPELINE_STAGE_2_BLIT_BIT, "BLIT"},
        {VK_PIPELINE_STAGE_2_CLEAR_BIT, "CLEAR"},
        {VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, "INDEX_INPUT"},
        {VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, "VERTEX_ATTRIBUTE_INPUT"},
        {VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT, "PRE_RASTERIZATION_SHADERS"}
    };
    
    if (flags == 0) {
        return "NONE";
    }
    
    std::vector<std::string> activeFlags;
    
    for (const auto& stagePair : stageNames) {
        if (flags & stagePair.first) {
            activeFlags.push_back(stagePair.second);
        }
    }
    
    if (activeFlags.empty()) {
        std::ostringstream oss;
        oss << "UNKNOWN_" << std::hex << static_cast<uint32_t>(flags);
        return oss.str();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < activeFlags.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << activeFlags[i];
    }
    
    return oss.str();
}

std::string AccessFlagsToString(VkAccessFlags2 flags) {
    std::vector<std::pair<VkAccessFlagBits2, const char*>> accessNames = {
        {VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, "INDIRECT_COMMAND_READ"},
        {VK_ACCESS_2_INDEX_READ_BIT, "INDEX_READ"},
        {VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, "VERTEX_ATTRIBUTE_READ"},
        {VK_ACCESS_2_UNIFORM_READ_BIT, "UNIFORM_READ"},
        {VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT, "INPUT_ATTACHMENT_READ"},
        {VK_ACCESS_2_SHADER_READ_BIT, "SHADER_READ"},
        {VK_ACCESS_2_SHADER_WRITE_BIT, "SHADER_WRITE"},
        {VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, "COLOR_ATTACHMENT_READ"},
        {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, "COLOR_ATTACHMENT_WRITE"},
        {VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, "DEPTH_STENCIL_ATTACHMENT_READ"},
        {VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "DEPTH_STENCIL_ATTACHMENT_WRITE"},
        {VK_ACCESS_2_TRANSFER_READ_BIT, "TRANSFER_READ"},
        {VK_ACCESS_2_TRANSFER_WRITE_BIT, "TRANSFER_WRITE"},
        {VK_ACCESS_2_HOST_READ_BIT, "HOST_READ"},
        {VK_ACCESS_2_HOST_WRITE_BIT, "HOST_WRITE"},
        {VK_ACCESS_2_MEMORY_READ_BIT, "MEMORY_READ"},
        {VK_ACCESS_2_MEMORY_WRITE_BIT, "MEMORY_WRITE"},
        {VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, "SHADER_SAMPLED_READ"},
        {VK_ACCESS_2_SHADER_STORAGE_READ_BIT, "SHADER_STORAGE_READ"},
        {VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, "SHADER_STORAGE_WRITE"}
    };
    
    if (flags == 0) {
        return "NONE";
    }
    
    std::vector<std::string> activeFlags;
    
    for (const auto& accessPair : accessNames) {
        if (flags & accessPair.first) {
            activeFlags.push_back(accessPair.second);
        }
    }
    
    if (activeFlags.empty()) {
        std::ostringstream oss;
        oss << "UNKNOWN_" << std::hex << static_cast<uint32_t>(flags);
        return oss.str();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < activeFlags.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << activeFlags[i];
    }
    
    return oss.str();
}

std::string ImageLayoutToString(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR: return "SHARED_PRESENT_KHR";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return "DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return "DEPTH_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return "STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return "STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return "READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return "ATTACHMENT_OPTIMAL";
        default: return "UNKNOWN";
    }
}

std::string DependencyFlagsToString(VkDependencyFlags flags) {
    if (flags == 0) {
        return "NONE";
    }
    
    std::vector<std::string> activeFlags;
    
    if (flags & VK_DEPENDENCY_BY_REGION_BIT) {
        activeFlags.push_back("BY_REGION");
    }
    if (flags & VK_DEPENDENCY_DEVICE_GROUP_BIT) {
        activeFlags.push_back("DEVICE_GROUP");
    }
    if (flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) {
        activeFlags.push_back("VIEW_LOCAL");
    }
    
    if (activeFlags.empty()) {
        std::ostringstream oss;
        oss << "UNKNOWN_" << std::hex << static_cast<uint32_t>(flags);
        return oss.str();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < activeFlags.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << activeFlags[i];
    }
    
    return oss.str();
}

std::string FormatSubmitInfo(const VkSubmitInfo2& submitInfo) {
    std::ostringstream oss;
    oss << "SubmitInfo [Wait: " << submitInfo.waitSemaphoreInfoCount
        << ", Commands: " << submitInfo.commandBufferInfoCount
        << ", Signal: " << submitInfo.signalSemaphoreInfoCount << "]";
    return oss.str();
}

std::string FormatBarrierInfo(const BarrierInfo& info) {
    std::ostringstream oss;
    oss << "Barrier [Src: " << PipelineStageFlagsToString(info.srcStageMask)
        << " -> Dst: " << PipelineStageFlagsToString(info.dstStageMask)
        << ", Access: " << AccessFlagsToString(info.srcAccessMask)
        << " -> " << AccessFlagsToString(info.dstAccessMask) << "]";
    return oss.str();
}

std::string FormatTimeNanoseconds(uint64_t nanoseconds) {
    const char* units[] = {"ns", "μs", "ms", "s"};
    int unitIndex = 0;
    double time = static_cast<double>(nanoseconds);
    
    while (time >= 1000.0 && unitIndex < 3) {
        time /= 1000.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << time << " " << units[unitIndex];
    return oss.str();
}

} // namespace SyncUtils

} // namespace AstralEngine
