#include "ProfilerWindow.h"
#include "../../../Core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>

namespace AstralEngine {

ProfilerWindow::ProfilerWindow() {
    // Grafik verilerini başlat
    m_frameTimeGraph.label = "Frame Time";
    m_frameTimeGraph.color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Kırmızı
    
    m_fpsGraph.label = "FPS";
    m_fpsGraph.color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Yeşil
    
    m_cpuTimeGraph.label = "CPU Time";
    m_cpuTimeGraph.color = ImVec4(0.0f, 0.0f, 1.0f, 1.0f); // Mavi
    
    m_gpuTimeGraph.label = "GPU Time";
    m_gpuTimeGraph.color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Sarı
    
    m_drawCallGraph.label = "Draw Calls";
    m_drawCallGraph.color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f); // Magenta
    
    m_triangleGraph.label = "Triangles";
    m_triangleGraph.color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); // Cyan
    
    m_memoryGraph.label = "Memory Usage";
    m_memoryGraph.color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // Turuncu
    
    m_lastCacheUpdate = std::chrono::system_clock::now();
}

void ProfilerWindow::OnInitialize() {
    Logger::Info("ProfilerWindow", "ProfilerWindow başlatılıyor");
    
    // Grafik verileri için yer ayır
    m_frameTimeGraph.values.reserve(m_frameTimeGraph.maxPoints);
    m_fpsGraph.values.reserve(m_fpsGraph.maxPoints);
    m_cpuTimeGraph.values.reserve(m_cpuTimeGraph.maxPoints);
    m_gpuTimeGraph.values.reserve(m_gpuTimeGraph.maxPoints);
    m_drawCallGraph.values.reserve(m_drawCallGraph.maxPoints);
    m_triangleGraph.values.reserve(m_triangleGraph.maxPoints);
    m_memoryGraph.values.reserve(m_memoryGraph.maxPoints);
    
    Logger::Info("ProfilerWindow", "ProfilerWindow başarıyla başlatıldı");
}

void ProfilerWindow::OnUpdate(float deltaTime) {
    if (!m_enabled || m_pauseUpdates) return;
    
    m_timeSinceLastUpdate += deltaTime;
    
    // Belirli aralıklarla grafik verilerini güncelle
    if (m_timeSinceLastUpdate >= m_updateInterval) {
        UpdateGraphData();
        m_timeSinceLastUpdate = 0.0f;
    }
}

void ProfilerWindow::OnRender() {
    if (!m_enabled) return;
    
    // Ana profiler penceresi
    if (!ImGui::Begin("Profiler", &m_enabled)) {
        ImGui::End();
        return;
    }
    
    // Ana menü
    RenderMainMenu();
    
    // CPU Profiler
    if (m_showCPUProfiler) {
        RenderCPUProfiler();
    }
    
    // GPU Profiler
    if (m_showGPUProfiler) {
        RenderGPUProfiler();
    }
    
    // Memory Profiler
    if (m_showMemoryProfiler) {
        RenderMemoryProfiler();
    }
    
    // Subsystem Profiler
    if (m_showSubsystemProfiler) {
        RenderSubsystemProfiler();
    }
    
    // Ayarlar
    if (m_showSettings) {
        RenderSettings();
    }
    
    ImGui::End();
}

void ProfilerWindow::OnShutdown() {
    Logger::Info("ProfilerWindow", "ProfilerWindow kapatılıyor");
    ResetGraphs();
    m_subsystemGraphs.clear();
    Logger::Info("ProfilerWindow", "ProfilerWindow başarıyla kapatıldı");
}

void ProfilerWindow::LoadSettings(const std::string& settings) {
    // Basit bir ayar yükleme implementasyonu
    // Gerçek implementasyonda JSON veya benzeri bir format kullanılabilir
    Logger::Info("ProfilerWindow", "Ayarlar yükleniyor");
    
    // Ayarları parse et (basit implementasyon)
    if (settings.find("updateInterval:") != std::string::npos) {
        // Ayarları ayıkla ve uygula
        // Bu kısım daha gelişmiş bir ayar sistemi ile genişletilebilir
    }
}

