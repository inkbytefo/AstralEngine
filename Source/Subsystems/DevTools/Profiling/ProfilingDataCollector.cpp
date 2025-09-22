#include "ProfilingDataCollector.h"
#include "../../../Core/Engine.h"
#include "../../../Core/Logger.h"
#include "../../../Subsystems/Renderer/GraphicsDevice.h"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace AstralEngine {

ProfilingDataCollector& ProfilingDataCollector::GetInstance() {
    static ProfilingDataCollector instance;
    return instance;
}

ProfilingDataCollector::ProfilingDataCollector() {
    Logger::Info("ProfilingDataCollector", "ProfilingDataCollector oluşturuluyor");
    
    // Engine referansını al
    // Not: Bu kısım Engine başlatıldıktan sonra çağrılmalı
    m_currentFrame.timestamp = std::chrono::system_clock::now();
}

void ProfilingDataCollector::BeginFrame() {
    if (!m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Frame başlangıç zamanını kaydet
    m_currentFrame.timestamp = std::chrono::system_clock::now();
    
    // Frame verilerini sıfırla
    m_currentFrame.totalFrameTime = 0.0f;
    m_currentFrame.cpuTime = 0.0f;
    m_currentFrame.gpuTime = 0.0f;
    m_currentFrame.drawCalls = 0;
    m_currentFrame.triangles = 0;
    m_currentFrame.memoryUsage = 0;
    m_currentFrame.subsystemStats.clear();
    
    // GPU profilling için query index'ini sıfırla
    m_currentQueryIndex = 0;
}

void ProfilingDataCollector::EndFrame() {
    if (!m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Toplam frame süresini hesapla
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_currentFrame.timestamp);
    m_currentFrame.totalFrameTime = duration.count() / 1000.0f; // microseconds to milliseconds
    
    // Frame geçmişine ekle
    m_frameHistory.push_back(m_currentFrame);
    
    // Frame geçmişini sınırla
    if (m_frameHistory.size() > m_maxFrameHistory) {
        m_frameHistory.erase(m_frameHistory.begin());
    }
    
    // Performans event'lerini yayınla
    PublishPerformanceEvents();
}

void ProfilingDataCollector::BeginSubsystemUpdate(const std::string& name) {
    if (!m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ProfilingScope scope;
    scope.name = name;
    scope.startTime = std::chrono::high_resolution_clock::now();
    m_scopes.push_back(scope);
}

void ProfilingDataCollector::EndSubsystemUpdate() {
    if (!m_profilingEnabled || m_scopes.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& scope = m_scopes.back();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - scope.startTime);
    float timeMs = duration.count() / 1000.0f;
    
    // Subsystem istatistiklerini güncelle
    UpdateSubsystemStats(scope.name, timeMs);
    
    // CPU zamanını ekle
    m_currentFrame.cpuTime += timeMs;
    
    m_scopes.pop_back();
}

void ProfilingDataCollector::BeginGPUProfiling(VkCommandBuffer commandBuffer, const std::string& name) {
    if (!m_profilingEnabled || m_device == VK_NULL_HANDLE) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Timestamp query'lerini başlat
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                       m_queryPools[0], m_currentQueryIndex * 2);
    
    GPUProfilingScope scope;
    scope.name = name;
    scope.queryPool = m_queryPools[0];
    scope.queryIndex = m_currentQueryIndex;
    m_gpuScopes.push_back(scope);
    
    m_currentQueryIndex++;
}

void ProfilingDataCollector::EndGPUProfiling(VkCommandBuffer commandBuffer) {
    if (!m_profilingEnabled || m_gpuScopes.empty() || m_device == VK_NULL_HANDLE) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& scope = m_gpuScopes.back();
    
    // Timestamp query'lerini bitir
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                       scope.queryPool, scope.queryIndex * 2 + 1);
    
    m_gpuScopes.pop_back();
}

