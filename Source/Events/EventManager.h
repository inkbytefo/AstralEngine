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

        static void Init() {
            s_Instance = new EventManager();
        }

        static void Shutdown() {
            delete s_Instance;
            s_Instance = nullptr;
        }

        static EventManager* GetInstance() { return s_Instance; }

        void AddListener(EventType type, const EventCallback& callback) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Listeners[type].push_back(callback);
        }

        void TriggerEvent(Event& event) {
            EventDispatcher dispatcher(event);
            dispatcher.Dispatch<Event>([this](Event& e) {
                if (m_Listeners.find(e.GetEventType()) == m_Listeners.end()) {
                    return false;
                }

                for (auto const& callback : m_Listeners.at(e.GetEventType())) {
                    callback(e);
                }
                return true;
            });
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

        static EventManager* s_Instance;
        std::unordered_map<EventType, std::vector<EventCallback>> m_Listeners;
        std::queue<std::unique_ptr<Event>> m_EventQueue;
        std::mutex m_Mutex;
    };
}