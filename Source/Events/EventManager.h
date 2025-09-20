#pragma once

#include "Event.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <mutex>

namespace AstralEngine {

/**
 * @brief Motorun merkezi olay yöneticisi.
 * 
 * Observer pattern kullanarak event-driven iletişimi sağlar.
 * Thread-safe tasarım ile çoklu iş parçacığından güvenli kullanım.
 */
class EventManager {
public:
    // Event handler fonksiyon tipi
    using EventHandler = std::function<bool(Event&)>;
    using EventHandlerID = size_t;

    // Singleton pattern
    static EventManager& GetInstance();

    // Event dinleyici yönetimi
    template<typename T>
    EventHandlerID Subscribe(EventHandler handler);
    
    // Tip-güvenli event dinleyici yönetimi (static_cast gerektirmez)
    template<typename TEvent, typename TFunc>
    EventHandlerID Subscribe(TFunc&& func);
    
    void Unsubscribe(EventHandlerID handlerID);
    void UnsubscribeAll();

    // Event gönderme
    void PublishEvent(std::unique_ptr<Event> event);
    
    template<typename T, typename... Args>
    void PublishEvent(Args&&... args);

    // Sıralı event işleme (ana thread'de çağrılmalı)
    void ProcessEvents();

    // İstatistikler
    size_t GetSubscriberCount() const;
    size_t GetPendingEventCount() const;

private:
    EventManager() = default;
    ~EventManager() = default;
    
    // Non-copyable
    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    struct HandlerInfo {
        EventHandlerID id;
        std::type_index eventType;
        EventHandler handler;
        
        HandlerInfo() = default;
        HandlerInfo(EventHandlerID i, std::type_index et, EventHandler h)
            : id(i), eventType(et), handler(h) {}
    };

    // Event handlers by type
    std::unordered_map<std::type_index, std::vector<HandlerInfo>> m_handlers;
    
    // Event queue for thread-safe publishing
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    
    // Thread safety
    mutable std::mutex m_handlersMutex;
    mutable std::mutex m_queueMutex;
    
    // Handler ID generation
    EventHandlerID m_nextHandlerID = 1;
    
    // Internal helper functions
    void ProcessEvent(Event& event);
};

// Template implementations
template<typename T>
EventManager::EventHandlerID EventManager::Subscribe(EventHandler handler) {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    HandlerInfo info(m_nextHandlerID++, T::GetStaticType(), handler);
    
    m_handlers[T::GetStaticType()].push_back(info);
    
    return info.id;
}

template<typename TEvent, typename TFunc>
EventManager::EventHandlerID EventManager::Subscribe(TFunc&& func) {
    static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must be derived from Event");
    
    // Tip-güvenli handler'ı generic EventHandler içine wrap et
    EventHandler wrappedHandler = [func = std::forward<TFunc>(func)](Event& event) -> bool {
        // Doğru event tipine güvenli bir şekilde dönüştür
        TEvent& typedEvent = static_cast<TEvent&>(event);
        return func(typedEvent);
    };
    
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    HandlerInfo info(m_nextHandlerID++, TEvent::GetStaticType(), wrappedHandler);
    
    m_handlers[TEvent::GetStaticType()].push_back(info);
    
    return info.id;
}

template<typename T, typename... Args>
void EventManager::PublishEvent(Args&&... args) {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    auto event = std::make_unique<T>(std::forward<Args>(args)...);
    PublishEvent(std::move(event));
}

} // namespace AstralEngine
