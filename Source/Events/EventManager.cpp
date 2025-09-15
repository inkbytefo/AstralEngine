#include "EventManager.h"
#include "../Core/Logger.h"
#include <algorithm>

namespace AstralEngine {

EventManager& EventManager::GetInstance() {
    static EventManager instance;
    return instance;
}

void EventManager::PublishEvent(std::unique_ptr<Event> event) {
    if (!event) {
        Logger::Warning("EventManager", "Attempted to publish null event");
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventQueue.push_back(std::move(event));
    }
    
    Logger::Trace("EventManager", "Event queued for processing: {}", 
                  m_eventQueue.back()->GetName());
}

void EventManager::ProcessEvents() {
    std::vector<std::unique_ptr<Event>> eventsToProcess;
    
    // Event kuyruğunu atomik olarak boşalt
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        eventsToProcess = std::move(m_eventQueue);
        m_eventQueue.clear();
    }
    
    // Eventleri işle
    for (auto& event : eventsToProcess) {
        if (event && !event->IsHandled()) {
            ProcessEvent(*event);
        }
    }
    
    if (!eventsToProcess.empty()) {
        Logger::Trace("EventManager", "Processed {} events", eventsToProcess.size());
    }
}

void EventManager::ProcessEvent(Event& event) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    auto eventType = event.GetType();
    auto it = m_handlers.find(eventType);
    
    if (it == m_handlers.end()) {
        // Bu event tipi için handler yok
        Logger::Trace("EventManager", "No handlers found for event: {}", event.GetName());
        return;
    }
    
    // Bu event tipi için tüm handler'ları çağır
    bool wasHandled = false;
    for (const auto& handlerInfo : it->second) {
        try {
            if (handlerInfo.handler(event)) {
                wasHandled = true;
                Logger::Trace("EventManager", "Event '{}' handled by handler ID: {}", 
                             event.GetName(), handlerInfo.id);
                
                // Eğer event handled olarak işaretlendiyse, diğer handler'ları çağırmayı durdur
                if (event.IsHandled()) {
                    break;
                }
            }
        } catch (const std::exception& e) {
            Logger::Error("EventManager", "Exception in event handler (ID: {}): {}", 
                         handlerInfo.id, e.what());
        }
    }
    
    if (wasHandled) {
        event.SetHandled(true);
    }
}

void EventManager::Unsubscribe(EventHandlerID handlerID) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    for (auto& [eventType, handlers] : m_handlers) {
        auto it = std::remove_if(handlers.begin(), handlers.end(),
            [handlerID](const HandlerInfo& info) {
                return info.id == handlerID;
            });
        
        if (it != handlers.end()) {
            handlers.erase(it, handlers.end());
            Logger::Debug("EventManager", "Unsubscribed handler ID: {}", handlerID);
            return;
        }
    }
    
    Logger::Warning("EventManager", "Handler ID not found for unsubscribe: {}", handlerID);
}

void EventManager::UnsubscribeAll() {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    size_t totalHandlers = 0;
    for (const auto& [eventType, handlers] : m_handlers) {
        totalHandlers += handlers.size();
    }
    
    m_handlers.clear();
    
    Logger::Info("EventManager", "Unsubscribed all handlers. Total removed: {}", totalHandlers);
}

size_t EventManager::GetSubscriberCount() const {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    size_t totalCount = 0;
    for (const auto& [eventType, handlers] : m_handlers) {
        totalCount += handlers.size();
    }
    
    return totalCount;
}

size_t EventManager::GetPendingEventCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    return m_eventQueue.size();
}

} // namespace AstralEngine
