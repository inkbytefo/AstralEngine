#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Astral {

struct FrameMetric {
    uint32_t frameIndex = 0;
    double cpuFrameMs = 0.0;
    double gpuTotalMs = 0.0;
    double gpuComputeMs = 0.0;
    double gpuRenderMs = 0.0;
    int screenW = 1280;
    int screenH = 720;
    std::string gpuName;
    std::string driverVersion;
};

struct BenchmarkStats {
    double avg = 0.0;
    double min = 0.0;
    double max = 0.0;
    double p50 = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double stddev = 0.0;
};

class BenchmarkLogger {
public:
    BenchmarkLogger() = default;

    void LogFrame(const FrameMetric& metric);
    void Clear();

    const std::vector<FrameMetric>& GetMetrics() const { return m_Metrics; }
    size_t GetFrameCount() const { return m_Metrics.size(); }

    BenchmarkStats CalculateStats(const std::vector<double>& values) const;
    void PrintSummary(uint32_t warmupFrames = 30) const;

    bool WriteCSV(const std::string& filepath) const;
    bool WriteSummaryJSON(const std::string& filepath, uint32_t warmupFrames = 30) const;

private:
    std::vector<FrameMetric> m_Metrics;
};

} // namespace Astral
