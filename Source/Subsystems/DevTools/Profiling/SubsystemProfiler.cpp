#include "SubsystemProfiler.h"

namespace AstralEngine {

SubsystemProfiler::SubsystemProfiler(const std::string& subsystemName) 
    : m_subsystemName(subsystemName) {
    m_startTime = std::chrono::high_resolution_clock::now();
    
    // ProfilingDataCollector'a subsystem başlangıcını bildir
    ProfilingDataCollector::GetInstance().BeginSubsystemUpdate(m_subsystemName);
}

SubsystemProfiler::~SubsystemProfiler() {
    // Bitiş zamanını hesapla
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_startTime);
    float timeMs = duration.count() / 1000.0f;
    
    // ProfilingDataCollector'a subsystem bitişini bildir
    ProfilingDataCollector::GetInstance().EndSubsystemUpdate();
}

} // namespace AstralEngine