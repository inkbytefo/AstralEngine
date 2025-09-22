#include "DevToolsEvent.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

DevToolsEventSystem& DevToolsEventSystem::GetInstance() {
    static DevToolsEventSystem instance;
    return instance;
}

void DevToolsEventSystem::Subscribe(const std::string& eventType, const EventCallback& callback, bool once) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_subscriptions[eventType].emplace_back(callback, once);
    Logger::Info("DevToolsEvent", "Event aboneliği eklendi: {}", eventType);
}

void DevToolsEventSystem::Unsubscribe(const std::string& eventType, const EventCallback& callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_subscriptions.find(eventType);
    if (it != m_subscriptions.end()) {
        auto& subscriptions = it->second;
        subscriptions.erase(
            std::remove_if(subscriptions.begin(), subscriptions.end(),
                [&callback](const EventSubscription& subscription) {
                    // Callback fonksiyonlarını karşılaştırma
                    return &subscription.callback == &callback;
                }),
            subscriptions.end()
        );
        
        if (subscriptions.empty()) {
            m_subscriptions.erase(it);
        }
        
        Logger::Info("DevToolsEvent", "Event aboneliği kaldırıldı: {}", eventType);
    }
}

void DevToolsEventSystem::Publish(const DevToolsEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Event geçmişine ekle
    m_eventHistory.push_back(event);
    
    // Geçmiş boyutunu sınırla
    const size_t MAX_HISTORY_SIZE = 1000;
    if (m_eventHistory.size() > MAX_HISTORY_SIZE) {
        m_eventHistory.erase(m_eventHistory.begin(), m_eventHistory.begin() + (m_eventHistory.size() - MAX_HISTORY_SIZE));
    }
    
    // Abonelere event'i gönder
    auto it = m_subscriptions.find(event.eventName);
    if (it != m_subscriptions.end()) {
        auto& subscriptions = it->second;
        
        // Once aboneliklerini takip etmek için geçici liste
        std::vector<size_t> onceSubscriptionsToRemove;
        
        for (size_t i = 0; i < subscriptions.size(); ++i) {
            try {
                const auto& subscription = subscriptions[i];
                subscription.callback(event);
                
                if (subscription.once) {
                    onceSubscriptionsToRemove.push_back(i);
                }
            }
            catch (const std::exception& e) {
                Logger::Error("DevToolsEvent", "Event callback çalıştırılırken hata: {}", e.what());
            }
        }
        
        // Once aboneliklerini kaldır (tersten silme)
        for (auto it = onceSubscriptionsToRemove.rbegin(); it != onceSubscriptionsToRemove.rend(); ++it) {
            subscriptions.erase(subscriptions.begin() + *it);
        }
        
        // Boş olan event tipini kaldır
        if (subscriptions.empty()) {
            m_subscriptions.erase(it);
        }
    }
    
    Logger::Debug("DevToolsEvent", "Event yayınlandı: {}", event.eventName);
}

void DevToolsEventSystem::PublishMaterialChanged(const std::string& materialPath) {
    DevToolsEvent event(DevToolsEventType::MaterialChanged, "MaterialChanged", materialPath, "MaterialSystem");
    Publish(event);
}

void DevToolsEventSystem::PublishEntitySelected(uint32_t entityId) {
    DevToolsEvent event(DevToolsEventType::EntitySelected, "EntitySelected", entityId, "EntitySystem");
    Publish(event);
}

void DevToolsEventSystem::PublishPerformanceThresholdExceeded(const std::string& metric, float value, float threshold) {
    struct PerformanceThresholdData {
        std::string metric;
        float value;
        float threshold;
    };
    
    PerformanceThresholdData data{metric, value, threshold};
    DevToolsEvent event(DevToolsEventType::PerformanceThresholdExceeded, "PerformanceThresholdExceeded", 
                       data, "PerformanceMonitor");
    Publish(event);
}

void DevToolsEventSystem::PublishToolEnabled(const std::string& toolName) {
    DevToolsEvent event(DevToolsEventType::ToolEnabled, "ToolEnabled", toolName, "DevToolsSubsystem");
    Publish(event);
}

void DevToolsEventSystem::PublishToolDisabled(const std::string& toolName) {
    DevToolsEvent event(DevToolsEventType::ToolDisabled, "ToolDisabled", toolName, "DevToolsSubsystem");
    Publish(event);
}

void DevToolsEventSystem::PublishDataUpdated(const std::string& dataName, const std::any& data) {
    struct DataUpdatedInfo {
        std::string name;
        std::any data;
    };
    
    DataUpdatedInfo info{dataName, data};
    DevToolsEvent event(DevToolsEventType::DataUpdated, "DataUpdated", info, "DataBindingSystem");
    Publish(event);
}

const std::vector<DevToolsEvent>& DevToolsEventSystem::GetEventHistory(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (maxCount >= m_eventHistory.size()) {
        return m_eventHistory;
    }
    
    // Son maxCount kadar event'i döndür
    static std::vector<DevToolsEvent> recentEvents;
    recentEvents.clear();
    recentEvents.insert(recentEvents.end(), 
                       m_eventHistory.end() - maxCount, 
                       m_eventHistory.end());
    return recentEvents;
}

void DevToolsEventSystem::ClearEventHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHistory.clear();
    Logger::Info("DevToolsEvent", "Event geçmişi temizlendi");
}

} // namespace AstralEngine