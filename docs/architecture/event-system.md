# Astral Engine - Event Sistemi

Event sistemi, Astral Engine'de subsystem'ler arasında gevşek bağlılık sağlayan ve event-driven mimariyi destekleyen temel iletişim mekanizmasıdır. Bu dokümantasyon, event sisteminin nasıl çalıştığını, nasıl kullanılacağını ve thread safety özelliklerini açıklamaktadır.

## 🎯 Event Sistemi Amacı

Event sisteminin temel hedefleri:

1. **Gevşek Bağlılık**: Subsystem'ler birbirine doğrudan bağımlı olmaz
2. **Asenkron İletişim**: Event'lerin zamanında işlenmesi
3. **Genişletilebilirlik**: Yeni event tiplerinin kolayca eklenmesi
4. **Thread Safety**: Çoklu iş parçacığından güvenli kullanım

## 🏗️ Event Sistemi Mimarisi

### EventManager Sınıfı

[`EventManager`](Source/Events/EventManager.h:19), motorun merkezi event yöneticisidir. Singleton pattern kullanılır ve tüm event işlemlerinden sorumludur:

```cpp
class EventManager {
public:
    // Singleton pattern
    static EventManager& GetInstance();

    // Event dinleyici yönetimi
    template<typename T>
    EventHandlerID Subscribe(EventHandler handler);
    
    void Unsubscribe(EventHandlerID handlerID);
    void UnsubscribeAll();

    // Event gönderme
    void PublishEvent(std::unique_ptr<Event> event);
    
    template<typename T, typename... Args>
    void PublishEvent(Args&&... args);

    // Sıralı event işleme
    void ProcessEvents();

private:
    std::unordered_map<std::type_index, std::vector<HandlerInfo>> m_handlers;
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    mutable std::mutex m_handlersMutex;
    mutable std::mutex m_queueMutex;
};
```

### Event Temel Sınıfı

Tüm event'ler [`Event`](Source/Events/Event.h) sınıfından türetilir:

```cpp
class Event {
public:
    virtual ~Event() = default;
    
    // Event tipini döndürür
    virtual std::type_index GetType() const = 0;
    
    // Statik tip bilgisi
    template<typename T>
    static std::type_index GetStaticType();
};
```

## 📋 Event Tipleri

### Mevcut Event Tipleri

Şu anda motor şu event tiplerini desteklemektedir:

#### Window Events
- **WindowResizeEvent**: Pencere yeniden boyutlandırma
- **WindowCloseEvent**: Pencere kapatma
- **WindowFocusEvent**: Pencere odak değişikliği

#### Input Events
- **KeyPressEvent**: Tuşa basma
- **KeyReleaseEvent**: Tuş bırakma
- **MousePressEvent**: Fare tuşu basma
- **MouseReleaseEvent**: Fare tuşu bırakma
- **MouseMoveEvent**: Fare hareketi
- **MouseScrollEvent**: Fare tekerleği

#### Rendering Events
- **FrameStartEvent**: Frame başlangıcı
- **FrameEndEvent**: Frame sonu
- **RenderCommandEvent**: Render komutu

#### Asset Events
- **AssetLoadedEvent**: Asset yüklendi
- **AssetFailedEvent**: Asset yüklenemedi

### Event Oluşturma

Yeni bir event tipi oluşturmak:

```cpp
class PlayerDeathEvent : public Event {
public:
    PlayerDeathEvent(uint32_t playerId, glm::vec3 position)
        : m_playerId(playerId), m_position(position) {}

    std::type_index GetType() const override {
        return typeid(PlayerDeathEvent);
    }

    static std::type_index GetStaticType() {
        return typeid(PlayerDeathEvent);
    }

    uint32_t GetPlayerId() const { return m_playerId; }
    const glm::vec3& GetPosition() const { return m_position; }

private:
    uint32_t m_playerId;
    glm::vec3 m_position;
};
```

## 🔧 Event Kullanımı

### Event Dinleyici Kaydetme

Event dinleyici kaydetmek için `Subscribe` metodu kullanılır:

```cpp
EventManager& eventMgr = EventManager::GetInstance();

// Lambda fonksiyonu ile dinleyici
auto handlerID = eventMgr.Subscribe<WindowResizeEvent>(
    [](Event& event) {
        auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
        Logger::Info("Window", "Window resized to {}x{}", 
                    resizeEvent.GetWidth(), resizeEvent.GetHeight());
        return true; // Event işlendi
    });

// Üye fonksiyon ile dinleyici
class InputManager {
public:
    bool HandleKeyPress(Event& event) {
        auto& keyEvent = static_cast<KeyPressEvent&>(event);
        // Tuş basma işlemleri
        return true;
    }
};

InputManager inputManager;
auto handlerID = eventMgr.Subscribe<KeyPressEvent>(
    [this](Event& event) { return inputManager.HandleKeyPress(event); });
```

### Event Gönderme

Event göndermek için `PublishEvent` metodu kullanılır:

```cpp
EventManager& eventMgr = EventManager::GetInstance();

// Template ile event gönderme
eventMgr.PublishEvent<WindowResizeEvent>(1920, 1080);

// Dinamik event gönderme
auto event = std::make_unique<PlayerDeathEvent>(42, glm::vec3(0, 0, 0));
eventMgr.PublishEvent(std::move(event));
```

### Event İşleme

Event'lerin işlenmesi için `ProcessEvents` metodu çağrılmalıdır:

```cpp
void Engine::Update() {
    // ... diğer güncelleme işlemleri
    
    // Event'leri işle
    EventManager::GetInstance().ProcessEvents();
    
    // ... diğer güncelleme işlemleri
}
```

## 🛡️ Thread Safety

### Çift Kilit Mimarisi

EventManager, thread safety için çift kilit (dual-mutex) tasarımı kullanır:

```cpp
class EventManager {
    mutable std::mutex m_handlersMutex;  // Handler yönetimi için
    mutable std::mutex m_queueMutex;     // Event kuyruğu için
};
```

**Kilit Mimarisi:**
- **`m_handlersMutex`**: Dinleyici kayıt ve silme işlemleri için
- **`m_queueMutex`**: Event gönderme ve işleme için

### Thread-Safe Kullanım

```cpp
// Thread-safe event gönderme
void EventManager::PublishEvent(std::unique_ptr<Event> event) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push_back(std::move(event));
}

// Thread-safe dinleyici kaydetme
template<typename T>
EventManager::EventHandlerID EventManager::Subscribe(EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    HandlerInfo info(m_nextHandlerID++, T::GetStaticType(), handler);
    m_handlers[T::GetStaticType()].push_back(info);
    
    return info.id;
}
```

### Event İşleme Kuralları

1. **Event'ler ana thread'de işlenmelidir**: `ProcessEvents()` sadece ana thread'de çağrılmalıdır
2. **Event gönderme thread-safe**: Event'ler her thread'den güvenle gönderilebilir
3. **Dinleyici yönetimi thread-safe**: Dinleyici kayıt/silme işlemleri thread-safe'dir

## 🔄 Event Yaşam Döngüsü

### Event Akışı

```
Event Gönderme → Event Kuyruğu → Event İşleme → Dinleyici Çağrısı
```

### Detaylı Akış

1. **Event Gönderme**
   ```cpp
   eventMgr.PublishEvent<WindowResizeEvent>(1920, 1080);
   ```

2. **Event Kuyruğa Ekleme**
   ```cpp
   void EventManager::PublishEvent(std::unique_ptr<Event> event) {
       std::lock_guard<std::mutex> lock(m_queueMutex);
       m_eventQueue.push_back(std::move(event));
   }
   ```

3. **Event İşleme**
   ```cpp
   void EventManager::ProcessEvents() {
       std::vector<std::unique_ptr<Event>> eventsToProcess;
       
       {
           std::lock_guard<std::mutex> lock(m_queueMutex);
           eventsToProcess = std::move(m_eventQueue);
       }
       
       for (auto& event : eventsToProcess) {
           ProcessEvent(*event);
       }
   }
   ```

4. **Dinleyici Çağrısı**
   ```cpp
   void EventManager::ProcessEvent(Event& event) {
       auto handlersIt = m_handlers.find(event.GetType());
       if (handlersIt != m_handlers.end()) {
           for (auto& handler : handlersIt->second) {
               if (handler.handler(event)) {
                   break; // Event işlendi, diğer dinleyicilere gitme
               }
           }
       }
   }
   ```

## 🎯 Event Filtreleme ve Önceliklendirme

### Event Filtreleme

Event'lerin belirli dinleyicilere gönderilmesini sağlamak:

```cpp
// Event tipi ve özelliklerine göre filtreleme
eventMgr.Subscribe<CollisionEvent>(
    [](Event& event) {
        auto& collisionEvent = static_cast<CollisionEvent&>(event);
        
        // Sadece belirli entity'ler için
        if (collisionEntityA == m_playerEntity || 
            collisionEntityB == m_playerEntity) {
            // Player çarpışma işlemleri
            return true;
        }
        
        return false; // Diğer dinleyicilere git
    });
```

### Event Önceliklendirme

Event'lerin işlenme sırasını kontrol etmek:

```cpp
struct HandlerInfo {
    EventHandlerID id;
    std::type_index eventType;
    EventHandler handler;
    int priority; // Öncelik (düşük değer = yüksek öncelik)
};

// Öncelikli dinleyici kaydetme
void EventManager::SubscribeWithPriority(EventHandler handler, int priority) {
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    
    HandlerInfo info(m_nextHandlerID++, eventType, handler);
    info.priority = priority;
    
    // Önceliğe göre ekleme
    auto& handlers = m_handlers[eventType];
    handlers.insert(std::lower_bound(handlers.begin(), handlers.end(), info,
        [](const HandlerInfo& a, const HandlerInfo& b) {
            return a.priority < b.priority;
        }), info);
}
```

## 📊 Event İstatistikleri

### Event İzleme

Event sisteminin performansını izlemek:

