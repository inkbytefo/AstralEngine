# Astral Engine - Mimari Genel Bakış

Astral Engine, modern oyun geliştirme için tasarlanmış modüler, esnek ve yüksek performanslı bir C++20 oyun motorudur. Bu dokümantasyon, motorun temel mimari prensiplerini, yapısal tasarımını ve bileşen arasındaki ilişkileri açıklamaktadır.

## 🏗️ Temel Mimari Prensipler

### 1. Modüler Subsystem Mimarisi

Astral Engine, **subsystem-based modular architecture** kullanır. Her subsystem bağımsız bir modül olarak çalışır ve belirli bir sorumluluğa sahiptir:

```
Engine (Çekirdek Orkestratör)
├── PlatformSubsystem (Pencere, Input)
├── AssetSubsystem (Varlık Yönetimi)
├── ECSSubsystem (Oyun Mantığı)
├── RenderSubsystem (Grafik)
├── PhysicsSubsystem (Fizik)
└── AudioSubsystem (Ses)
```

**Avantajları:**
- **Bağımsız Geliştirme**: Her subsystem ayrı ayrı geliştirilebilir ve test edilebilir
- **Kolay Genişletme**: Yeni özellikler eklemek için yeni subsystem'ler oluşturulur
- **Değiştirilebilirlik**: Bir subsystem (örneğin renderer) kolayca değiştirilebilir
- **Test Koluluğu**: Her subsystem bağımsız olarak test edilebilir

### 2. Soyutlama (Abstraction)

Her alt sistem, kendi iç karmaşıklığını gizler ve dışarıya temiz bir arayüz sunar:

```cpp
// Örnek: Renderer soyutlama
class IRenderer {
public:
    virtual bool Initialize(GraphicsDevice* device) = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    // ... diğer temel fonksiyonlar
};
```

Bu sayede:
- **API Değişiklikleri**: İç implementasyon değişirken dış arayüz sabit kalır
- **Platform Bağımsızlık**: Farklı platformlarda aynı API kullanılır
- **Kolay Entegrasyon**: Sistemler arasında standart iletişim imkanı

### 3. Veri Odaklı Tasarım (Data-Oriented Design)

Motorun merkezinde, verilerin kendisi ve bu verilerin nasıl işlendiği yer alır. ECS (Entity Component System) pattern ile performans odaklı tasarım:

```cpp
// Veri yapısı (cache-friendly)
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

// Sistem (veri üzerinde çalışır)
class TransformSystem {
public:
    void Update(std::vector<TransformComponent>& transforms) {
        // Tüm transform verilerini tek seferde işle
        for (auto& transform : transforms) {
            // Transform güncelleme mantığı
        }
    }
};
```

**Avantajları:**
- **Performans**: Bellek erişimi optimize edilir, cache hit oranları artar
- **Paralellik**: Veri parçaları bağımsız olarak işlenebilir
- **Öngörülebilirlik**: Bellek kullanımı öngörülebilir hale gelir

### 4. Gevşek Bağlılık (Loose Coupling)

Alt sistemler birbirine doğrudan bağımlı olmamalıdır. İletişim, kontrollü arayüzler üzerinden sağlanır:

```cpp
// Event-driven iletişim
EventManager& eventMgr = EventManager::GetInstance();
eventMgr.Subscribe<WindowResizeEvent>([](Event& event) {
    // Pencere yeniden boyutlandırma olayı
    return true;
});

// Event gönderme
eventMgr.PublishEvent<WindowResizeEvent>(1920, 1080);
```

**İletişim Yöntemleri:**
1. **Event System**: Anlık ve asenkron olaylar için
2. **ECS (Merkezi Veri Havuzu)**: Sistemler arası dolaylı iletişim
3. **Service Locator**: Bir sistemin başka bir sistemin servisine erişimi

### 5. Thread Safety

Çoklu iş parçacığı desteği ile modern donanımlardan en iyi şekilde faydalanma:

```cpp
// Thread-safe event yönetimi
class EventManager {
    mutable std::mutex m_handlersMutex;  // Handler yönetimi için
    mutable std::mutex m_queueMutex;     // Event kuyruğu için
    
    void ProcessEvents() {
        // Ana thread'de güvenli event işleme
    }
};
```

## 🔄 Ana Döngü (Frame Yaşam Döngüsü)

`Engine::Run()` metodu içindeki bir frame'in tipik akışı:

```cpp
void Engine::Run() {
    while (m_isRunning) {
        // 1. Zaman Yönetimi
        float deltaTime = CalculateDeltaTime();
        
        // 2. Platform Güncellemesi
        m_platformSubsystem->OnUpdate(deltaTime);
        
        // 3. Event İşleme
        EventManager::GetInstance().ProcessEvents();
        
        // 4. Asset Yönetimi
        m_assetSubsystem->OnUpdate(deltaTime);
        
        // 5. Oyun Mantığı
        m_ecsSubsystem->OnUpdate(deltaTime);
        
        // 6. Render Verisi Gönderimi
        RenderData renderData = m_ecsSubsystem->GetRenderData();
        
        // 7. Çizim
        m_renderSubsystem->OnUpdate(deltaTime, renderData);
        
        // 8. Frame Sonu
        m_memoryManager->ResetFrameMemory();
    }
}
```

## 📊 Bileşen Arasındaki İlişkiler

### Bağımlılık Grafiği

```
Engine (Orkestratör)
├── PlatformSubsystem ──┐
├── AssetSubsystem ─────┤
├── ECSSubsystem ──────┤
├── RenderSubsystem ───┼──> GraphicsDevice
├── PhysicsSubsystem ──┤
└── AudioSubsystem ─────┘
```

### Veri Akışı

1. **Platform → Event**: Platform olayları event sistemine gönderilir
2. **Event → ECS**: Event'ler ECS sistemini tetikler
3. **ECS → Render**: Render verileri ECS'ten renderer'a aktarılır
4. **Asset → ECS**: Asset'ler ECS tarafından yüklenir ve kullanılır

## 🎯 Tasarım Desenleri

### 1. Singleton Pattern
- **Kullanım**: Logger, EventManager, MemoryManager
- **Avantaj**: Global erişim ve tekil örnek yönetimi

### 2. Observer Pattern
- **Kullanım**: Event sistemi
- **Avantaj**: Gevşek bağlılık ve olay tabanlı iletişim

### 3. RAII (Resource Acquisition Is Initialization)
- **Kullanım**: Vulkan kaynakları, bellek yönetimi
- **Avantaj**: Otomatik kaynak yönetimi ve exception safety

### 4. Factory Pattern
- **Kullanım**: Asset yükleme, subsystem oluşturma
- **Avantaj**: Esnek nesne oluşturma ve polymorphism

## 🛡️ Hata Yönetimi

### Exception Handling
```cpp
try {
    engine.Run();
} catch (const std::exception& e) {
    Logger::Critical("Engine", "Fatal exception: {}", e.what());
    return -1;
}
```

### Vulkan Hata Yönetimi
```cpp
void HandleVulkanError(VkResult result, const std::string& operation) {
    if (result != VK_SUCCESS) {
        Logger::Error("Vulkan", "{} failed with error: {}", operation, result);
        // Hata kurtarma veya güvenli kapatma
    }
}
```

## 📈 Performans Optimizasyonları

### 1. Bellek Yönetimi
- **Pool Allocator**: Sıkça ayrılan/serbest bırakılan bellek için
- **Frame-based Memory**: Frame başına geçici bellek için
- **Cache-friendly Data Structures**: Veri odaklı tasarım

### 2. Rendering Optimizasyonları
- **Batch Rendering**: Çoklu nesnelerin tek çizim komutuyla çizilmesi
- **Frustum Culling**: Görünmeyen nesnelerin atılması
- **Level of Detail**: Nesne detayının mesafeye göre ayarlanması

### 3. Paralel İşleme
- **Multi-threaded Asset Loading**: Asset'lerin paralel olarak yüklenmesi
- **Job System**: Bağımsız görevlerin paralel çalıştırılması
- **ECS Paralellik**: ECS sistemlerinin paralel çalıştırılması

## 🔮 Gelecek Geliştirmeler

### 1. Plugin Sistemi
- **Dynamic Loading**: Runtime'da subsystem'lerin yüklenmesi
- **Hot Reload**: Kod değişikliklerinin runtime'da uygulanması

### 2. Scripting Desteği
- **Lua Integration**: Lua scripting desteği
- **Reflection**: C++ objelerine runtime erişim

### 3. Networking
- **Multiplayer**: Çok oyunculu ağ desteği
- **Replication**: Oyun durumunun senkronizasyonu

### 4. Visual Editor
- **Scene Editor**: Görsel sane editörü
- **Component Inspector**: Bileşenlerin görsel olarak düzenlenmesi

---

Bu mimari tasarım, Astral Engine'in modern oyun geliştirme ihtiyaçlarını karşılamak için esnek, performanslı ve bakımı kolay bir temel oluşturmayı hedeflemektedir.