std::string ProfilerWindow::SaveSettings() const {
    // Basit bir ayar kaydetme implementasyonu
    std::stringstream ss;
    ss << "updateInterval:" << m_updateInterval << "\n";
    ss << "maxFrameHistory:" << m_maxFrameHistory << "\n";
    ss << "showGraphValues:" << (m_showGraphValues ? "true" : "false") << "\n";
    ss << "smoothGraphs:" << (m_smoothGraphs ? "true" : "false") << "\n";
    
    Logger::Info("ProfilerWindow", "Ayarlar kaydediliyor");
    return ss.str();
}

void ProfilerWindow::RenderMainMenu() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Panels")) {
            ImGui::MenuItem("CPU Profiler", nullptr, &m_showCPUProfiler);
            ImGui::MenuItem("GPU Profiler", nullptr, &m_showGPUProfiler);
            ImGui::MenuItem("Memory Profiler", nullptr, &m_showMemoryProfiler);
            ImGui::MenuItem("Subsystem Profiler", nullptr, &m_showSubsystemProfiler);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &m_showSettings);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("Reset Graphs")) {
                ResetGraphs();
            }
            if (ImGui::MenuItem(m_pauseUpdates ? "Resume Updates" : "Pause Updates")) {
                m_pauseUpdates = !m_pauseUpdates;
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Zaman aralığı seçimi
    const char* timeRangeItems[] = { "1s", "5s", "10s", "30s", "60s" };
    ImGui::Combo("Time Range", &m_selectedTimeRange, timeRangeItems, IM_ARRAYSIZE(timeRangeItems));
}

