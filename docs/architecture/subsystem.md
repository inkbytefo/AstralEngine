# Astral Engine - Subsystem Mimarisi

Subsystem mimarisi, Astral Engine'in temelini oluşturan ve tüm sistemin nasıl organize edildiğini belirleyen yapısal tasarım desenidir. Bu dokümantasyon, subsystem'lerin nasıl çalıştığını, nasıl entegre edildiklerini ve nasıl genişletilebileceklerini açıklamaktadır.

## 🏗️ Subsystem Temelleri

### ISubsystem Arayüzü

Tüm subsystem'ler, [`ISubsystem`](Source/Core/ISubsystem.h:14) arayüzünü uygular. Bu arayüz, motorun tüm sistemlerini tek tip olarak yönetmesini sağlar:

```cpp
class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    // Motor başlatıldığında bir kez çağrılır
    virtual void OnInitialize(Engine* owner) = 0;

    // Her frame'de ana döngü tarafından çağrılır
    virtual void OnUpdate(float deltaTime) = 0;

    // Motor kapatıldığında bir kez çağrılır
    virtual void OnShutdown() = 0;

    // Hata ayıklama için sistemin adını döndürür
    virtual const char* GetName() const = 0;
};
```

### Engine Çekirdek Sınıfı

[`Engine`](Source/Core/Engine.h:19) sınıfı, motorun orkestra şefidir. Alt sistemleri kaydeder, yaşam döngülerini yönetir ve ana döngüyü çalıştırır:

```cpp
class Engine {
public:
    // Bir alt sistemi kaydeder
    template<typename T, typename... Args>
    void RegisterSubsystem(Args&&... args);

    // Kayıtlı bir alt sisteme erişim sağlar
    template<typename T>
    T* GetSubsystem() const;

    // Motorun ana yaşam döngüsünü başlatır
    void Run();

private:
    std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
    std::unordered_map<std::type_index, ISubsystem*> m_subsystemMap;
};
```

## 📋 Mevcut Subsystem'ler

### 1. PlatformSubsystem

**Sorumlulukları:**
- İşletim sistemi ile ilgili tüm işlemleri soyutlamak
- Pencere oluşturma ve yönetimi
- Kullanıcı girdilerini (klavye, fare) işleme
- Olay döngüsünü yönetme

**Bağımlılıklar:**
- [`Logger`](Source/Core/Logger.h:22) - Hata loglama
- [`EventManager`](Source/Events/EventManager.h:19) - Event gönderme

**Kullanım:**
```cpp
engine.RegisterSubsystem<PlatformSubsystem>();
auto* platform = engine.GetSubsystem<PlatformSubsystem>();
```

### 2. AssetSubsystem

