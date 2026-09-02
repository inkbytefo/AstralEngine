#include "Astral/Core/BenchmarkLogger.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace Astral {

void BenchmarkLogger::LogFrame(const FrameMetric& metric) {
    m_Metrics.push_back(metric);
}

void BenchmarkLogger::Clear() {
    m_Metrics.clear();
}

BenchmarkStats BenchmarkLogger::CalculateStats(const std::vector<double>& rawValues) const {
    BenchmarkStats stats{};
    if (rawValues.empty()) return stats;

    std::vector<double> values = rawValues;
    std::sort(values.begin(), values.end());

    stats.min = values.front();
    stats.max = values.back();

    double sum = 0.0;
    for (double val : values) {
        sum += val;
    }
    stats.avg = sum / static_cast<double>(values.size());

    // Persentil hesaplari
    size_t n = values.size();
    stats.p50 = values[static_cast<size_t>(n * 0.50)];
    stats.p90 = values[std::min(static_cast<size_t>(n * 0.90), n - 1)];
    stats.p95 = values[std::min(static_cast<size_t>(n * 0.95), n - 1)];

    // Standart sapma
    double varianceSum = 0.0;
    for (double val : values) {
        double diff = val - stats.avg;
        varianceSum += diff * diff;
    }
    stats.stddev = std::sqrt(varianceSum / static_cast<double>(values.size()));

    return stats;
}

void BenchmarkLogger::PrintSummary(uint32_t warmupFrames) const {
    if (m_Metrics.empty()) {
        std::cout << "[BenchmarkLogger] Hic metrik kaydedilmedi.\n";
        return;
    }

    std::vector<double> gpuTimes;
    std::vector<double> cpuTimes;

    size_t startIdx = (m_Metrics.size() > warmupFrames) ? warmupFrames : 0;
    for (size_t i = startIdx; i < m_Metrics.size(); ++i) {
        gpuTimes.push_back(m_Metrics[i].gpuTotalMs);
        cpuTimes.push_back(m_Metrics[i].cpuFrameMs);
    }

    auto gpuStats = CalculateStats(gpuTimes);
    auto cpuStats = CalculateStats(cpuTimes);

    auto printRow = [](const char* name, const BenchmarkStats& s) {
        int prec = (s.avg > 0.0 && s.avg < 0.01) ? 5 : 3;
        std::cout << std::left << std::setw(14) << name << " | "
                  << std::right << std::fixed << std::setprecision(prec)
                  << std::setw(7) << s.avg << " | "
                  << std::setw(7) << s.min << " | "
                  << std::setw(7) << s.max << " | "
                  << std::setw(7) << s.p50 << " | "
                  << std::setw(7) << s.p95 << " |\n";
    };

    std::cout << "\n=================================================================\n";
    std::cout << "                  ASTRAL ENGINE BENCHMARK RAPORU                 \n";
    std::cout << "=================================================================\n";
    std::cout << " GPU Modeli      : " << m_Metrics.front().gpuName << "\n";
    std::cout << " Surucu          : " << m_Metrics.front().driverVersion << "\n";
    std::cout << " Ekran Boyutu    : " << m_Metrics.front().screenW << "x" << m_Metrics.front().screenH << "\n";
    std::cout << " Toplam Kare     : " << m_Metrics.size() << " (Isinma atlanan: " << startIdx << ")\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " Metrik (ms)   |   Ort   |   Min   |   Max   |   p50   |   p95   |\n";
    std::cout << "---------------+---------+---------+---------+---------+---------+\n";
    printRow(" GPU Zamani", gpuStats);
    printRow(" CPU Zamani", cpuStats);
    std::cout << "=================================================================\n\n";
}

bool BenchmarkLogger::WriteCSV(const std::string& filepath) const {
    if (m_Metrics.empty()) return false;

    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[BenchmarkLogger] CSV dosyasi acilamadi: " << filepath << "\n";
            return false;
        }

        // CSV Basligi (RENDERER_ARCHITECTURE.md semasina tam uyumlu)
        file << "frameIndex,cpuFrameMs,gpuTotalMs,gpuComputeMs,gpuRenderMs,screenW,screenH,driverVersion,gpuName\n";
        file << std::fixed << std::setprecision(3);

        for (const auto& m : m_Metrics) {
            file << m.frameIndex << ","
                 << m.cpuFrameMs << ","
                 << m.gpuTotalMs << ","
                 << m.gpuComputeMs << ","
                 << m.gpuRenderMs << ","
                 << m.screenW << ","
                 << m.screenH << ","
                 << m.driverVersion << ",\""
                 << m.gpuName << "\"\n";
        }

        std::cout << "[BenchmarkLogger] CSV basariyla kaydedildi: " << filepath << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BenchmarkLogger Hata]: " << e.what() << "\n";
        return false;
    }
}

bool BenchmarkLogger::WriteSummaryJSON(const std::string& filepath, uint32_t warmupFrames) const {
    if (m_Metrics.empty()) return false;

    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        std::vector<double> gpuTimes;
        std::vector<double> cpuTimes;

        size_t startIdx = (m_Metrics.size() > warmupFrames) ? warmupFrames : 0;
        for (size_t i = startIdx; i < m_Metrics.size(); ++i) {
            gpuTimes.push_back(m_Metrics[i].gpuTotalMs);
            cpuTimes.push_back(m_Metrics[i].cpuFrameMs);
        }

        auto gpuStats = CalculateStats(gpuTimes);
        auto cpuStats = CalculateStats(cpuTimes);

        file << "{\n";
        file << "  \"benchmark_metadata\": {\n";
        file << "    \"gpu_name\": \"" << m_Metrics.front().gpuName << "\",\n";
        file << "    \"driver_version\": \"" << m_Metrics.front().driverVersion << "\",\n";
        file << "    \"screen_width\": " << m_Metrics.front().screenW << ",\n";
        file << "    \"screen_height\": " << m_Metrics.front().screenH << "\n";
        file << "  },\n";
        file << "  \"metrics\": {\n";
        file << "    \"total_frames\": " << m_Metrics.size() << ",\n";
        file << "    \"warmup_frames_skipped\": " << startIdx << ",\n";
        file << "    \"gpu_total_ms\": {\n";
        file << "      \"avg\": " << gpuStats.avg << ",\n";
        file << "      \"min\": " << gpuStats.min << ",\n";
        file << "      \"max\": " << gpuStats.max << ",\n";
        file << "      \"p50\": " << gpuStats.p50 << ",\n";
        file << "      \"p90\": " << gpuStats.p90 << ",\n";
        file << "      \"p95\": " << gpuStats.p95 << ",\n";
        file << "      \"stddev\": " << gpuStats.stddev << "\n";
        file << "    },\n";
        file << "    \"cpu_frame_ms\": {\n";
        file << "      \"avg\": " << cpuStats.avg << ",\n";
        file << "      \"p50\": " << cpuStats.p50 << ",\n";
        file << "      \"p95\": " << cpuStats.p95 << "\n";
        file << "    }\n";
        file << "  }\n";
        file << "}\n";

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BenchmarkLogger JSON Hata]: " << e.what() << "\n";
        return false;
    }
}

} // namespace Astral
