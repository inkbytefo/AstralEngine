#include "GPUProfiler.h"
#include "../../../Core/Logger.h"
#include <algorithm>

namespace AstralEngine {

GPUProfiler& GPUProfiler::GetInstance() {
    static GPUProfiler instance;
    return instance;
}

void GPUProfiler::Initialize(VkDevice device, uint32_t queueFamilyIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        Logger::Warning("GPUProfiler", "GPUProfiler zaten başlatılmış");
        return;
    }
    
    m_device = device;
    
    // Queue'yu al
    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    m_queue = queue;
    
    // Query pool oluştur
    CreateQueryPool();
    
    m_initialized = true;
    Logger::Info("GPUProfiler", "GPUProfiler başarıyla başlatıldı");
}

void GPUProfiler::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    // Query pool'u temizle
    DestroyQueryPool();
    
    // Diğer kaynakları temizle
    m_scopes.clear();
    m_availableQueryIndices.clear();
    m_device = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
    m_initialized = false;
    
    Logger::Info("GPUProfiler", "GPUProfiler kapatıldı");
}

void GPUProfiler::BeginFrame(VkCommandBuffer commandBuffer) {
    if (!m_initialized) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Önceki frame'in sonuçlarını işle
    ProcessQueryResults();
    
    // Mevcut frame için scope'ları temizle
    m_scopes.clear();
    
    // Kullanılabilir query indekslerini sıfırla
    m_availableQueryIndices.clear();
    for (uint32_t i = 0; i < 100; ++i) { // Maksimum 50 scope (her scope için 2 query)
        m_availableQueryIndices.push_back(i);
    }
    
    m_currentFrameIndex++;
}

void GPUProfiler::EndFrame(VkCommandBuffer commandBuffer) {
    if (!m_initialized) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Frame sonu işlemleri
    // Query sonuçları bir sonraki frame'de işlenecek
}

void GPUProfiler::BeginScope(VkCommandBuffer commandBuffer, const std::string& name) {
    if (!m_initialized || m_queryPool == VK_NULL_HANDLE) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Yeni query indeksi al
    uint32_t queryIndex = GetNextQueryIndex();
    if (queryIndex == UINT32_MAX) {
        Logger::Warning("GPUProfiler", "Maksimum query sayısına ulaşıldı, scope atlanıyor: {}", name);
        return;
    }
    
    // Başlangıç timestamp'ini yaz
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                       m_queryPool, queryIndex * 2);
    
    // Scope bilgisini kaydet
    GPUScope scope;
    scope.name = name;
    scope.queryIndex = queryIndex;
    scope.duration = 0.0f;
    m_scopes.push_back(scope);
}

void GPUProfiler::EndScope(VkCommandBuffer commandBuffer) {
    if (!m_initialized || m_queryPool == VK_NULL_HANDLE || m_scopes.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Son scope'u al
    GPUScope& scope = m_scopes.back();
    
    // Bitiş timestamp'ini yaz
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                       m_queryPool, scope.queryIndex * 2 + 1);
    
    // Query indeksini geri döndürme - sonuçlar işlendikten sonra yapılacak
}

float GPUProfiler::GetGPUTime(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Scope'ları ara ve süreyi döndür
    for (const auto& scope : m_scopes) {
        if (scope.name == name) {
            return scope.duration;
        }
    }
    
    return 0.0f;
}

bool GPUProfiler::IsInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

void GPUProfiler::CreateQueryPool() {
    if (m_device == VK_NULL_HANDLE) return;
    
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = 200; // Maksimum 100 scope (her scope için 2 timestamp)
    
    VkResult result = vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &m_queryPool);
    if (result == VK_SUCCESS) {
        Logger::Info("GPUProfiler", "Timestamp query pool oluşturuldu ({} query)", queryPoolInfo.queryCount);
    } else {
        Logger::Error("GPUProfiler", "Timestamp query pool oluşturulamadı: {}", result);
        m_queryPool = VK_NULL_HANDLE;
    }
}

void GPUProfiler::DestroyQueryPool() {
    if (m_device != VK_NULL_HANDLE && m_queryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
        m_queryPool = VK_NULL_HANDLE;
        Logger::Info("GPUProfiler", "Timestamp query pool yok edildi");
    }
}

uint32_t GPUProfiler::GetNextQueryIndex() {
    if (m_availableQueryIndices.empty()) {
        return UINT32_MAX; // Kullanılabilir query kalmadı
    }
    
    uint32_t index = m_availableQueryIndices.back();
    m_availableQueryIndices.pop_back();
    return index;
}

void GPUProfiler::ReturnQueryIndex(uint32_t index) {
    m_availableQueryIndices.push_back(index);
}

void GPUProfiler::ProcessQueryResults() {
    if (m_device == VK_NULL_HANDLE || m_queryPool == VK_NULL_HANDLE || m_scopes.empty()) {
        return;
    }
    
    // Timestamp sonuçlarını al
    std::vector<uint64_t> timestamps(m_scopes.size() * 2);
    VkResult result = vkGetQueryPoolResults(
        m_device, 
        m_queryPool, 
        0, 
        timestamps.size(), 
        timestamps.size() * sizeof(uint64_t), 
        timestamps.data(), 
        sizeof(uint64_t), 
        VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT
    );
    
    if (result != VK_SUCCESS) {
        Logger::Warning("GPUProfiler", "Query sonuçları alınamadı: {}", result);
        return;
    }
    
    // GPU özelliklerini al (timestamp period)
    VkPhysicalDeviceProperties deviceProperties;
    // Not: Burada physical device'e erişim gerekiyor, bu bilgi Initialize sırasında saklanabilir
    // Şimdilik varsayılan bir değer kullanıyoruz
    float timestampPeriod = 1.0f; // nanoseconds
    
    // Her scope için süreyi hesapla
    for (size_t i = 0; i < m_scopes.size(); ++i) {
        uint32_t queryIndex = m_scopes[i].queryIndex;
        uint64_t startTimestamp = timestamps[queryIndex * 2];
        uint64_t endTimestamp = timestamps[queryIndex * 2 + 1];
        
        if (startTimestamp > 0 && endTimestamp > startTimestamp) {
            uint64_t duration = endTimestamp - startTimestamp;
            m_scopes[i].duration = duration * timestampPeriod / 1000000.0f; // nanoseconds to milliseconds
        } else {
            m_scopes[i].duration = 0.0f;
        }
        
        // Query indeksini geri döndür
        ReturnQueryIndex(queryIndex);
        
        // ProfilingDataCollector'a GPU süresini bildir
        ProfilingDataCollector::GetInstance().BeginGPUProfiling(VK_NULL_HANDLE, m_scopes[i].name);
        ProfilingDataCollector::GetInstance().EndGPUProfiling(VK_NULL_HANDLE);
    }
    
    // Debug bilgisi
    if (!m_scopes.empty()) {
        Logger::Debug("GPUProfiler", "{} GPU scope işlendi", m_scopes.size());
    }
}

} // namespace AstralEngine