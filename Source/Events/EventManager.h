#pragma once

#include "Event.h"
#include "Core/Logger.h"

#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>

namespace AstralEngine {

    class EventManager {
    public:
        using EventCallback = std::function<void(Event&)>;
        using EventHandlerID = size_t;

        EventManager(const EventManager&) = delete;
        EventManager& operator=(const EventManager&) = delete;
        EventManager(EventManager&&) = delete;
        EventManager& operator=(EventManager&&) = delete;

        static EventManager& GetInstance() {
            static EventManager instance;
            return instance;
        }

        template<typename T>
        void AddListener(const EventCallback& callback) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Listeners[T::GetStaticType()].push_back(callback);
        }

        void TriggerEvent(Event& event) {
            std::type_index type = event.GetType();
            auto it = m_Listeners.find(type);
            if (it == m_Listeners.end()) {
                return;
            }

            for (auto const& callback : it->second) {
                callback(event);
                if (event.Handled) break;
            }
        }

        void QueueEvent(std::unique_ptr<Event> event) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_EventQueue.push(std::move(event));
        }

        void DispatchQueuedEvents() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            while (!m_EventQueue.empty()) {
                std::unique_ptr<Event> event = std::move(m_EventQueue.front());
                m_EventQueue.pop();
                TriggerEvent(*event);
            }
        }

    private:
        EventManager() = default;

        std::unordered_map<std::type_index, std::vector<EventCallback>> m_Listeners;
        std::queue<std::unique_ptr<Event>> m_EventQueue;
        std::mutex m_Mutex;
    };
}