void ProfilingDataCollector::AddDrawCall(uint32_t count) {
    if (!m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame.drawCalls += count;
}

void ProfilingDataCollector::AddTriangleCount(uint32_t count) {
    if (!m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame.triangles += count;
}

const ProfilingDataCollector::FrameData& ProfilingDataCollector::GetCurrentFrameData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentFrame;
}

const std::vector<ProfilingDataCollector::FrameData>& ProfilingDataCollector::GetFrameHistory(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (maxCount >= m_frameHistory.size()) {
        return m_frameHistory;
    }
    
    static std::vector<FrameData> truncatedHistory;
    truncatedHistory.clear();
    truncatedHistory.insert(truncatedHistory.end(), 
                           m_frameHistory.end() - maxCount, 
                           m_frameHistory.end());
    return truncatedHistory;
}

const ProfilingDataCollector::SubsystemStats& ProfilingDataCollector::GetSubsystemStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    static SubsystemStats emptyStats;
    auto it = m_subsystemStats.find(name);
    if (it != m_subsystemStats.end()) {
        return it->second;
    }
    return emptyStats;
}

float ProfilingDataCollector::GetAverageFPS(size_t frameCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_frameHistory.empty() || frameCount == 0) return 0.0f;
    
    size_t count = std::min(frameCount, m_frameHistory.size());
    float totalTime = 0.0f;
    
    for (size_t i = m_frameHistory.size() - count; i < m_frameHistory.size(); ++i) {
        totalTime += m_frameHistory[i].totalFrameTime;
    }
    
    if (totalTime == 0.0f) return 0.0f;
    return (count * 1000.0f) / totalTime; // Convert to FPS
}

float ProfilingDataCollector::GetAverageFrameTime(size_t frameCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_frameHistory.empty() || frameCount == 0) return 0.0f;
    
    size_t count = std::min(frameCount, m_frameHistory.size());
    float totalTime = 0.0f;
    
    for (size_t i = m_frameHistory.size() - count; i < m_frameHistory.size(); ++i) {
        totalTime += m_frameHistory[i].totalFrameTime;
    }
    
    return totalTime / count;
}

float ProfilingDataCollector::GetAverageCPUTime(size_t frameCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_frameHistory.empty() || frameCount == 0) return 0.0f;
    
    size_t count = std::min(frameCount, m_frameHistory.size());
    float totalTime = 0.0f;
    
    for (size_t i = m_frameHistory.size() - count; i < m_frameHistory.size(); ++i) {
        totalTime += m_frameHistory[i].cpuTime;
    }
    
    return totalTime / count;
}

float ProfilingDataCollector::GetAverageGPUTime(size_t frameCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_frameHistory.empty() || frameCount == 0) return 0.0f;
    
    size_t count = std::min(frameCount, m_frameHistory.size());
    float totalTime = 0.0f;
    
    for (size_t i = m_frameHistory.size() - count; i < m_frameHistory.size(); ++i) {
        totalTime += m_frameHistory[i].gpuTime;
    }
    
    return totalTime / count;
}

void ProfilingDataCollector::SetMaxFrameHistory(size_t maxFrames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxFrameHistory = maxFrames;
    
    // Frame geçmişini sınırla
    while (m_frameHistory.size() > m_maxFrameHistory) {
        m_frameHistory.erase(m_frameHistory.begin());
    }
}

void ProfilingDataCollector::SetProfilingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profilingEnabled = enabled;
    
    if (enabled) {
        InitializeGPUProfiling();
    } else {
        CleanupGPUProfiling();
    }
}

bool ProfilingDataCollector::IsProfilingEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profilingEnabled;
}

void ProfilingDataCollector::ClearFrameHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameHistory.clear();
}

void ProfilingDataCollector::ResetSubsystemStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subsystemStats.clear();
}

void ProfilingDataCollector::InitializeGPUProfiling() {
    // GPU profilling için Engine'den Vulkan device'ı al
    // Not: Bu kısım Engine başlatıldıktan sonra çağrılmalı
    if (m_engine) {
        auto* graphicsDevice = m_engine->GetSubsystem<GraphicsDevice>();
        if (graphicsDevice && graphicsDevice->IsInitialized()) {
            m_device = graphicsDevice->GetDevice();
            
            // Timestamp query pool oluştur
            VkQueryPoolCreateInfo queryPoolInfo{};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolInfo.queryCount = 100; // Maksimum 50 scope (her scope için 2 timestamp)
            
            VkQueryPool queryPool;
            if (vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &queryPool) == VK_SUCCESS) {
                m_queryPools.push_back(queryPool);
                Logger::Info("ProfilingDataCollector", "GPU profilling için query pool oluşturuldu");
            } else {
                Logger::Error("ProfilingDataCollector", "GPU profilling için query pool oluşturulamadı");
                m_device = VK_NULL_HANDLE;
            }
        }
    }
}

