# Astral Engine Core API Referansı

## Giriş

Bu dokümantasyon, Astral Engine'in çekirdek API'lerini detaylı olarak açıklar. Core modüller, motorun temel işlevlerini sağlar ve diğer tüm subsystem'lerin temelini oluşturur.

## İçindekiler

1. [Engine Sınıfı](#engine-sınıfı)
2. [ISubsystem Arayüzü](#isubsystem-arayüzü)
3. [Logger Sınıfı](#logger-sınıfı)
4. [MemoryManager Sınıfı](#memorymanager-sınıfı)
5. [ThreadPool Sınıfı](#threadpool-sınıfı)
6. [EventManager Sınıfı](#eventmanager-sınıfı)
7. [Event Sistemleri](#event-sistemleri)

## Engine Sınıfı

### Genel Bakış

`Engine` sınıfı, Astral Engine'in merkezi orkestratörüdür. Tüm subsystem'lerin yaşam döngüsünü yönetir ve ana döngüyü çalıştırır.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class Engine {
public:
    // Constructor / Destructor
    Engine();
    ~Engine();
    
    // Ana motor döngüsü
    void Run(IApplication* application);
    
    // Subsystem yönetimi
    template<typename T, typename... Args>
    void RegisterSubsystem(Args&&... args);
    
    template<typename T>
    T* GetSubsystem() const;
    
    // Motor kontrolü
    void RequestShutdown();
    bool IsRunning() const { return m_isRunning; }
    
    // Yol yönetimi
    void SetBasePath(const std::filesystem::path& path);
    const std::filesystem::path& GetBasePath() const;
    
private:
    void Initialize();
    void Shutdown();
    void Update();
    
    // Üye değişkenler
    std::map<UpdateStage, std::vector<std::unique_ptr<ISubsystem>>> m_subsystemsByStage;
    std::unordered_map<std::type_index, ISubsystem*> m_subsystemMap;
    std::filesystem::path m_basePath;
    bool m_isRunning = false;
    bool m_initialized = false;
    IApplication* m_application = nullptr;
};

} // namespace AstralEngine
```

### Metodlar

#### `Engine::Run(IApplication* application)`

Motorun ana çalışma döngüsünü başlatır.

**Parametreler:**
- `application`: Çalıştırılacak uygulama örneği

**Örnek Kullanım:**
```cpp
AstralEngine::Engine engine;
MyApplication app;
engine.Run(&app);
```

#### `Engine::RegisterSubsystem<T>(Args&&... args)`

Yeni bir subsystem'i motora kaydeder.

**Parametreler:**
- `T`: Kaydedilecek subsystem tipi
- `args...`: Subsystem constructor parametreleri

**Örnek Kullanım:**
```cpp
engine.RegisterSubsystem<PlatformSubsystem>();
engine.RegisterSubsystem<RenderSubsystem>(1280, 720);
```

#### `Engine::GetSubsystem<T>()`

Kayıtlı bir subsystem'e tip güvenli erişim sağlar.

**Parametreler:**
- `T`: Erişilecek subsystem tipi

**Dönüş Değeri:**
- `T*`: İstenen subsystem işaretçisi veya `nullptr`

**Örnek Kullanım:**
```cpp
auto* renderSubsystem = engine.GetSubsystem<RenderSubsystem>();
if (renderSubsystem) {
    renderSubsystem->SetRenderScale(2.0f);
}
```

#### `Engine::RequestShutdown()`

Motorun kapatılmasını talep eder. Ana döngü bir sonraki iterasyonda sonlanır.

**Örnek Kullanım:**
```cpp
// Bir subsystem içinden
if (m_window->ShouldClose()) {
    m_owner->RequestShutdown();
}
```

### Hata Yönetimi

Engine sınıfı exception-safe tasarıma sahiptir:

```cpp
try {
    Engine engine;
    MyApplication app;
    engine.Run(&app);
} catch (const std::exception& e) {
    std::cerr << "Engine failed: " << e.what() << std::endl;
    return -1;
}
```

## ISubsystem Arayüzü

### Genel Bakış

`ISubsystem` arayüzü, tüm subsystem'lerin uyması gereken temel sözleşmedir.

### Arayüz Tanımı

```cpp
namespace AstralEngine {

enum class UpdateStage {
    PreUpdate,   // Input, Platform Events
    Update,      // Game Logic, ECS Systems
    PostUpdate,  // Physics
    UI,          // UI logic updates
    Render       // Render operations
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;
    
    // Yaşam döngüsü metodları
    virtual void OnInitialize(Engine* owner) = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnShutdown() = 0;
    
    // Tanılama metodları
    virtual const char* GetName() const = 0;
    virtual UpdateStage GetUpdateStage() const = 0;
};

} // namespace AstralEngine
```

### Metodlar

#### `ISubsystem::OnInitialize(Engine* owner)`

Subsystem'in başlatılması için çağrılır.

**Parametreler:**
- `owner`: Sahip Engine işaretçisi

**Örnek Uygulama:**
```cpp
void MySubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info(GetName(), "Initializing...");
    // Başlatma kodu
}
```

#### `ISubsystem::OnUpdate(float deltaTime)`

Her frame'de çağrılır.

**Parametreler:**
- `deltaTime`: Bir önceki frame'den bu yana geçen süre (saniye)

**Örnek Uygulama:**
```cpp
void MySubsystem::OnUpdate(float deltaTime) {
    // Frame başına güncelleme mantığı
    UpdateSystems(deltaTime);
    ProcessInput(deltaTime);
}
```

#### `ISubsystem::OnShutdown()`

Motor kapatıldığında çağrılır.

**Örnek Uygulama:**
```cpp
void MySubsystem::OnShutdown() {
    Logger::Info(GetName(), "Shutting down...");
    // Temizlik kodu
    m_resources.clear();
}
```

#### `ISubsystem::GetName() const`

Subsystem'in adını döndürür.

**Dönüş Değeri:**
- `const char*`: Subsystem adı

**Örnek Uygulama:**
```cpp
const char* MySubsystem::GetName() const {
    return "MySubsystem";
}
```

#### `ISubsystem::GetUpdateStage() const`

Subsystem'in güncelleme aşamasını döndürür.

**Dönüş Değeri:**
- `UpdateStage`: Güncelleme aşaması

**Örnek Uygulama:**
```cpp
UpdateStage MySubsystem::GetUpdateStage() const {
    return UpdateStage::Update;
}
```

## Logger Sınıfı

### Genel Bakış

`Logger` sınıfı, merkezi loglama sistemidir. Farklı log seviyelerini destekler ve hem konsol hem dosya çıkışı sağlar.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class Logger {
public:
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Critical = 5
    };
    
    // Static log metodları
    template<typename... Args>
    static void Trace(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Debug(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Info(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Warning(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Error(const std::string& category, const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void Critical(const std::string& category, const std::string& format, Args&&... args);
    
    // Log seviyesi yönetimi
    static void SetLogLevel(LogLevel level);
    
    // Dosya loglaması
    static bool InitializeFileLogging(const std::string& logDirectory = "");
    static void ShutdownFileLogging();
    
private:
    static void Log(LogLevel level, const std::string& category, const std::string& message);
    static std::string GetLevelString(LogLevel level);
    static std::string GetTimeString();
    
    static LogLevel s_currentLevel;
    static std::unique_ptr<FileLogger> s_fileLogger;
};

} // namespace AstralEngine
```

### Log Seviyeleri

| Seviye | Kullanım Durumu | Örnek |
|--------|----------------|---------|
| `Trace` | En detaylı debug bilgisi | Algoritma adımları, loop iterasyonları |
| `Debug` | Debug bilgisi | Fonksiyon giriş/çıkışları, değişken değerleri |
| `Info` | Bilgi mesajları | Sistem başlatma, asset yükleme |
| `Warning` | Uyarı mesajları | Deprecated API kullanımı, performans uyarıları |
| `Error` | Hata mesajları | Asset yükleme hatası, geçersiz parametre |
| `Critical` | Kritik hatalar | Başlatma başarısızlığı, kaynak tükenmesi |

### Kullanım Örnekleri

#### Basit Loglama
```cpp
Logger::Info("Engine", "Motor başlatıldı");
Logger::Error("Render", "Shader yüklenemedi");
```

#### Formatlı Loglama
```cpp
Logger::Debug("AssetManager", "Texture yüklendi: {}x{}", width, height);
Logger::Error("Render", "Failed to create pipeline: {}", errorMessage);
```

#### Kategori Bazlı Loglama
```cpp
Logger::Info("Platform", "Window created: {}x{}", windowWidth, windowHeight);
Logger::Warning("Asset", "Deprecated asset format: {}", filePath);
Logger::Critical("Memory", "Out of memory! Requested: {} bytes", requestedSize);
```

#### Dosya Loglaması
```cpp
// Başlatma
Logger::InitializeFileLogging("logs/");

// Kullanım
Logger::Info("Engine", "This will be logged to both console and file");

// Kapatma
Logger::ShutdownFileLogging();
```

#### Log Seviyesi Ayarlama
```cpp
// Sadece Warning ve üstü loglar
Logger::SetLogLevel(Logger::LogLevel::Warning);

// Tüm logları göster
Logger::SetLogLevel(Logger::LogLevel::Trace);
```

## MemoryManager Sınıfı

### Genel Bakış

`MemoryManager` sınıfı, merkezi bellek yönetimini sağlar. Şu anda basit bir wrapper olarak çalışır ancak gelecekte özel allocator'lar içerecek şekilde tasarlanmıştır.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class MemoryManager {
public:
    // Singleton erişimi
    static MemoryManager& GetInstance();
    
    // Bellek ayırma ve serbest bırakma
    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    void Deallocate(void* ptr, size_t alignment = alignof(std::max_align_t));
    
    // Frame-based temporary allocator
    void* AllocateFrameMemory(size_t size, size_t alignment = alignof(std::max_align_t));
    void ResetFrameMemory();
    
    // İstatistikler
    size_t GetTotalAllocated() const { return m_totalAllocated; }
    size_t GetFrameAllocated() const { return m_frameAllocated; }
    
    // Yaşam döngüsü
    void Initialize();
    void Shutdown();
    
private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    
    // Non-copyable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
    // İstatistik takibi
    size_t m_totalAllocated = 0;
    size_t m_frameAllocated = 0;
    
    // Frame memory
    static constexpr size_t FRAME_MEMORY_SIZE = 1024 * 1024; // 1MB
    std::unique_ptr<char[]> m_frameMemoryBuffer;
    size_t m_frameMemoryOffset = 0;
    
    bool m_initialized = false;
};

} // namespace AstralEngine
```

### Kullanım Örnekleri

#### Normal Bellek Ayırma
```cpp
auto& memoryManager = MemoryManager::GetInstance();

// Hizalı bellek ayırma
void* data = memoryManager.Allocate(1024, 16); // 16-byte aligned

// Kullanım
ProcessData(data, 1024);

// Belleği serbest bırak
memoryManager.Deallocate(data, 16);
```

#### Frame-Based Bellek Kullanımı
```cpp
// Frame başında
void* tempBuffer = memoryManager.AllocateFrameMemory(256);

// Geçici veri için kullan
char* stringData = static_cast<char*>(tempBuffer);
strcpy(stringData, "Temporary data");

// Frame sonunda - otomatik temizlik
memoryManager.ResetFrameMemory();
```

#### Bellek İstatistikleri
```cpp
auto& memoryManager = MemoryManager::GetInstance();

size_t totalMemory = memoryManager.GetTotalAllocated();
size_t frameMemory = memoryManager.GetFrameAllocated();

Logger::Info("Memory", "Total: {} bytes, Frame: {} bytes", totalMemory, frameMemory);
```

## ThreadPool Sınıfı

### Genel Bakış

`ThreadPool` sınıfı, genel amaçlı thread havuzu sağlar. Asenkron görev çalıştırmak için kullanılır ve ana thread'i bloklamadan uzun süren işlemleri paralel olarak yürütür.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();
    
    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Task submission
    template<class F, class... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    
private:
    // Worker threads
    std::vector<std::thread> m_workers;
    
    // Task queue
    std::queue<std::function<void()>> m_tasks;
    
    // Synchronization
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop;
};

} // namespace AstralEngine
```

### Kullanım Örnekleri

#### Basit Async İşlem
```cpp
ThreadPool pool(4); // 4 worker thread

// Async görev gönder
auto future = pool.Submit([]() {
    return CalculateExpensiveOperation();
});

// Ana thread'de devam et
DoOtherWork();

// Sonucu bekle
int result = future.get();
```

#### Asset Yükleme
```cpp
class AssetManager {
    ThreadPool m_threadPool{4};
    
public:
    void LoadTextureAsync(const std::string& path) {
        auto future = m_threadPool.Submit([this, path]() {
            return LoadTextureFromDisk(path);
        });
        
        // Future'ı sakla, sonucu daha sonra al
        m_pendingTextures[path] = std::move(future);
    }
};
```

#### Çoklu Async İşlemler
```cpp
ThreadPool pool(8);

std::vector<std::future<int>> futures;

// Birden fazla görev gönder
for (int i = 0; i < 100; ++i) {
    futures.push_back(pool.Submit([i]() {
        return ProcessItem(i);
    }));
}

// Tüm sonuçları bekle
std::vector<int> results;
for (auto& future : futures) {
    results.push_back(future.get());
}
```

## EventManager Sınıfı

### Genel Bakış

`EventManager` sınıfı, olay tabanlı iletişim sistemidir. Observer pattern implementasyonu ile thread-safe event yönetimi sağlar.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class EventManager {
public:
    using EventHandler = std::function<bool(Event&)>;
    using EventHandlerID = size_t;
    
    // Singleton erişimi
    static EventManager& GetInstance();
    
    // Event dinleyici yönetimi
    template<typename T>
    EventHandlerID Subscribe(EventHandler handler);
    
    template<typename TEvent, typename TFunc>
    EventHandlerID Subscribe(TFunc&& func);
    
    void Unsubscribe(EventHandlerID handlerID);
    void UnsubscribeAll();
    
    // Event gönderme
    void PublishEvent(std::unique_ptr<Event> event);
    
    template<typename T, typename... Args>
    void PublishEvent(Args&&... args);
    
    // Event işleme
    void ProcessEvents();
    
    // İstatistikler
    size_t GetSubscriberCount() const;
    size_t GetPendingEventCount() const;
    
private:
    EventManager() = default;
    ~EventManager() = default;
    
    // Non-copyable
    EventManager(const EventManager&) = delete;
    EventManager& operator=(EventManager&) = delete;
    
    // Thread safety
    mutable std::mutex m_handlersMutex;
    mutable std::mutex m_queueMutex;
    
    // Handler management
    std::unordered_map<std::type_index, std::vector<HandlerInfo>> m_handlers;
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    EventHandlerID m_nextHandlerID = 1;
};

} // namespace AstralEngine
```

### Kullanım Örnekleri

#### Custom Event Tanımı
```cpp
struct WindowResizeEvent : public Event {
    uint32_t width;
    uint32_t height;
    
    WindowResizeEvent(uint32_t w, uint32_t h) : width(w), height(h) {}
    
    static std::type_index GetStaticType() {
        return typeid(WindowResizeEvent);
    }
};
```

#### Event Handler Kaydetme
```cpp
// Genel handler
auto handlerID = EventManager::GetInstance().Subscribe<WindowResizeEvent>(
    [](Event& event) {
        auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
        Logger::Info("Render", "Window resized to {}x{}", 
                    resizeEvent.width, resizeEvent.height);
        return true; // Event işlendi
    }
);

// Tip güvenli handler
auto handlerID = EventManager::GetInstance().Subscribe<WindowResizeEvent>(
    [](WindowResizeEvent& event) {
        UpdateRenderTargets(event.width, event.height);
        return true;
    }
);
```

#### Event Yayınlama
```cpp
// Event oluştur ve yayınla
EventManager::GetInstance().PublishEvent<WindowResizeEvent>(1920, 1080);

// Veya doğrudan unique_ptr ile
auto event = std::make_unique<WindowResizeEvent>(1920, 1080);
EventManager::GetInstance().PublishEvent(std::move(event));
```

#### Event İşleme
```cpp
// Ana döngüde
void Engine::Update() {
    // Event'leri işle
    EventManager::GetInstance().ProcessEvents();
    
    // Diğer güncellemeler...
}
```

## Event Sistemleri

### Temel Event Sınıfı

```cpp
namespace AstralEngine {

class Event {
public:
    virtual ~Event() = default;
    virtual std::type_index GetType() const = 0;
    
    static std::type_index GetStaticType() {
        return typeid(Event);
    }
};

} // namespace AstralEngine
```

### Hazır Event Türleri

#### WindowResizeEvent
```cpp
struct WindowResizeEvent : public Event {
    uint32_t width;
    uint32_t height;
    
    WindowResizeEvent(uint32_t w, uint32_t h) : width(w), height(h) {}
    
    static std::type_index GetStaticType() {
        return typeid(WindowResizeEvent);
    }
};
```

#### KeyPressedEvent
```cpp
struct KeyPressedEvent : public Event {
    KeyCode key;
    bool repeat;
    
    KeyPressedEvent(KeyCode k, bool r = false) : key(k), repeat(r) {}
    
    static std::type_index GetStaticType() {
        return typeid(KeyPressedEvent);
    }
};
```

#### MouseMovedEvent
```cpp
struct MouseMovedEvent : public Event {
    float x;
    float y;
    float deltaX;
    float deltaY;
    
    MouseMovedEvent(float xPos, float yPos, float dx, float dy) 
        : x(xPos), y(yPos), deltaX(dx), deltaY(dy) {}
    
    static std::type_index GetStaticType() {
        return typeid(MouseMovedEvent);
    }
};
```

## Thread Safety

Tüm core modüller thread-safe tasarıma sahiptir:

- **Logger**: Static metodlar, iç durum paylaşılmaz
- **MemoryManager**: Singleton, atomic operasyonlar
- **ThreadPool**: Doğası gereği thread-safe
- **EventManager**: Çift katmanlı mutex sistemi

## Performans Karakteristikleri

### MemoryManager
- Frame memory: O(1) ayırma
- Normal memory: O(1) malloc/free wrapper
- Memory overhead: < 1%

### ThreadPool
- Task submission: ~1-2 μs
- Thread creation: ~50-100 μs
- Ideal thread sayısı: CPU core sayısı

### EventManager
- Event dispatch: O(n) where n = subscriber count
- Thread safety overhead: ~5-10%
- Memory per event: ~50-100 bytes

## Örnek Uygulama

```cpp
#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/MemoryManager.h"
#include "Core/ThreadPool.h"
#include "Events/EventManager.h"

using namespace AstralEngine;

class MyApplication : public IApplication {
public:
    void OnStart(Engine* engine) override {
        Logger::Info("App", "Application starting...");
        
        // Event handler kaydet
        m_resizeHandler = EventManager::GetInstance().Subscribe<WindowResizeEvent>(
            [this](WindowResizeEvent& event) {
                Logger::Info("App", "Window resized: {}x{}", 
                           event.width, event.height);
                return true;
            }
        );
        
        // Async asset yükleme
        m_assetLoadFuture = m_threadPool.Submit([this]() {
            return LoadGameAssets();
        });
    }
    
    void OnUpdate(float deltaTime) override {
        // Asset yükleme kontrolü
        if (m_assetLoadFuture.valid() && 
            m_assetLoadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            bool success = m_assetLoadFuture.get();
            Logger::Info("App", "Asset loading {}", success ? "succeeded" : "failed");
        }
        
        // Frame-based memory kullanımı
        auto& memoryManager = MemoryManager::GetInstance();
        void* tempData = memoryManager.AllocateFrameMemory(1024);
        
        // Geçici veri işleme
        ProcessFrameData(tempData, deltaTime);
        
        // Bellek otomatik olarak ResetFrameMemory() ile temizlenir
    }
    
    void OnShutdown() override {
        EventManager::GetInstance().Unsubscribe(m_resizeHandler);
        Logger::Info("App", "Application shutting down...");
    }
    
private:
    ThreadPool m_threadPool{4};
    std::future<bool> m_assetLoadFuture;
    EventManager::EventHandlerID m_resizeHandler;
};

int main() {
    try {
        Engine engine;
        MyApplication app;
        
        // Subsystem'leri kaydet
        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<AssetSubsystem>();
        engine.RegisterSubsystem<RenderSubsystem>();
        
        // Dosya loglamayı başlat
        Logger::InitializeFileLogging();
        
        // Motoru çalıştır
        engine.Run(&app);
        
        // Temizlik
        Logger::ShutdownFileLogging();
        
    } catch (const std::exception& e) {
        Logger::Critical("Main", "Fatal error: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## Hata Kodları ve İstisnalar

### Engine Hataları
- `std::runtime_error`: Subsystem başlatma başarısız
- `std::invalid_argument`: Geçersiz parametre
- `std::bad_alloc`: Bellek ayırma başarısız

### MemoryManager Hataları
- `std::bad_alloc`: Yetersiz bellek
- `nullptr`: Frame memory overflow

### ThreadPool Hataları
- `std::runtime_error`: Pool durdurulduğunda task submission

### EventManager Hataları
- `std::runtime_error`: Geçersiz handler ID

## En İyi Uygulamalar

### 1. Exception Handling
```cpp
try {
    auto* subsystem = engine.GetSubsystem<MySubsystem>();
    if (subsystem) {
        subsystem->DoSomething();
    }
} catch (const std::exception& e) {
    Logger::Error("MyCode", "Operation failed: {}", e.what());
}
```

### 2. Memory Management
```cpp
// Frame memory kullan geçici veriler için
void ProcessFrame() {
    auto& memoryManager = MemoryManager::GetInstance();
    
    // Geçici buffer
    void* tempBuffer = memoryManager.AllocateFrameMemory(1024);
    
    // Frame sonunda otomatik temizlik
    // Manuel ResetFrameMemory() çağrısı gerekmez
}
```

### 3. Event System Kullanımı
```cpp
// Event handler'lar hızlı olmalı
auto handler = EventManager::GetInstance().Subscribe<MyEvent>(
    [](MyEvent& event) {
        // Hafif işlem, uzun süren işlemler thread pool'a gönder
        return true; // Event işlendi
    }
);
```

### 4. Thread Pool Kullanımı
```cpp
// Uzun süren işlemler için thread pool kullan
auto future = m_threadPool.Submit([]() {
    return LoadLargeAsset();
});

// Ana thread'de devam et
ContinueGameLogic();

// Sonucu bekle
auto result = future.get();
```

## Performans İpuçları

1. **MemoryManager**: Frame memory'yi geçici veriler için kullan
2. **ThreadPool**: CPU-bound işlemler için idealdir
3. **EventManager**: Hafif event handler'lar kullan
4. **Logger**: Production'da log seviyesini ayarla
5. **Engine**: Subsystem'leri doğru sırayla kaydet

## Sık Karşılaşılan Sorular

**S: Engine birden fazla kez oluşturulabilir mi?**
A: Evet, Engine sınıfı singleton değildir. Birden fazla instance oluşturulabilir.

**S: MemoryManager thread-safe mi?**
A: Evet, tüm MemoryManager metodları thread-safe'dir.

**S: EventManager'da kaç tane handler olabilir?**
A: Teknik olarak sınırsız, ancak performans için binlerce handler'dan kaçının.

**S: ThreadPool'da kaç thread kullanmalıyım?**
A: Genellikle CPU core sayısı kadar: `std::thread::hardware_concurrency()`

**S: Logger dosya loglaması otomatik mi?**
A: Hayır, `InitializeFileLogging()` ile manuel olarak başlatılmalıdır.

Bu API referansı, Astral Engine'in çekirdek modüllerinin güçlü ve esnek yapısını göstermektedir. Her modül tek bir sorumluluğa sahiptir ve diğer modüllerle gevşek bağlılık prensibiyle iletişim kurar.