void ProfilerWindow::RenderCPUProfiler() {
    if (!ImGui::CollapsingHeader("CPU Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    
    // Genel CPU istatistikleri
    ImGui::Text("Frame Time: %.2f ms", currentFrame.totalFrameTime);
    ImGui::Text("CPU Time: %.2f ms", currentFrame.cpuTime);
    ImGui::Text("FPS: %.1f", currentFrame.totalFrameTime > 0.0f ? 1000.0f / currentFrame.totalFrameTime : 0.0f);
    
    ImGui::Separator();
    
    // Frame time grafiği
    RenderCPUFrameTimeGraph();
    
    // FPS grafiği
    RenderCPUFPSGraph();
    
    // Subsystem breakdown
    RenderCPUSubsystemBreakdown();
}

void ProfilerWindow::RenderGPUProfiler() {
    if (!ImGui::CollapsingHeader("GPU Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    
    // Genel GPU istatistikleri
    ImGui::Text("GPU Time: %.2f ms", currentFrame.gpuTime);
    ImGui::Text("Draw Calls: %u", currentFrame.drawCalls);
    ImGui::Text("Triangles: %u", currentFrame.triangles);
    
    ImGui::Separator();
    
    // GPU frame time grafiği
    RenderGPUFrameTimeGraph();
    
    // Draw call grafiği
    RenderGPUDrawCallGraph();
    
    // Triangle grafiği
    RenderGPUTriangleGraph();
}

void ProfilerWindow::RenderMemoryProfiler() {
    if (!ImGui::CollapsingHeader("Memory Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    
    // Genel memory istatistikleri
    ImGui::Text("Memory Usage: %zu MB", currentFrame.memoryUsage / (1024 * 1024));
    
    ImGui::Separator();
    
    // Memory usage grafiği
    RenderMemoryUsageGraph();
    
    // Memory breakdown
    RenderMemoryBreakdown();
}

void ProfilerWindow::RenderSubsystemProfiler() {
    if (!ImGui::CollapsingHeader("Subsystem Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    // Subsystem tablosu
    RenderSubsystemTable();
    
    // Seçili subsystem grafiği
    if (!m_selectedSubsystem.empty()) {
        RenderSubsystemGraph(m_selectedSubsystem);
    }
}

void ProfilerWindow::RenderCPUFrameTimeGraph() {
    RenderGraph("Frame Time (ms)", m_frameTimeGraph.values, m_frameTimeGraph.color, 0.0f, 50.0f);
}

void ProfilerWindow::RenderCPUFPSGraph() {
    RenderGraph("FPS", m_fpsGraph.values, m_fpsGraph.color, 0.0f, 120.0f);
}

void ProfilerWindow::RenderCPUSubsystemBreakdown() {
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    
    if (currentFrame.subsystemStats.empty()) {
        ImGui::Text("No subsystem data available");
        return;
    }
    
    ImGui::Text("Subsystem Breakdown:");
    
    float maxTime = 0.0f;
    for (const auto& stats : currentFrame.subsystemStats) {
        maxTime = std::max(maxTime, stats.updateTime);
    }
    
    for (const auto& stats : currentFrame.subsystemStats) {
        RenderTimeBar(stats.name.c_str(), stats.updateTime, maxTime, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    }
}

void ProfilerWindow::RenderGPUFrameTimeGraph() {
    RenderGraph("GPU Time (ms)", m_gpuTimeGraph.values, m_gpuTimeGraph.color, 0.0f, 50.0f);
}

void ProfilerWindow::RenderGPUDrawCallGraph() {
    RenderGraph("Draw Calls", m_drawCallGraph.values, m_drawCallGraph.color, 0.0f, 1000.0f);
}

void ProfilerWindow::RenderGPUTriangleGraph() {
    RenderGraph("Triangles", m_triangleGraph.values, m_triangleGraph.color, 0.0f, 100000.0f);
}

void ProfilerWindow::RenderMemoryUsageGraph() {
    RenderGraph("Memory (MB)", m_memoryGraph.values, m_memoryGraph.color, 0.0f, 1024.0f);
}

void ProfilerWindow::RenderMemoryBreakdown() {
    // Basit bir memory breakdown gösterimi
    ImGui::Text("Memory Breakdown:");
    
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    size_t totalMemory = currentFrame.memoryUsage;
    
    if (totalMemory == 0) {
        ImGui::Text("No memory data available");
        return;
    }
    
    // Basit bir memory dağılımı gösterimi
    // Gerçek implementasyonda daha detaylı memory tracking yapılabilir
    float texturesMemory = totalMemory * 0.4f; // %40 textures
    float meshesMemory = totalMemory * 0.3f;   // %30 meshes
    float shadersMemory = totalMemory * 0.2f;  // %20 shaders
    float otherMemory = totalMemory * 0.1f;    // %10 other
    
    RenderTimeBar("Textures", texturesMemory / (1024 * 1024), totalMemory / (1024 * 1024), ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%.1f MB");
    RenderTimeBar("Meshes", meshesMemory / (1024 * 1024), totalMemory / (1024 * 1024), ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "%.1f MB");
    RenderTimeBar("Shaders", shadersMemory / (1024 * 1024), totalMemory / (1024 * 1024), ImVec4(0.5f, 0.0f, 1.0f, 1.0f), "%.1f MB");
    RenderTimeBar("Other", otherMemory / (1024 * 1024), totalMemory / (1024 * 1024), ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.1f MB");
}

void ProfilerWindow::RenderSubsystemTable() {
    const auto& currentFrame = ProfilingDataCollector::GetInstance().GetCurrentFrameData();
    
    if (currentFrame.subsystemStats.empty()) {
        ImGui::Text("No subsystem data available");
        return;
    }
    
    // Subsystem tablosu
    if (ImGui::BeginTable("SubsystemTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Subsystem");
        ImGui::TableSetupColumn("Time (ms)");
        ImGui::TableSetupColumn("Calls");
        ImGui::TableSetupColumn("Avg (ms)");
        ImGui::TableSetupColumn("Max (ms)");
        ImGui::TableHeadersRow();
        
        for (const auto& stats : currentFrame.subsystemStats) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            
            // Subsystem adı ve seçim butonu
            if (ImGui::Selectable(stats.name.c_str(), m_selectedSubsystem == stats.name, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedSubsystem = (m_selectedSubsystem == stats.name) ? "" : stats.name;
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stats.updateTime);
            
            ImGui::TableNextColumn();
            ImGui::Text("%u", stats.callCount);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stats.avgTime);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stats.maxTime);
        }
        
        ImGui::EndTable();
    }
}

void ProfilerWindow::RenderSubsystemGraph(const std::string& subsystemName) {
    auto it = m_subsystemGraphs.find(subsystemName);
    if (it == m_subsystemGraphs.end()) {
        // Yeni bir grafik oluştur
        GraphData graph;
        graph.label = subsystemName + " Time";
        graph.color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); // Sarımsı renk
        graph.maxPoints = 200;
        graph.values.reserve(graph.maxPoints);
        m_subsystemGraphs[subsystemName] = graph;
        it = m_subsystemGraphs.find(subsystemName);
    }
    
    ImGui::Separator();
    ImGui::Text("%s Performance", subsystemName.c_str());
    RenderGraph(it->second.label.c_str(), it->second.values, it->second.color, 0.0f, 50.0f);
}

void ProfilerWindow::UpdateGraphData() {
    auto& collector = ProfilingDataCollector::GetInstance();
    
    // Frame geçmişini al
    const auto& frameHistory = collector.GetFrameHistory(m_maxFrameHistory);
    
    if (frameHistory.empty()) return;
    
    // Zaman aralığına göre veri miktarını belirle
    size_t dataPoints = 0;
    switch (m_selectedTimeRange) {
        case 0: dataPoints = 60; break;   // 1s (60 FPS)
        case 1: dataPoints = 300; break;  // 5s
        case 2: dataPoints = 600; break;  // 10s
        case 3: dataPoints = 1800; break; // 30s
        case 4: dataPoints = 3600; break; // 60s
        default: dataPoints = 60; break;
    }
    
    dataPoints = std::min(dataPoints, frameHistory.size());
    
    // Grafik verilerini güncelle
    m_frameTimeGraph.values.clear();
    m_fpsGraph.values.clear();
    m_cpuTimeGraph.values.clear();
    m_gpuTimeGraph.values.clear();
    m_drawCallGraph.values.clear();
    m_triangleGraph.values.clear();
    m_memoryGraph.values.clear();
    
    for (size_t i = frameHistory.size() - dataPoints; i < frameHistory.size(); ++i) {
        const auto& frame = frameHistory[i];
        
        m_frameTimeGraph.values.push_back(frame.totalFrameTime);
        m_fpsGraph.values.push_back(frame.totalFrameTime > 0.0f ? 1000.0f / frame.totalFrameTime : 0.0f);
        m_cpuTimeGraph.values.push_back(frame.cpuTime);
        m_gpuTimeGraph.values.push_back(frame.gpuTime);
        m_drawCallGraph.values.push_back(static_cast<float>(frame.drawCalls));
        m_triangleGraph.values.push_back(static_cast<float>(frame.triangles));
        m_memoryGraph.values.push_back(static_cast<float>(frame.memoryUsage / (1024 * 1024)));
    }
    
    // Subsystem grafiklerini güncelle
    for (auto& [subsystemName, graph] : m_subsystemGraphs) {
        graph.values.clear();
        for (size_t i = frameHistory.size() - dataPoints; i < frameHistory.size(); ++i) {
            const auto& frame = frameHistory[i];
            float subsystemTime = 0.0f;
            
            // Bu subsystem için zamanı bul
            for (const auto& stats : frame.subsystemStats) {
                if (stats.name == subsystemName) {
                    subsystemTime = stats.updateTime;
                    break;
                }
            }
            
            graph.values.push_back(subsystemTime);
        }
    }
    
    // Min/max değerleri güncelle
    auto updateMinMax = [](GraphData& graph) {
        if (graph.values.empty()) {
            graph.minValue = 0.0f;
            graph.maxValue = 1.0f;
            return;
        }
        
        graph.minValue = FLT_MAX;
        graph.maxValue = FLT_MIN;
        
        for (float value : graph.values) {
            graph.minValue = std::min(graph.minValue, value);
            graph.maxValue = std::max(graph.maxValue, value);
        }
    };
    
    updateMinMax(m_frameTimeGraph);
    updateMinMax(m_fpsGraph);
    updateMinMax(m_cpuTimeGraph);
    updateMinMax(m_gpuTimeGraph);
    updateMinMax(m_drawCallGraph);
    updateMinMax(m_triangleGraph);
    updateMinMax(m_memoryGraph);
    
    for (auto& [subsystemName, graph] : m_subsystemGraphs) {
        updateMinMax(graph);
    }
}

void ProfilerWindow::ResetGraphs() {
    m_frameTimeGraph.values.clear();
    m_fpsGraph.values.clear();
    m_cpuTimeGraph.values.clear();
    m_gpuTimeGraph.values.clear();
    m_drawCallGraph.values.clear();
    m_triangleGraph.values.clear();
    m_memoryGraph.values.clear();
    
    for (auto& [subsystemName, graph] : m_subsystemGraphs) {
        graph.values.clear();
    }
    
    Logger::Info("ProfilerWindow", "Grafik verileri sıfırlandı");
}

void ProfilerWindow::RenderGraph(const std::string& label, const std::vector<float>& data, 
                                 const ImVec4& color, float minValue, float maxValue) {
    if (data.empty()) {
        ImGui::Text("No data available for %s", label.c_str());
        return;
    }
    
    // Grafik boyutunu ayarla
    ImVec2 graphSize = ImVec2(-1, 100); // Otomatik genişlik, 100px yükseklik
    
    // Değer aralığını hesapla
    float actualMinValue = minValue;
    float actualMaxValue = maxValue;
    
    if (minValue == 0.0f && maxValue == 0.0f) {
        // Otomatik aralık belirle
        actualMinValue = FLT_MAX;
        actualMaxValue = FLT_MIN;
        
        for (float value : data) {
            actualMinValue = std::min(actualMinValue, value);
            actualMaxValue = std::max(actualMaxValue, value);
        }
        
        if (actualMinValue == actualMaxValue) {
            actualMaxValue = actualMinValue + 1.0f;
        }
    }
    
    // Grafiği çiz
    ImGui::PushStyleColor(ImGuiCol_PlotLines, color);
    
    if (m_smoothGraphs) {
        ImGui::PlotLines(label.c_str(), data.data(), static_cast<int>(data.size()), 0, nullptr, 
                        actualMinValue, actualMaxValue, graphSize);
    } else {
        // Smooth olmayan çizim için basit bir implementasyon
        ImGui::PlotLines(label.c_str(), data.data(), static_cast<int>(data.size()), 0, nullptr, 
                        actualMinValue, actualMaxValue, graphSize);
    }
    
    ImGui::PopStyleColor();
    
    // Değerleri göster
    if (m_showGraphValues && !data.empty()) {
        float currentValue = data.back();
        float avgValue = 0.0f;
        
        for (float value : data) {
            avgValue += value;
        }
        avgValue /= data.size();
        
        ImGui::SameLine();
        ImGui::Text("Current: %.2f, Avg: %.2f", currentValue, avgValue);
    }
}

void ProfilerWindow::RenderTimeBar(const std::string& label, float time, float maxTime, 
                                   const ImVec4& color, const std::string& format) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    
    float progress = (maxTime > 0.0f) ? (time / maxTime) : 0.0f;
    ImVec2 barSize = ImVec2(-1, 20); // Otomatik genişlik, 20px yükseklik
    
    ImGui::ProgressBar(progress, barSize, "");
    ImGui::SameLine();
    ImGui::Text("%s: %s", label.c_str(), format.c_str(), time);
    
    ImGui::PopStyleColor();
}

} // namespace AstralEngine