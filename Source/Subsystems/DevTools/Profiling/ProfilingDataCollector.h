#pragma once
#include "../Common/DevToolsTypes.h"
#include "../Events/DevToolsEvent.h"
#include <chrono>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace AstralEngine {

class Engine;
class ISubsystem;

class ProfilingDataCollector {
public:
    struct SubsystemStats {
        std::string name;
        float updateTime = 0.0f;
        float renderTime = 0.0f;
        uint32_t entityCount = 0;
        size_t memoryUsage = 0;
        uint32_t callCount = 0;
        float maxTime = 0.0f;
        float minTime = 0.0f;
        float avgTime = 0.0f;
    };
    
    struct FrameData {
        float totalFrameTime = 0.0f;
        float cpuTime = 0.0f;
        float gpuTime = 0.0f;
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        size_t memoryUsage = 0;
        std::vector<SubsystemStats> subsystemStats;
        std::chrono::system_clock::time_point timestamp;
    };
    
    static ProfilingDataCollector& GetInstance();
    
    // Frame yönetimi
    void BeginFrame();
    void EndFrame();
    
    // Subsystem profilling
    void BeginSubsystemUpdate(const std::string& name);
    void EndSubsystemUpdate();
    
    // GPU profilling
    void BeginGPUProfiling(VkCommandBuffer commandBuffer, const std::string& name);
    void EndGPUProfiling(VkCommandBuffer commandBuffer);
    
    // Render istatistikleri
    void AddDrawCall(uint32_t count = 1);
    void AddTriangleCount(uint32_t count);
    
    // Veri erişimi
    const FrameData& GetCurrentFrameData() const;
    const std::vector<FrameData>& GetFrameHistory(size_t maxCount = 100) const;
    const SubsystemStats& GetSubsystemStats(const std::string& name) const;
    
    // Performans metrikleri
    float GetAverageFPS(size_t frameCount = 60) const;
    float GetAverageFrameTime(size_t frameCount = 60) const;
    float GetAverageCPUTime(size_t frameCount = 60) const;
    float GetAverageGPUTime(size_t frameCount = 60) const;
    
    // Ayarlar
    void SetMaxFrameHistory(size_t maxFrames);
    void SetProfilingEnabled(bool enabled);
    bool IsProfilingEnabled() const;
    
    // Veri temizleme
    void ClearFrameHistory();
    void ResetSubsystemStats();
    
private:
    ProfilingDataCollector();
    ~ProfilingDataCollector() = default;
    
    struct ProfilingScope {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    
    struct GPUProfilingScope {
        std::string name;
        VkQueryPool queryPool;
        uint32_t queryIndex;
    };
    
    Engine* m_engine = nullptr;
    std::vector<ProfilingScope> m_scopes;
    std::vector<GPUProfilingScope> m_gpuScopes;
    std::vector<FrameData> m_frameHistory;
    FrameData m_currentFrame;
    std::unordered_map<std::string, SubsystemStats> m_subsystemStats;
    
    size_t m_maxFrameHistory = 1000;
    bool m_profilingEnabled = true;
    mutable std::mutex m_mutex;
    
    // GPU profilling için Vulkan objeleri
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkQueryPool> m_queryPools;
    uint32_t m_currentQueryIndex = 0;
    
    void InitializeGPUProfiling();
    void CleanupGPUProfiling();
    void UpdateSubsystemStats(const std::string& name, float time);
    void PublishPerformanceEvents();
};

} // namespace AstralEngine