**Sorumlulukları:**
- Varlıkların (modeller, dokular, shader'lar) diskten yüklenmesi
- Bellekte tutulması ve önbellekleme
- Diğer sistemlere varlık sunulması
- Asenkron yükleme desteği

**Bağımlılıklar:**
- [`AssetManager`](Source/Subsystems/Asset/AssetManager.h:22) - Asset yönetimi
- [`Logger`](Source/Core/Logger.h:22) - Hata loglama

**Kullanım:**
```cpp
engine.RegisterSubsystem<AssetSubsystem>();
auto* assetSubsystem = engine.GetSubsystem<AssetSubsystem>();
auto* assetManager = assetSubsystem->GetAssetManager();
```

### 3. RenderSubsystem

**Sorumlulukları:**
- Grafik API'sini (Vulkan) soyutlamak
- Sahneyi ekrana çizmek
- Render komutlarını yönetmek
- Shader ve pipeline yönetimi

**Bağımlılıklar:**
- [`PlatformSubsystem`](Source/Subsystems/Platform/PlatformSubsystem.h) - Pencere handle
- [`AssetSubsystem`](Source/Subsystems/Asset/AssetSubsystem.h) - Shader ve texture yükleme
- [`VulkanRenderer`](Source/Subsystems/Renderer/VulkanRenderer.h:33) - Vulkan implementasyonu

**Kullanım:**
```cpp
engine.RegisterSubsystem<RenderSubsystem>();
auto* renderSubsystem = engine.GetSubsystem<RenderSubsystem>();
```

## 🔧 Subsystem Kayıt Sırası

Subsystem'lerin kayıt sırası kritik öneme sahiptir. Bağımlılıklar nedeniyle belirli bir sırada kaydedilmelidir:

```cpp
// DOĞRU SIRA
engine.RegisterSubsystem<PlatformSubsystem>();  // 1. Pencere ve input
engine.RegisterSubsystem<AssetSubsystem>();     // 2. Asset yönetimi
engine.RegisterSubsystem<RenderSubsystem>();   // 3. Rendering (pencereye ihtiyaç var)
```

### Neden Bu Sıra?

1. **PlatformSubsystem**: RenderSubsystem pencere handle'ına ihtiyaç duyar
2. **AssetSubsystem**: RenderSubsystem shader ve texture yüklemek için ihtiyaç duyar
3. **RenderSubsystem**: Diğer sistemlere rendering hizmeti sunar

## 🔄 Subsystem Yaşam Döngüsü

### Başlatma (Initialization)

```cpp
void Engine::Initialize() {
    for (auto& subsystem : m_subsystems) {
        subsystem->OnInitialize(this);
        Logger::Info("Engine", "Subsystem '{}' initialized", subsystem->GetName());
    }
}
```

### Güncelleme (Update)

```cpp
void Engine::Update() {
    float deltaTime = CalculateDeltaTime();
    
    for (auto& subsystem : m_subsystems) {
        subsystem->OnUpdate(deltaTime);
    }
}
```

### Kapatma (Shutdown)

```cpp
void Engine::Shutdown() {
    // Ters sırada kapatma
    for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it) {
        (*it)->OnShutdown();
        Logger::Info("Engine", "Subsystem '{}' shutdown", (*it)->GetName());
    }
}
```

## 🎯 Subsystem Geliştirme

### Yeni Subsystem Oluşturma

Yeni bir subsystem oluşturmak için `ISubsystem` arayüzünü uygulamak yeterlidir:

```cpp
class PhysicsSubsystem : public ISubsystem {
public:
    void OnInitialize(Engine* owner) override {
        // Fizik motoru başlatma
        m_physicsWorld = std::make_unique<PhysicsWorld>();
    }

    void OnUpdate(float deltaTime) override {
        // Fizik simülasyonu güncelleme
        m_physicsWorld->Step(deltaTime);
    }

    void OnShutdown() override {
        // Fizik motoru kapatma
        m_physicsWorld.reset();
    }

    const char* GetName() const override { return "PhysicsSubsystem"; }

private:
    std::unique_ptr<PhysicsWorld> m_physicsWorld;
};
```

### Subsystem Entegrasyonu

Yeni subsystem'i motor entegre etmek için:

```cpp
// main.cpp'de
engine.RegisterSubsystem<PhysicsSubsystem>();

// Diğer subsystem'lerden erişim
auto* physics = engine.GetSubsystem<PhysicsSubsystem>();
```

## 🔄 Subsystem Arası İletişim

### 1. Event Sistemi Kullanımı

Subsystem'ler arasında event-driven iletişim:

```cpp
// Event dinleyici kaydetme
EventManager& eventMgr = EventManager::GetInstance();
auto handlerID = eventMgr.Subscribe<WindowResizeEvent>(
    [](Event& event) {
        auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
        // Pencere yeniden boyutlandırma işlemleri
        return true;
    });

// Event gönderme
eventMgr.PublishEvent<WindowResizeEvent>(1920, 1080);
```

### 2. Service Locator Pattern

Bir subsystem'in başka bir subsystem'in servisine erişimi:

```cpp
// AssetSubsystem'ten AssetManager erişimi
AssetManager* AssetSubsystem::GetAssetManager() const {
    return m_assetManager.get();
}

// Diğer subsystem'lerden kullanım
auto* assetManager = engine.GetSubsystem<AssetSubsystem>()->GetAssetManager();
```

### 3. Veri Paylaşımı

ECS verilerinin paylaşımı:

```cpp
// RenderSubsystem'den render verisi talep etme
RenderData renderData = engine.GetSubsystem<ECSSubsystem>()->GetRenderData();

// Render verisini kullanma
m_renderer->Submit(renderData.commands);
```

## 🛡️ Hata Yönetimi

### Subsystem Hataları

Subsystem'lerde hata yönetimi için try-catch blokları:

```cpp
void Engine::Initialize() {
    for (auto& subsystem : m_subsystems) {
        try {
            subsystem->OnInitialize(this);
        } catch (const std::exception& e) {
            Logger::Critical("Engine", "Subsystem '{}' failed to initialize: {}", 
                           subsystem->GetName(), e.what());
            // Hata kurtarma veya güvenli kapatma
        }
    }
}
```

### Vulkan Hata Yönetimi

Vulkan kaynaklarında hata yönetimi:

```cpp
void VulkanRenderer::HandleVulkanError(VkResult result, const std::string& operation) {
    if (result != VK_SUCCESS) {
        Logger::Error("Vulkan", "{} failed with error: {}", operation, result);
        // Güvenli kapatma
        Shutdown();
    }
}
```

## 📊 Performans Optimizasyonları

### 1. Lazy Initialization

Subsystem'lerin ihtiyaç duyulduğunda başlatılması:

```cpp
void Engine::GetSubsystem<T>() {
    if (!m_subsystemInitialized[typeid(T)]) {
        auto* subsystem = static_cast<T*>(m_subsystemMap[typeid(T)]);
        subsystem->OnInitialize(this);
        m_subsystemInitialized[typeid(T)] = true;
    }
    return static_cast<T*>(m_subsystemMap[typeid(T)]);
}
```

### 2. Paralel Güncelleme

Bağımsız subsystem'lerin paralel güncellenmesi:

```cpp
void Engine::Update() {
    float deltaTime = CalculateDeltaTime();
    
    // Bağımsız subsystem'leri paralel güncelleme
    std::vector<std::future<void>> futures;
    
    for (auto& subsystem : m_parallelSubsystems) {
        futures.push_back(std::async(std::launch::async, [&subsystem, deltaTime]() {
            subsystem->OnUpdate(deltaTime);
        }));
    }
    
    // Seri güncelleme
    for (auto& subsystem : m_serialSubsystems) {
        subsystem->OnUpdate(deltaTime);
    }
    
    // Paralel işlemleri bekle
    for (auto& future : futures) {
        future.wait();
    }
}
```

### 3. Asset Önbellekleme

Asset'lerin tekrar tekrar yüklenmesini önleme:

```cpp
std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& filePath) {
    // Önbellekte kontrol et
    auto cached = GetFromCache<Texture>(filePath);
    if (cached) {
        return cached;
    }
    
    // Yeni texture yükle
    auto texture = std::make_shared<Texture>(filePath);
    AddToCache(filePath, texture);
    return texture;
}
```

## 🔮 Gelecek Geliştirmeler

### 1. Plugin Sistemi

Runtime'da subsystem'lerin dinamik yüklenmesi:

```cpp
class PluginManager {
public:
    void LoadSubsystem(const std::string& pluginPath);
    void UnloadSubsystem(const std::string& pluginName);
    
private:
    std::unordered_map<std::string, std::unique_ptr<ISubsystem>> m_loadedPlugins;
};
```

### 2. Hot Reload

Subsystem'lerin runtime'da yeniden yüklenmesi:

```cpp
void Engine::ReloadSubsystem(const std::string& subsystemName) {
    // Mevcut subsystem'i kapat
    auto* subsystem = FindSubsystem(subsystemName);
    if (subsystem) {
        subsystem->OnShutdown();
    }
    
    // Yeni subsystem'i yükle
    LoadSubsystem(subsystemName);
}
```

### 3. Dependency Injection

Subsystem bağımlılıklarının enjeksiyonu:

```cpp
template<typename T>
class SubsystemFactory {
public:
    static std::unique_ptr<ISubsystem> Create(Engine* engine) {
        return std::make_unique<T>(engine->GetSubsystem<Dependency1>(),
                                  engine->GetSubsystem<Dependency2>());
    }
};
```

---

Subsystem mimarisi, Astral Engine'in esnek, genişletilebilir ve bakımı kolay bir temel oluşturmasını sağlamaktadır. Bu yapı, yeni özelliklerin eklenmesini ve mevcut sistemlerin değiştirilmesini kolaylaştırır.