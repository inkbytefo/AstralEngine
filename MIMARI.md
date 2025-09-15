# Astral Engine - Mimari Tasarım Belgesi

## 1. Felsefe ve Temel Prensipler

Bu mimarinin temel amacı, **modüler, ölçeklenebilir, veri odaklı ve bakımı kolay** bir oyun motoru oluşturmaktır. Her bir parça net bir sorumluluğa sahip olacak ve diğer parçalarla olan bağımlılığı minimumda tutulacaktır.

-   **Modülerlik:** Motor, "alt sistemler" (subsystems) adı verilen bağımsız modüllerden oluşur. Yeni bir özellik (örneğin, fizik motoru) eklemek, yeni bir alt sistem oluşturmak kadar kolay olmalıdır.
-   **Soyutlama (Abstraction):** Her alt sistem, kendi iç karmaşıklığını gizler ve dışarıya temiz bir arayüz sunar. Örneğin, `RenderSubsystem`, Vulkan mı yoksa DirectX mi kullandığını diğer sistemlerden gizler.
-   **Veri Odaklı Tasarım (Data-Oriented Design):** Motorun merkezinde, verilerin kendisi ve bu verilerin nasıl işlendiği yer alır. ECS (Entity Component System), bu prensibin temelini oluşturur. Sistemler, bileşen verileri üzerinde çalışır.
-   **Gevşek Bağlılık (Loose Coupling):** Alt sistemler birbirine doğrudan bağımlı olmamalıdır. İletişim, kontrollü arayüzler (olaylar, mesajlar, merkezi veri havuzları) üzerinden sağlanmalıdır. Bu, bir sistemi değiştirmenin diğerlerini bozmamasını sağlar.
-   **Paralellik:** Bağımsız alt sistemler, modern çok çekirdekli işlemcilerden en iyi şekilde faydalanmak için potansiyel olarak ayrı iş parçacıklarında (threads) çalıştırılabilir.

## 2. Çekirdek Mimari: `Engine` ve `ISubsystem`

Motorun kalbinde, tüm alt sistemlerin yaşam döngüsünü yöneten bir **`Engine`** sınıfı ve tüm alt sistemlerin uyması gereken bir **`ISubsystem`** arayüzü bulunur.

### 2.1. `ISubsystem` Arayüzü

Bu, her alt sistemin uyması gereken sözleşmedir. Motorun tüm modülleri tek tip olarak yönetmesini sağlar.

```cpp
class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    // Motor başlatıldığında bir kez çağrılır.
    virtual void OnInitialize(Engine* owner) = 0;

    // Her frame'de ana döngü tarafından çağrılır.
    virtual void OnUpdate(float deltaTime) = 0;

    // Motor kapatıldığında bir kez çağrılır.
    virtual void OnShutdown() = 0;

    // Hata ayıklama ve tanılama için sistemin adını döndürür.
    virtual const char* GetName() const = 0;
};
```

### 2.2. `Engine` Çekirdek Sınıfı

`Engine` sınıfı, motorun orkestra şefidir. Alt sistemleri kaydeder, yaşam döngülerini (initialize, update, shutdown) yönetir ve ana döngüyü çalıştırır.

## 3. Alt Sistemlerin Detaylı Açıklaması

### 3.1. `PlatformSubsystem`
-   **Sorumlulukları:** İşletim sistemi ile ilgili tüm işlemleri soyutlamak. Pencere oluşturma, kullanıcı girdilerini (klavye, fare) işleme ve olay döngüsünü yönetme.
-   **Yönettiği Sınıflar:** `Window`, `InputManager`.
-   **Çıktısı:** `OnUpdate` içinde `window.pollEvents()` çağrısı yaparak `Event` sistemine olayları gönderir.