void ProfilingDataCollector::CleanupGPUProfiling() {
    if (m_device != VK_NULL_HANDLE) {
        for (VkQueryPool queryPool : m_queryPools) {
            vkDestroyQueryPool(m_device, queryPool, nullptr);
        }
        m_queryPools.clear();
        m_device = VK_NULL_HANDLE;
        Logger::Info("ProfilingDataCollector", "GPU profilling temizlendi");
    }
}

void ProfilingDataCollector::UpdateSubsystemStats(const std::string& name, float time) {
    auto& stats = m_subsystemStats[name];
    stats.name = name;
    stats.callCount++;
    stats.updateTime += time;
    
    if (stats.callCount == 1) {
        stats.maxTime = time;
        stats.minTime = time;
    } else {
        stats.maxTime = std::max(stats.maxTime, time);
        stats.minTime = std::min(stats.minTime, time);
    }
    
    stats.avgTime = stats.updateTime / stats.callCount;
    
    // Mevcut frame verilerine ekle
    SubsystemStats currentStats;
    currentStats.name = name;
    currentStats.updateTime = time;
    currentStats.callCount = 1;
    currentStats.avgTime = time;
    currentStats.maxTime = time;
    currentStats.minTime = time;
    m_currentFrame.subsystemStats.push_back(currentStats);
}

void ProfilingDataCollector::PublishPerformanceEvents() {
    // GPU timestamp sonuçlarını al
    if (m_device != VK_NULL_HANDLE && !m_queryPools.empty()) {
        std::vector<uint64_t> timestamps(m_queryPools.size() * 2);
        if (vkGetQueryPoolResults(m_device, m_queryPools[0], 0, timestamps.size(), 
                                 timestamps.size() * sizeof(uint64_t), timestamps.data(), 
                                 sizeof(uint64_t), VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS) {
            
            // Timestamp'leri nanosaniyeden milisaniyeye çevir
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(m_engine->GetSubsystem<GraphicsDevice>()->GetPhysicalDevice(), &deviceProperties);
            float timestampPeriod = deviceProperties.limits.timestampPeriod;
            
            for (size_t i = 0; i < timestamps.size(); i += 2) {
                if (i + 1 < timestamps.size()) {
                    uint64_t start = timestamps[i];
                    uint64_t end = timestamps[i + 1];
                    float duration = (end - start) * timestampPeriod / 1000000.0f; // nanoseconds to milliseconds
                    m_currentFrame.gpuTime += duration;
                }
            }
        }
    }
    
    // Performans event'lerini yayınla
    auto& eventSystem = DevToolsEventSystem::GetInstance();
    
    // FPS threshold kontrolü
    float fps = GetAverageFPS(60);
    if (fps < 30.0f && fps > 0.0f) {
        eventSystem.PublishPerformanceThresholdExceeded("FPS", fps, 30.0f);
    }
    
    // Frame time threshold kontrolü
    float frameTime = GetAverageFrameTime(60);
    if (frameTime > 33.3f) { // 30 FPS'den düşük
        eventSystem.PublishPerformanceThresholdExceeded("FrameTime", frameTime, 33.3f);
    }
    
    // CPU time threshold kontrolü
    float cpuTime = GetAverageCPUTime(60);
    if (cpuTime > 16.6f) { // 60 FPS'den düşük
        eventSystem.PublishPerformanceThresholdExceeded("CPUTime", cpuTime, 16.6f);
    }
    
    // GPU time threshold kontrolü
    float gpuTime = GetAverageGPUTime(60);
    if (gpuTime > 16.6f) { // 60 FPS'den düşük
        eventSystem.PublishPerformanceThresholdExceeded("GPUTime", gpuTime, 16.6f);
    }
    
    // Performans verilerini data updated event'i olarak yayınla
    PerformanceData perfData;
    perfData.cpuTime = m_currentFrame.cpuTime;
    perfData.gpuTime = m_currentFrame.gpuTime;
    perfData.drawCalls = m_currentFrame.drawCalls;
    perfData.triangles = m_currentFrame.triangles;
    perfData.memoryUsage = m_currentFrame.memoryUsage;
    perfData.timestamp = m_currentFrame.timestamp;
    
    eventSystem.PublishDataUpdated("PerformanceData", perfData);
}

} // namespace AstralEngine