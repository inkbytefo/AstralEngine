# Astral Engine - Mimari Tasarım Belgesi

**Sürüm:** 0.2.0-alpha  
**Tarih:** 19 Eylül 2025  
**Yazar:** Astral Engine Geliştirme Ekibi

## 1. Giriş ve Felsefe

Astral Engine, modern C++20 standartları üzerine inşa edilmiş, yüksek performanslı, modüler ve platformdan bağımsız bir 3D oyun motorudur. Temel felsefesi, karmaşık sistemleri yönetilebilir, bakımı kolay ve genişletilebilir modüller halinde soyutlamaktır.

-   **Modülerlik:** Motor, her biri belirli bir sorumluluğa sahip olan **Alt Sistemler (Subsystems)** etrafında tasarlanmıştır. Bu yapı, yeni özelliklerin (örneğin fizik, yapay zeka) mevcut sisteme entegrasyonunu kolaylaştırır.
-   **Veri Odaklı Tasarım (Data-Oriented Design):** Motorun kalbinde, oyun dünyasının durumunu verimli bir şekilde yöneten bir **Entity Component System (ECS)** bulunur. Mantık, veriler üzerinde çalışan sistemler tarafından işlenir, bu da önbellek dostu ve performanslı operasyonlar sağlar.
-   **Soyutlama ve Gevşek Bağlılık (Abstraction & Loose Coupling):** Alt sistemler, iç uygulama detaylarını (örneğin, Vulkan API'si, SDL3 pencere yönetimi) gizler ve birbirleriyle temiz, iyi tanımlanmış arayüzler üzerinden iletişim kurar. Bu, bir alt sistemdeki değişikliğin diğerlerini etkileme riskini en aza indirir.
-   **Açık Genişletilebilirlik:** Motor, yeni render teknikleri, varlık türleri veya oyun mantığı sistemleri eklemeye olanak tanıyan esnek bir yapı sunar.

## 2. Teknoloji Stack

Astral Engine, endüstri standardı ve modern teknolojilerden yararlanır:

| Bileşen                  | Teknoloji                                       | Amaç                                                    |
| ------------------------- | ----------------------------------------------- | ------------------------------------------------------- |
| **Dil**                   | C++20                                           | Modern, performanslı ve güvenli kod yazımı.             |
| **Derleme Sistemi**       | CMake                                           | Platformlar arası tutarlı ve esnek proje yönetimi.      |
| **Grafik API'si**         | Vulkan                                          | Düşük seviyeli, yüksek performanslı ve modern grafikler. |
| **Pencere ve Girdi**      | SDL3                                            | Platformdan bağımsız pencere, girdi ve olay yönetimi.   |
| **Shader Dili**           | Slang                                           | Modern, esnek ve SPIR-V'ye derlenebilen shader dili.    |
| **3D Matematik**          | GLM (OpenGL Mathematics)                        | Vektör ve matris operasyonları için endüstri standardı.  |
| **Model Yükleme**         | Assimp (Open Asset Import Library)              | Çeşitli 3D model formatlarını yükleme ve işleme.        |
| **Veri Serileştirme**     | nlohmann/json                                   | Yapılandırılmış veriler ve ayarlar için JSON desteği.   |
| **Grafik Bellek Yönetimi**| VMA (Vulkan Memory Allocator)                   | Verimli ve güvenli Vulkan bellek tahsisi.               |

## 3. Çekirdek Mimari

### 3.1. Engine Sınıfı ve Subsystem Yönetimi

Motorun mimari omurgası, [`Engine`](Source/Core/Engine.h:23) sınıfı ve [`ISubsystem`](Source/Core/ISubsystem.h:14) arayüzü tarafından oluşturulur.

-   **[`ISubsystem`](Source/Core/ISubsystem.h:14) Arayüzü:** Tüm alt sistemlerin uyması gereken temel sözleşmedir. Her alt sistemin [`OnInitialize`](Source/Core/ISubsystem.h:19), [`OnUpdate`](Source/Core/ISubsystem.h:22) ve [`OnShutdown`](Source/Core/ISubsystem.h:25) gibi yaşam döngüsü metodlarını içermesini zorunlu kılar.
-   **[`Engine`](Source/Core/Engine.h:23) Sınıfı:** Motorun ana orkestratörüdür. [`RegisterSubsystem<T>()`](Source/Core/Engine.h:68-74) metodu ile alt sistemleri kaydeder, başlatır, ana döngü boyunca günceller ve motor kapatıldığında güvenli bir şekilde sonlandırır. Ayrıca alt sistemler arasında kontrollü bir erişim noktası görevi görür ve [`GetSubsystem<T>()`](Source/Core/Engine.h:76-83) metodu ile tip-güvenli erişim sağlar.

### 3.2. Kritik Başlatma Sırası

Alt sistemlerin kayıt sırası kritik öneme sahiptir ve [`main.cpp`](Source/main.cpp:100-103) dosyasında belirlenmiştir:

1. **[`PlatformSubsystem`](Source/Subsystems/Platform/PlatformSubsystem.h:18)** - Pencere oluşturma ve girdi yönetimi
2. **[`AssetSubsystem`](Source/Subsystems/Asset/AssetSubsystem.h:15)** - Varlık yükleme ve yönetimi  
3. **[`ECSSubsystem`](Source/Subsystems/ECS/ECSSubsystem.h:24)** - Entity Component System
4. **[`RenderSubsystem`](Source/Subsystems/Renderer/RenderSubsystem.h:39)** - 3D grafik renderleme

Bu sıra, bağımlılıklar tarafından zorunlu kılınır - [`RenderSubsystem`](Source/Subsystems/Renderer/RenderSubsystem.h:39), [`PlatformSubsystem`](Source/Subsystems/Platform/PlatformSubsystem.h:18)'ın pencere tanıtıcısına ihtiyaç duyar.

### 3.3. Temel Sistemler

#### 3.3.1. Core Sistemleri

**[Logger](Source/Core/Logger.h:22):** Merkezi loglama sistemi
- Farklı log seviyelerini destekler: Trace, Debug, Info, Warning, Error, Critical
- Hem konsol hem dosya çıkışı sağlar
- [`InitializeFileLogging()`](Source/Core/Logger.h:56) ile otomatik olarak exe dizininde log dosyası oluşturur
- Template tabanlı [`Info()`](Source/Core/Logger.h:88-94), [`Error()`](Source/Core/Logger.h:106-112) gibi metotlarla tip-güvenli loglama

**[MemoryManager](Source/Core/MemoryManager.h:15):** Bellek yönetim sistemi
- Singleton pattern ile merkezi bellek yönetimi
- [`AllocateFrameMemory()`](Source/Core/MemoryManager.h:25) ile frame bazlı geçici bellek ayırma
- [`ResetFrameMemory()`](Source/Core/MemoryManager.h:26) ile frame sonunda belleği temizleme
- Gelecekte pool allocator ve stack allocator optimizasyonları için altyapı sağlar

**[ThreadPool](Source/Core/ThreadPool.h:23):** Çoklu iş parçacığı yönetimi
- [`Submit()`](Source/Core/ThreadPool.h:67) metoduyla asenkron görev çalıştırma
- Asset yükleme gibi uzun süren işlemler için ana thread'i bloklamaz
- Template tabanlı [`Submit()`](Source/Core/ThreadPool.h:67) metoduyla tip-güvenli async işlemler

**[EventManager](Source/Events/EventManager.h:19):** Olay tabanlı iletişim sistemi
- Observer pattern implementasyonu
- Thread-safe tasarım: [`m_handlersMutex`](Source/Events/EventManager.h:73) ve [`m_queueMutex`](Source/Events/EventManager.h:74) dual-mutex sistemi
- [`ProcessEvents()`](Source/Events/EventManager.h:42) metodu ile ana thread'de olay işleme
- Template tabanlı [`PublishEvent<T>()`](Source/Events/EventManager.h:98-103) ve [`Subscribe<T>()`](Source/Events/EventManager.h:85-95) metotları

## 4. Alt Sistem Detayları

### 4.1. PlatformSubsystem

**Sorumluluk:** İşletim sistemi ile ilgili tüm işlemleri soyutlar.

**Bileşenler:**
-   **[`Window`](Source/Subsystems/Platform/Window.h):** SDL3 kullanarak pencere oluşturma, boyutlandırma ve yönetme
-   **[`InputManager`](Source/Subsystems/Platform/InputManager.h):** Klavye, fare ve diğer girdi cihazlarından gelen verileri işleme

**Özellikler:**
- [`IsWindowOpen()`](Source/Subsystems/Platform/PlatformSubsystem.h:34) ve [`CloseWindow()`](Source/Subsystems/Platform/PlatformSubsystem.h:35) metotlarıyla pencere kontrolü
- Olay tabanlı girdi sistemi ile düşük gecikme süresi
- Platformdan bağımsız tek tip API

### 4.2. AssetSubsystem

**Sorumluluk:** Oyun varlıklarının (modeller, dokular, materyaller) diskten yüklenmesini, önbelleğe alınmasını ve yönetilmesini sağlar.

**Bileşenler:**
-   **[`AssetManager`](Source/Subsystems/Asset/AssetManager.h):** Varlıkların yaşam döngüsünü yönetir ve [`AssetHandle`](Source/Subsystems/Asset/AssetHandle.h)lar aracılığıyla güvenli erişim sağlar
-   **[`AssetRegistry`](Source/Subsystems/Asset/AssetRegistry.h):** Varlıkların meta verilerini ve disk üzerindeki konumlarını takip eder

**Özellikler:**
- [`SetAssetDirectory()`](Source/Subsystems/Asset/AssetSubsystem.h:30) ile konfigüre edilebilir varlık dizini
- Thread-safe asset yükleme ve önbellekleme
- [`AssetHandle`](Source/Subsystems/Asset/AssetHandle.h) sistemi ile tip-güvenli asset erişimi

### 4.3. ECSSubsystem (Entity Component System)

**Sorumluluk:** Oyun dünyasının durumunu veri odaklı bir yaklaşımla yönetir.

**Bileşenler:**
-   **[`Entity`](Source/Subsystems/ECS/ECSSubsystem.h:58):** Oyun dünyasındaki bir nesneyi temsil eden basit bir ID (uint32_t)
-   **[`Component`](Source/ECS/Components.h):** Bir entity'ye eklenen veri bloğu (örn: [`TransformComponent`](Source/ECS/Components.h), [`RenderComponent`](Source/ECS/Components.h))
-   **[`System`](Source/Subsystems/ECS/ECSSubsystem.h:24):** Belirli bileşenlere sahip entity'ler üzerinde çalışan mantık

**Özellikler:**
- Template tabanlı [`AddComponent<T>()`](Source/Subsystems/ECS/ECSSubsystem.h:118-142) ve [`GetComponent<T>()`](Source/Subsystems/ECS/ECSSubsystem.h:145-157) metotları
- [`QueryEntities<Components...>()`](Source/Subsystems/ECS/ECSSubsystem.h:202-227) ile çoklu bileşen filtresi
- [`RenderPacket`](Source/Subsystems/ECS/ECSSubsystem.h:58) yapısı ile render sistemine veri sağlama
- Veri odaklı tasarım ile yüksek performanslı component erişimi

### 4.4. RenderSubsystem

**Sorumluluk:** Tüm çizim (rendering) işlemlerini yönetir ve grafik API'sini (Vulkan) soyutlar.

**Bileşenler:**
-   **[`GraphicsDevice`](Source/Subsystems/Renderer/GraphicsDevice.h):** Vulkan instance, device ve kuyruklarını yönetir
-   **[`VulkanRenderer`](Source/Subsystems/Renderer/VulkanRenderer.h:27):** Deferred rendering pipeline implementasyonu
-   **[`MaterialManager`](Source/Subsystems/Renderer/Material/MaterialManager.h):** Gelişmiş malzeme ve shader yönetim sistemi
-   **[`VulkanMeshManager`](Source/Subsystems/Renderer/VulkanMeshManager.h):** Model verilerini (vertex/index buffer) GPU'ya yükler ve yönetir
-   **[`VulkanTextureManager`](Source/Subsystems/Renderer/VulkanTextureManager.h):** Doku (texture) verilerini GPU'ya yükler ve yönetir
-   **[`PostProcessingSubsystem`](Source/Subsystems/Renderer/PostProcessingSubsystem.h):** Bloom, tonemapping gibi post-processing efektleri

**Render Pipeline:**
1. **Shadow Pass:** Derinlik haritası oluşturma
2. **G-Buffer Pass:** Geometri verilerini toplama (PBR deferred rendering)
3. **Lighting Pass:** Işık hesaplamaları ve gölgelendirme
4. **Post-Processing:** Bloom, tonemapping gibi efektler

## 5. Vulkan Renderer Mimarisi

### 5.1. Başlatma Sırası

VulkanRenderer karmaşık bir başlatma sırası gerektirir, AGENTS.md'de belirtildiği gibi:

1. **Shader Yükleme:** [`Assets/Shaders/`](Assets/Shaders/) dizininden `.spv` dosyalarının yüklenmesi
2. **Descriptor Set Layout Oluşturma:** Vulkan kaynak tanımları
3. **Pipeline Başlatma:** Grafik pipeline'larının oluşturulması
4. **Vertex Buffer Başlatma:** Geometri verileri için buffer'lar
5. **Descriptor Pool Oluşturma:** Kaynak ayırma havuzu
6. **Uniform Buffer Oluşturma:** Uniform veriler için buffer'lar
7. **Descriptor Set Oluşturma:** Kaynak bağlamaları
8. **Descriptor Set Güncelleme:** Kaynak verilerinin atanması

### 5.2. Pipeline Cache Sistemi

[`VulkanRenderer::GetOrCreatePipeline()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:183-247) metodu ile malzeme bazlı pipeline önbellekleme:
- Shader handle'larına göre hash oluşturma
- Pipeline cache ile performans optimizasyonu
- Malzeme değişikliklerinde otomatik pipeline yeniden oluşturma

### 5.3. Instance Rendering

[`VulkanRenderer::RecordGBufferCommands()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:315-403) ile çoklu örnek renderleme:
- Frame bazlı instance buffer yönetimi
- [`INSTANCE_BUFFER_SIZE`](Source/Subsystems/Renderer/VulkanRenderer.h:102) = 1MB limit
- [`ResetInstanceBuffer()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:658-663) ve [`EndFrame()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:666-671) ile buffer yönetimi

## 6. Shader Sistemi ve Materyal Yönetimi

### 6.1. Otomatik Shader Derleme

CMake build sistemi ile entegre shader derleme:
- [`compile_vulkan_shaders()`](CMakeLists.txt:760-793) fonksiyonu ile otomatik derleme
- Slang shader'ları otomatik olarak SPIR-V formatına dönüştürülür
- Derlenmiş `.spv` dosyaları çıktı dizinine kopyalanır
- Hot reload desteği ile development sırasında dinamik güncelleme

### 6.2. Materyal Sistemi

PBR (Physically Based Rendering) tabanlı materyal sistemi:
- Albedo, normal, metalness, roughness, AO texture desteği
- [`Material`](Source/Subsystems/Renderer/Material/Material.h) sınıfı ile uniform yönetimi
- Descriptor set layout ile Vulkan kaynak yönetimi
- Pipeline cache ile performans optimizasyonu

## 7. Build Sistemi ve Bağımlılıklar

### 7.1. Modern CMake Konfigürasyonu

AstralEngine, modern CMake en iyi pratiklerine uygun olarak yeniden yapılandırılmıştır. Derleme sistemi şu modüllerden oluşur:

- **cmake/ProjectOptions.cmake**: Tüm kullanıcı tarafından yapılandırılabilir seçenekleri içerir
  - Temel derleme seçenekleri (shared/static library, examples, tests, tools)
  - Alt sistem seçenekleri (SDL3, Vulkan, ImGui, Jolt Physics)
  - Geliştirme seçenekleri (validation, debug markers, shader hot reload)

- **cmake/ProjectConfiguration.cmake**: Merkezi bir INTERFACE target (`astral_common_settings`) tanımlar
  - Derleyiciye özgü bayraklar (MSVC, GCC/Clang)
  - Platforma özgü tanımlamalar (Windows, Linux, macOS)
  - Ortak derleyici tanımlamaları ve optimizasyonlar
  - Link Time Optimization (LTO) ve uyarı yönetimi

- **cmake/Dependencies.cmake**: Tüm bağımlılıkları yönetir
  - FetchContent veya find_package kullanarak bağımlılık yönetimi
  - Kategorize edilmiş bağımlılık yapısı (Core, Math, JSON, 3D Model, Vulkan, UI, Physics)
  - Standartlaştırılmış yardımcı fonksiyonlar

- **CMakeLists.txt**: Projenin üst düzey orkestratörü
  - Modüler yapı ile temiz ve okunabilir ana dosya
  - Hedef odaklı tasarım ile bağımlılık yönetimi
  - Otomatik asset kopyalama ve shader yönetimi
  - Gelişmiş kurulum ve paketleme desteği

### 7.2. Build Seçenekleri ve Yapılandırma

Proje, modern CMake'in hedef odaklı yaklaşımını kullanır:

```bash
# Temel yapılandırma
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Tüm seçenekleri görüntüleme
cmake -B build -LAH

# Örnek yapılandırma
cmake -B build \
  -DASTRAL_BUILD_SHARED=ON \
  -DASTRAL_BUILD_EXAMPLES=ON \
  -DASTRAL_USE_VULKAN=ON \
  -DASTRAL_USE_IMGUI=ON \
  -DASTRAL_ENABLE_LTO=ON \
  -DCMAKE_BUILD_TYPE=Release
```

### 7.3. Bağımlılık Yönetimi

Bağımlılıklar, modern CMake pratiklerine göre yönetilir:

- **Otomatik İndirme**: FetchContent ile bağımlılıklar otomatik olarak indirilir
- **vcpkg Entegrasyonu**: Varsa vcpkg kullanılır
- **Interface Targets**: Bağımlılıklar INTERFACE target olarak tanımlanır
- **Koşullu Bağlama**: İsteğe bağlı bağımlılıklar koşullu olarak bağlanır

### 7.4. Platform Desteği ve Optimizasyonlar

- **Çapraz Platform**: Windows, Linux ve macOS desteği
- **Derleyici Optimizasyonları**: MSVC için AVX2, GCC/Clang için native optimizasyonlar
- **Debug/Release Ayırımı**: Farklı build türleri için özel ayarlar
- **Validation Desteği**: Debug build'lerde otomatik validation katmanları

## 8. Veri Akışı: Bir Frame'in Anatomisi

Bir oyun döngüsü (frame) boyunca verinin sistemler arasındaki akışı:

1.  **Girdi ve Olaylar:** [`PlatformSubsystem`](Source/Subsystems/Platform/PlatformSubsystem.h:18), kullanıcı girdilerini ve pencere olaylarını işler
2.  **Oyun Mantığı:** [`ECSSubsystem`](Source/Subsystems/ECS/ECSSubsystem.h:24), oyun mantığını günceller (örneğin, bir karakterin [`TransformComponent`](Source/ECS/Components.h)'ini değiştirir)
3.  **Render Verisi Toplama:** [`ECSSubsystem`](Source/Subsystems/ECS/ECSSubsystem.h:24), [`RenderComponent`](Source/ECS/Components.h) ve [`TransformComponent`](Source/ECS/Components.h) gibi bileşenlere sahip tüm entity'leri sorgular. Bu verilerden bir [`RenderPacket`](Source/Subsystems/ECS/ECSSubsystem.h:58) (çizim komut listesi) oluşturur
4.  **Render Komutları:** [`RenderPacket`](Source/Subsystems/ECS/ECSSubsystem.h:58), [`RenderSubsystem`](Source/Subsystems/Renderer/RenderSubsystem.h:39)'e gönderilir
5.  **Çizim:** [`RenderSubsystem`](Source/Subsystems/Renderer/RenderSubsystem.h:39), bu paketi işler. [`AssetSubsystem`](Source/Subsystems/Asset/AssetSubsystem.h:15)'den aldığı GPU'daki kaynakları (mesh, texture) kullanarak Vulkan çizim komutlarını oluşturur ve yürütür
6.  **Sunum:** [`GraphicsDevice`](Source/Subsystems/Renderer/GraphicsDevice.h), tamamlanan görüntüyü [`Window`](Source/Subsystems/Platform/Window.h)'a sunar

## 9. Hata Yönetimi ve Debug Desteği

### 9.1. Vulkan Hata Yönetimi

- **Validation Layers:** Debug build'lerde otomatik etkinleştirme via [`ASTRAL_VULKAN_VALIDATION`](CMakeLists.txt:58)
- **Açıklayıcı Hata Mesajları:** [`VulkanUtils`](Source/Subsystems/Renderer/VulkanUtils.h) ile Vulkan hata kodlarının açıklamaları
- **RAII Pattern:** Tüm Vulkan kaynakları için explicit [`Shutdown()`](Source/Subsystems/Renderer/VulkanRenderer.cpp:256-313) metotları

### 9.2. Loglama ve İzleme

- **Çoklu Log Seviyeleri:** Trace'den Critical'e kadar detaylı loglama
- **Kategori Bazlı Loglama:** Sistem bazlı log kategorileri (örn: "VulkanRenderer", "AssetManager")
- **Dosya Loglaması:** Otomatik log dosyası oluşturma ve yönetimi
- **Debug Logging:** [`ASTRAL_LOG_LEVEL=DEBUG`](AGENTS.md) ortam değişkeni ile detaylı debug çıktısı

## 10. Planlanan Özellikler ve Gelecek Geliştirmeler

### 10.1. 🚧 Planlama Aşamasında Olan Sistemler

**Fizik Sistemi (PhysicsSubsystem):**
- **Teknoloji:** [Jolt Physics](JOLT_PHYSICS_INTEGRATION_GUIDE.md)
- **Durum:** Planlama aşamasında - implemente edilmedi
- **Amaç:** Gerçek zamanlı rigid body simülasyonu, çarpışma tespiti ve fizik tabanlı oyun mekanikleri

**Audio Sistemi (AudioSubsystem):**
- **Durum:** Planlama aşamasında - implemente edilmedi  
- **Amaç:** 3D spatial audio, efektler ve müzik yönetimi

**Scripting Desteği:**
- **Durum:** Planlama aşamasında - implemente edilmedi
- **Amaç:** Lua veya Python tabanlı scripting desteği ile oyun mantığının dışarıdan yüklenmesi

### 10.2. 🎯 Geliştirme Roadmap

**Kısa Vadeli (v0.2.0 - v0.3.0):**
- Jolt Physics entegrasyonu
- Temel audio sistemi
- ImGui entegrasyonu (debug araçları için)

**Orta Vadeli (v0.4.0 - v0.5.0):**
- Scripting desteği
- Gelişmiş post-processing efektleri
- Level/Scene yönetim sistemi

**Uzun Vadeli (v1.0.0):**
- Editör ara yüzü
- Gelişmiş fizik özellikleri (soft body, fluid simülasyonu)
- Ağ/çoklu oyuncu desteği

## 11. Örnek Kullanım: Sandbox Uygulaması

Proje, [`AstralEngine`](Source/Core/Engine.h:23)'i bir kütüphane olarak nasıl kullanacağını gösteren bir [`Sandbox`](Source/main.cpp:16) yürütülebilir dosyası içerir:

```cpp
// Motor başlatma
Engine engine;
SandboxApp sandbox;

// Alt sistemleri kaydet (kritik sıraya dikkat!)
engine.RegisterSubsystem<PlatformSubsystem>();
engine.RegisterSubsystem<AssetSubsystem>();
engine.RegisterSubsystem<ECSSubsystem>();
engine.RegisterSubsystem<RenderSubsystem>();

// Motoru çalıştır
engine.Run(&sandbox);
```

Bu uygulama, motorun yeteneklerini test etmek ve yeni özellikleri denemek için birincil geliştirme ve test ortamıdır. ECS sistemi ile entity oluşturma, asset yükleme ve render sistemine veri sağlama örneklerini içerir.

## 12. Sonuç

Astral Engine, modern C++20 teknolojileri üzerine inşa edilmiş, modüler ve genişletilebilir bir 3D oyun motoru sunar. Vulkan tabanlı gelişmiş grafik sistemi, veri odaklı ECS mimarisi ve güçlü alt sistem yapısı ile hem öğrenme hem de profesyonel kullanım için uygun bir platform sağlar. Gelecekteki fizik, audio ve scripting entegrasyonları ile tam özellikli bir oyun motoru olma yolunda ilerlemektedir.