```cpp
class EventManager {
public:
    // İstatistikler
    size_t GetSubscriberCount() const;
    size_t GetPendingEventCount() const;
    
    // Event işleme süresi
    void EnableProfiling(bool enable) { m_enableProfiling = enable; }
    
private:
    mutable std::mutex m_statsMutex;
    size_t m_totalEventsProcessed = 0;
    size_t m_totalSubscribers = 0;
    bool m_enableProfiling = false;
};
```

### Kullanım Örneği

```cpp
EventManager& eventMgr = EventManager::GetInstance();

// Event istatistiklerini al
Logger::Info("EventManager", "Active subscribers: {}", 
            eventMgr.GetSubscriberCount());
Logger::Info("EventManager", "Pending events: {}", 
            eventMgr.GetPendingEventCount());
```

## 🔧 Event Sistemi Genişletme

### Özel Event Handler'lar

Fonksiyon objeleri kullanarak özel event handler'lar oluşturma:

```cpp
// Event handler sınıfı
class PlayerDeathHandler {
public:
    PlayerDeathHandler(GameState& gameState) : m_gameState(gameState) {}
    
    bool HandleEvent(Event& event) {
        auto& deathEvent = static_cast<PlayerDeathEvent&>(event);
        
        // Oyun durumunu güncelle
        m_gameState.OnPlayerDeath(deathEvent.GetPlayerId());
        
        // Event işlendi
        return true;
    }
    
private:
    GameState& m_gameState;
};

// Kullanım
GameState gameState;
PlayerDeathHandler handler(gameState);
auto handlerID = eventMgr.Subscribe<PlayerDeathEvent>(
    [handler](Event& event) { return handler.HandleEvent(event); });
```

### Event Chaining

Event'lerin birbirini tetiklemesi:

```cpp
// Event zinciri oluşturma
eventMgr.Subscribe<PlayerDeathEvent>(
    [](Event& event) {
        auto& deathEvent = static_cast<PlayerDeathEvent&>(event);
        
        // Yeni event gönder
        EventManager::GetInstance().PublishEvent<ScoreEvent>(
            deathEvent.GetPlayerId(), -100);
        
        return true;
    });
```

## 🚀 Performans Optimizasyonları

### Event Pooling

Event nesnelerinin tekrar kullanımı:

```cpp
class EventManager {
private:
    std::unordered_map<std::type_index, std::vector<std::unique_ptr<Event>>> m_eventPool;
    
    template<typename T>
    std::unique_ptr<Event> GetEventFromPool() {
        auto& pool = m_eventPool[typeid(T)];
        if (pool.empty()) {
            return std::make_unique<T>();
        }
        
        auto event = std::move(pool.back());
        pool.pop_back();
        return event;
    }
    
    template<typename T>
    void ReturnEventToPool(std::unique_ptr<Event> event) {
        auto& pool = m_eventPool[typeid(T)];
        pool.push_back(std::move(event));
    }
};
```

### Batch Event Processing

Event'lerin toplu olarak işlenmesi:

```cpp
void EventManager::ProcessEvents() {
    std::vector<std::unique_ptr<Event>> eventsToProcess;
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_eventQueue.empty()) return;
        
        // Toplu taşıma
        eventsToProcess.reserve(m_eventQueue.size());
        for (auto& event : m_eventQueue) {
            eventsToProcess.push_back(std::move(event));
        }
        m_eventQueue.clear();
    }
    
    // Toplu işleme
    for (auto& event : eventsToProcess) {
        ProcessEvent(*event);
    }
}
```

## 🔮 Gelecek Geliştirmeler

### 1. Event Queuing Sistemi

Farklı öncelikteki event kuyrukları:

```cpp
class EventManager {
public:
    enum class EventQueue {
        HighPriority,
        Normal,
        LowPriority
    };
    
    void PublishEvent(std::unique_ptr<Event> event, EventQueue queue = EventQueue::Normal);
    
private:
    std::array<std::vector<std::unique_ptr<Event>>, 3> m_eventQueues;
};
```

### 2. Event Filtering System

Gelişmiş event filtreleme:

```cpp
class EventFilter {
public:
    virtual bool ShouldProcess(Event& event) = 0;
};

// Event tipi ve özelliklerine göre filtreleme
class TypeAndPropertyFilter : public EventFilter {
public:
    bool ShouldProcess(Event& event) override {
        return event.GetType() == typeid(TargetEvent) && 
               m_propertyChecker(event);
    }
    
private:
    std::function<bool(Event&)> m_propertyChecker;
};
```

### 3. Event Replay System

Event'lerin kaydedilip tekrar oynatılması:

```cpp
class EventRecorder {
public:
    void RecordEvent(std::unique_ptr<Event> event);
    void ReplayEvents(float startTime, float endTime);
    void ClearRecording();
    
private:
    std::vector<std::pair<float, std::unique_ptr<Event>>> m_recordedEvents;
};
```

---

Event sistemi, Astral Engine'de subsystem'ler arasında esnek ve güvenli iletişim sağlayan temel bir bileşendir. Bu mimari, motorun genişletilebilirliğini ve bakım kolaylığını önemli ölçüde artırmaktadır.