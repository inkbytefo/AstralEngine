#pragma once
#include "../Interfaces/IDeveloperTool.h"
#include "../Profiling/ProfilingDataCollector.h"
#include "../Common/DevToolsTypes.h"
#include <memory>
#include <vector>
#include <string>

// Imgui forward declarations to avoid including the full header
struct ImVec4;

namespace AstralEngine {

class ProfilerWindow : public IDeveloperTool {
public:
    ProfilerWindow();
    ~ProfilerWindow() = default;
    
    // IDeveloperTool implementasyonu
    void OnInitialize() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
    
    const std::string& GetName() const override { return m_name; }
    bool IsEnabled() const override { return m_enabled; }
    void SetEnabled(bool enabled) override { m_enabled = enabled; }
    
    void LoadSettings(const std::string& settings) override;
    std::string SaveSettings() const override;
    
private:
    // Ana render metodları
    void RenderMainMenu();
    void RenderCPUProfiler();
    void RenderGPUProfiler();
    void RenderMemoryProfiler();
    void RenderSubsystemProfiler();
    void RenderSettings();
    
    // CPU Profiler metodları
    void RenderCPUFrameTimeGraph();
    void RenderCPUFPSGraph();
    void RenderCPUSubsystemBreakdown();
    
    // GPU Profiler metodları
    void RenderGPUFrameTimeGraph();
    void RenderGPUDrawCallGraph();
    void RenderGPUTriangleGraph();
    
    // Memory Profiler metodları
    void RenderMemoryUsageGraph();
    void RenderMemoryBreakdown();
    
    // Subsystem Profiler metodları
    void RenderSubsystemTable();
    void RenderSubsystemGraph(const std::string& subsystemName);
    
    // Yardımcı metodlar
    void UpdateGraphData();
    void ResetGraphs();
    void RenderGraph(const std::string& label, const std::vector<float>& data, 
                     const ImVec4& color, float minValue = 0.0f, float maxValue = 0.0f);
    void RenderTimeBar(const std::string& label, float time, float maxTime, 
                       const ImVec4& color, const std::string& format = "%.2f ms");
    
    // Veri yapıları
    struct GraphData {
        std::vector<float> values;
        size_t maxPoints = 200;
        float minValue = FLT_MAX;
        float maxValue = FLT_MIN;
        ImVec4 color;
        std::string label;
        
        GraphData() : color(1.0f, 1.0f, 1.0f, 1.0f) {}
    };
    
    std::string m_name = "Profiler";
    bool m_enabled = true;
    bool m_showCPUProfiler = true;
    bool m_showGPUProfiler = true;
    bool m_showMemoryProfiler = true;
    bool m_showSubsystemProfiler = true;
    bool m_showSettings = false;
    
    // Grafik verileri
    GraphData m_frameTimeGraph;
    GraphData m_fpsGraph;
    GraphData m_cpuTimeGraph;
    GraphData m_gpuTimeGraph;
    GraphData m_drawCallGraph;
    GraphData m_triangleGraph;
    GraphData m_memoryGraph;
    
    // Subsystem grafik verileri
    std::unordered_map<std::string, GraphData> m_subsystemGraphs;
    
    // Ayarlar
    float m_updateInterval = 0.5f;
    float m_timeSinceLastUpdate = 0.0f;
    size_t m_maxFrameHistory = 100;
    bool m_pauseUpdates = false;
    bool m_showGraphValues = true;
    bool m_smoothGraphs = true;
    
    // Seçimler
    std::string m_selectedSubsystem = "";
    int m_selectedTimeRange = 0; // 0: 1s, 1: 5s, 2: 10s, 3: 30s, 4: 60s
    
    // Performans verileri için cache
    std::vector<ProfilingDataCollector::FrameData> m_frameDataCache;
    std::chrono::system_clock::time_point m_lastCacheUpdate;
};

} // namespace AstralEngine