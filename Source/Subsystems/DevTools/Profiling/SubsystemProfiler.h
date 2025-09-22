#pragma once
#include "ProfilingDataCollector.h"
#include <string>

namespace AstralEngine {

/**
 * @brief Subsystem profilling için RAII tabanlı helper sınıfı
 * 
 * Bu sınıf, subsystem'lerin performansını ölçmek için kullanılır.
 * Constructor'da profilling başlatır, destructor'da bitirir.
 * Bu sayede kodun temiz ve güvenli kalması sağlanır.
 * 
 * Kullanım örneği:
 * @code
 * void MySubsystem::OnUpdate(float deltaTime) {
 *     SubsystemProfiler profiler("MySubsystem");
 *     // Subsystem güncelleme kodları
 * }
 * @endcode
 */
class SubsystemProfiler {
public:
    /**
     * @brief Subsystem profilling başlatır
     * @param subsystemName Profilling yapılacak subsystem'in adı
     */
    SubsystemProfiler(const std::string& subsystemName);
    
    /**
     * @brief Subsystem profilling bitirir ve verileri kaydeder
     */
    ~SubsystemProfiler();
    
    // Kopyalamayı ve taşımayı engelle
    SubsystemProfiler(const SubsystemProfiler&) = delete;
    SubsystemProfiler& operator=(const SubsystemProfiler&) = delete;
    SubsystemProfiler(SubsystemProfiler&&) = delete;
    SubsystemProfiler& operator=(SubsystemProfiler&&) = delete;
    
private:
    std::string m_subsystemName;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

} // namespace AstralEngine