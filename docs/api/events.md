# Events Sistemi - API Referansı

## Genel Bakış

Events sistemi, Astral Engine içindeki subsystem'ler ve modüller arasında gevşek bağlı, tip güvenli iletişim sağlar. Merkezi erişim noktası `EventManager`'dır; temel event tipi `Event`'ten türetilir.

Önemli kaynak dosyalar:
- [`Source/Events/Event.h`](Source/Events/Event.h:1)
- [`Source/Events/EventManager.h`](Source/Events/EventManager.h:1)
- [`Source/Events/KeyEvent.h`](Source/Events/KeyEvent.h:1)
- [`Source/Events/MouseEvent.h`](Source/Events/MouseEvent.h:1)

## Temel Kavramlar

- Event: Tüm event'ler `Event` arayüzünden türetilir.
- Publisher: Event üreten taraf; genellikle `PlatformSubsystem` veya `InputManager`.
- Subscriber: Belirli bir event türüne abone olan callback fonksiyonu.

## EventManager - Özet

API başlıca şu işlevleri sağlar:

- Singleton erişimi: [`EventManager::GetInstance()`](Source/Events/EventManager.h:1)
- Abonelik: `Subscribe<T>(callback)` / `Subscribe<TEvent>(handler)`
- Yayınlama: `PublishEvent<T>(args...)` veya `PublishEvent(std::unique_ptr<Event>)`
- İşleme: `ProcessEvents()`
- Abonelik iptali: `Unsubscribe(handlerID)` / `UnsubscribeAll()`
- İstatistik: `GetSubscriberCount()`, `GetPendingEventCount()`

## Nasıl Kullanılır - Örnekler

Basit tip güvenli abonelik:
```cpp
auto id = EventManager::GetInstance().Subscribe<WindowResizeEvent>(
    [](WindowResizeEvent& e) {
        // Handle resize
        return true; // Event tüketildi
    }
);
```

Event yayınlama:
```cpp
EventManager::GetInstance().PublishEvent<WindowResizeEvent>(1920, 1080);
```

Abonelik iptali:
```cpp
EventManager::GetInstance().Unsubscribe(id);
```

ProcessEvents çağrısı:
- `Engine::Update()` içinde per-frame çağrılır: [`Source/Core/Engine.cpp`](Source/Core/Engine.cpp:1)

## Önemli Notlar ve Best-Practices

- Handler fonksiyonları hızlı olmalıdır; uzun süren işler `ThreadPool`'a taşınmalıdır.
- Event'ler tip güvenli olduğundan `static std::type_index` ile tür tanımı kullanılır (`GetStaticType()` pattern).
- Thread-safety: `EventManager` çift mutex kullanır (handlers ve queue).
- Event queue ana thread'de işlenir; publish asenkron olabilir.

## Mevcut Event Türleri (Örnek)

- `WindowResizeEvent` — pencere yeniden boyutlandırma
- `KeyPressedEvent`, `KeyReleasedEvent` — klavye olayları: [`Source/Events/KeyEvent.h`](Source/Events/KeyEvent.h:1)
- `MouseMovedEvent`, `MouseButtonPressedEvent` — fare olayları: [`Source/Events/MouseEvent.h`](Source/Events/MouseEvent.h:1)

## Kaynaklar

- Kaynak implementasyon: [`Source/Events/EventManager.cpp`](Source/Events/EventManager.cpp:1)
- Event tanımları ve yardımcı sınıflar: [`Source/Events/`](Source/Events/:1)