### 3.2. `AssetSubsystem`
-   **Sorumlulukları:** Varlıkların (modeller, dokular, materyaller) diskten yüklenmesi, yönetilmesi ve bellekte tutulması.
-   **Yönettiği Sınıflar:** `AssetManager`, `AssetLocator`.
-   **İşleyişi:** Diğer sistemler, ihtiyaç duydukları varlıkları bu alt sistem üzerinden asenkron olarak talep eder.

### 3.3. `ECSSubsystem`
-   **Sorumlulukları:** Oyun dünyasının durumunu yönetmek. Entity, Component ve System'lerin yaşam döngüsünden sorumludur.
-   **İşleyişi:** `OnUpdate` içinde, tüm mantık sistemlerini günceller ve render verilerini toplayıp `RenderSubsystem`'e gönderir.

### 3.4. `RenderSubsystem`
-   **Sorumlulukları:** Grafik API'sini (Vulkan) soyutlamak ve sahneyi ekrana çizmek.
-   **Yönettiği Sınıflar:** `GraphicsDevice`, `RenderScene`, `FrameRenderer`.
-   **İşleyişi:** Diğer sistemlerden tamamen soyutlanmıştır. Sadece kendisine gönderilen render verisi ile çalışır.

## 4. Sistemler Arası İletişim ve Veri Akışı

Gevşek bağlılığı korumak için sistemler arası iletişim kritik öneme sahiptir. Üç ana iletişim yöntemi kullanılacaktır:

1.  **ECS (Merkezi Veri Havuzu):** Sistemler arası dolaylı iletişimin ana yöntemidir.

2.  **Olay Sistemi (Event System):** Anlık ve asenkron olaylar için kullanılır.

3.  **Servis Konumlandırıcı (Service Locator):** Bir alt sistemin, başka bir alt sistemin sunduğu bir servise doğrudan erişmesi gerektiğinde kullanılır.

### Örnek Veri Akışı: ECS'den Renderer'a

1.  **Frame Başlangıcı:** `ECSSubsystem::OnUpdate()` çağrılır.
2.  **Mantık Güncellemesi:** ECS içindeki sistemler çalışarak bileşenleri günceller.
3.  **Render Verisi Toplama:** `ECSSubsystem`, render bileşenlerine sahip entity'leri sorgular.
4.  **Veri Gönderimi:** Toplanan verilerden oluşan bir liste `RenderSubsystem`'e gönderilir.
5.  **Render İşlemi:** `RenderSubsystem` bu veriyi kullanarak sahneyi çizer.

## 5. Ana Döngü (Frame Yaşam Döngüsü)

`Engine::Run()` metodu içindeki bir frame'in tipik akışı:

1.  **Zaman Yönetimi:** Delta time hesaplama
2.  **Platform Güncellemesi:** `PlatformSubsystem::OnUpdate()`
3.  **Asset Yönetimi:** `AssetSubsystem::OnUpdate()` (isteğe bağlı)
4.  **Oyun Mantığı:** `ECSSubsystem::OnUpdate()`
5.  **Render Verisi Gönderimi:** ECS'den render sistemine veri aktarımı
6.  **Çizim:** `RenderSubsystem::OnUpdate()`
7.  **Frame Sonu:** Bellek yöneticisi sıfırlama, performans sayaçları

## 6. Uygulama Planı ve İlk Adımlar

1.  **Çekirdek Yapıyı Oluşturun:** `Engine` ve `ISubsystem` sınıfları
2.  **İlk Subsystem: `PlatformSubsystem`:** Boş pencere açma
3.  **Temel Alt Sistemleri Entegre Edin:** Logger, MemoryManager
4.  **`ECSSubsystem`'i Oluşturun:** Mevcut ECS kodunu entegre etme
5.  **`RenderSubsystem`'i Oluşturun:** Temel render işlevi
6.  **Veri Akışını Kurun:** ECS'den renderer'a veri gönderimi

Bu belge, Astral Engine'in temelini sağlam ve geleceğe dönük bir şekilde atmak için bir başlangıç noktasıdır.
