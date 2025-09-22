#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <any>
#include <chrono>
#include <mutex>

namespace AstralEngine {

enum class DevToolsEventType {
    MaterialChanged,
    EntitySelected,
    PerformanceThresholdExceeded,
    ToolEnabled,
    ToolDisabled,
    DataUpdated,
    Custom
};

struct DevToolsEvent {
    DevToolsEventType type;
    std::string eventName;
    std::any data;
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    
    DevToolsEvent(DevToolsEventType t, const std::string& name, const std::any& d = {}, const std::string& src = "")
        : type(t), eventName(name), data(d), timestamp(std::chrono::system_clock::now()), source(src) {}
};

class DevToolsEventSystem {
public:
    using EventCallback = std::function<void(const DevToolsEvent&)>;
    
    static DevToolsEventSystem& GetInstance();
    
    // Event yönetimi
    void Subscribe(const std::string& eventType, const EventCallback& callback, bool once = false);
    void Unsubscribe(const std::string& eventType, const EventCallback& callback);
    void Publish(const DevToolsEvent& event);
    
    // Özel event yayınlama metodları
    void PublishMaterialChanged(const std::string& materialPath);
    void PublishEntitySelected(uint32_t entityId);
    void PublishPerformanceThresholdExceeded(const std::string& metric, float value, float threshold);
    void PublishToolEnabled(const std::string& toolName);
    void PublishToolDisabled(const std::string& toolName);
    void PublishDataUpdated(const std::string& dataName, const std::any& data);
    
    // Event geçmişi
    const std::vector<DevToolsEvent>& GetEventHistory(size_t maxCount = 100) const;
    void ClearEventHistory();
    
private:
    DevToolsEventSystem() = default;
    ~DevToolsEventSystem() = default;
    
    // Copy constructor ve assignment operator'ı sil
    DevToolsEventSystem(const DevToolsEventSystem&) = delete;
    DevToolsEventSystem& operator=(const DevToolsEventSystem&) = delete;
    
    struct EventSubscription {
        EventCallback callback;
        bool once;
        std::chrono::system_clock::time_point subscribeTime;
        
        EventSubscription(const EventCallback& cb, bool o)
            : callback(cb), once(o), subscribeTime(std::chrono::system_clock::now()) {}
    };
    
    std::unordered_map<std::string, std::vector<EventSubscription>> m_subscriptions;
    std::vector<DevToolsEvent> m_eventHistory;
    mutable std::mutex m_mutex;
};

} // namespace AstralEngine