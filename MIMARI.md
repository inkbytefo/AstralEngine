# Astral Engine - Mimari Tasarım Belgesi

**Sürüm:** 0.1.0-alpha  
**Tarih:** 17 Eylül 2025  
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

## 3. Çekirdek Mimari: `Engine` ve `ISubsystem`

Motorun mimari omurgası, `Engine` sınıfı ve `ISubsystem` arayüzü tarafından oluşturulur.

-   **`ISubsystem` Arayüzü:** Tüm alt sistemlerin uyması gereken temel sözleşmedir. Her alt sistemin `OnInitialize`, `OnUpdate` ve `OnShutdown` gibi yaşam döngüsü metodlarını içermesini zorunlu kılar.
-   **`Engine` Sınıfı:** Motorun ana orkestratörüdür. Alt sistemleri kaydeder, başlatır, ana döngü boyunca günceller ve motor kapatıldığında güvenli bir şekilde sonlandırır. Ayrıca alt sistemler arasında kontrollü bir erişim noktası görevi görür.

## 4. Mevcut Alt Sistemler

Astral Engine'in mevcut kararlı sürümü aşağıdaki alt sistemleri içermektedir:

### 4.1. `PlatformSubsystem`
-   **Sorumluluk:** İşletim sistemi ile ilgili tüm işlemleri soyutlar.
-   **Bileşenler:**
    -   `Window`: SDL3 kullanarak pencere oluşturma, boyutlandırma ve yönetme.
    -   `InputManager`: Klavye, fare ve diğer girdi cihazlarından gelen verileri işleme.
-   **İşleyiş:** İşletim sistemi olaylarını yakalar ve motorun `EventManager`'ına iletir.

### 4.2. `RenderSubsystem`
-   **Sorumluluk:** Tüm çizim (rendering) işlemlerini yönetir ve grafik API'sini (Vulkan) soyutlar.
-   **Bileşenler:**
    -   `GraphicsDevice`: Vulkan instance, device ve kuyruklarını yönetir.
    -   `VulkanSwapchain`: Görüntüyü ekrana sunmak için swapchain'i yönetir.
    -   `VulkanMeshManager`: Model verilerini (vertex/index buffer) GPU'ya yükler ve yönetir.
    -   `VulkanTextureManager`: Doku (texture) verilerini GPU'ya yükler ve yönetir.
    -   `ShaderManager`: Slang ile derlenmiş SPIR-V shader'larını yükler ve Vulkan pipeline'larını oluşturur.
    -   `MaterialManager`: Malzeme verilerini (PBR parametreleri, doku haritaları) yönetir.
-   **İşleyiş:** `ECSSubsystem`'den aldığı `RenderPacket` verisini kullanarak sahneyi çizer.

### 4.3. `ECSSubsystem`
-   **Sorumluluk:** Oyun dünyasının durumunu veri odaklı bir yaklaşımla yönetir.
-   **Bileşenler:**
    -   **Entity:** Oyun dünyasındaki bir nesneyi temsil eden basit bir ID.
    -   **Component:** Bir entity'ye eklenen veri bloğu (örn: `TransformComponent`, `RenderComponent`).
    -   **System:** Belirli bileşenlere sahip entity'ler üzerinde çalışan mantık (henüz tam olarak uygulanmadı, mantık `ECSSubsystem` içinde).
-   **İşleyiş:** Her frame'de, render edilebilir entity'leri sorgular ve bu entity'lerin verilerinden (`Transform`, `Model`, `Material`) bir `RenderPacket` oluşturarak `RenderSubsystem`'e sunar.

### 4.4. `AssetSubsystem`
-   **Sorumluluk:** Oyun varlıklarının (modeller, dokular, materyaller) diskten yüklenmesini, önbelleğe alınmasını ve yönetilmesini sağlar.
-   **Bileşenler:**
    -   `AssetManager`: Varlıkların yaşam döngüsünü yönetir ve `AssetHandle`'lar aracılığıyla güvenli erişim sağlar.
    -   `AssetRegistry`: Varlıkların meta verilerini ve disk üzerindeki konumlarını takip eder.
-   **İşleyiş:** Diğer alt sistemler, ihtiyaç duydukları varlıkları bu sistem üzerinden talep eder. `RenderSubsystem`, çizim için gerekli olan model ve doku verilerini bu sistem aracılığıyla alır.

## 5. Veri Akışı: Bir Frame'in Anatomisi

Bir oyun döngüsü (frame) boyunca verinin sistemler arasındaki akışı şu şekildedir:

1.  **Girdi ve Olaylar:** `PlatformSubsystem`, kullanıcı girdilerini ve pencere olaylarını işler.
2.  **Oyun Mantığı:** `ECSSubsystem`, oyun mantığını günceller (örneğin, bir karakterin `TransformComponent`'ini değiştirir).
3.  **Render Verisi Toplama:** `ECSSubsystem`, `RenderComponent` ve `TransformComponent` gibi bileşenlere sahip tüm entity'leri sorgular. Bu verilerden bir `RenderPacket` (çizim komut listesi) oluşturur.
4.  **Render Komutları:** `RenderPacket`, `RenderSubsystem`'e gönderilir.
5.  **Çizim:** `RenderSubsystem`, bu paketi işler. `AssetSubsystem`'den aldığı GPU'daki kaynakları (mesh, texture) kullanarak Vulkan çizim komutlarını oluşturur ve yürütür.
6.  **Sunum:** `GraphicsDevice`, tamamlanan görüntüyü `Window`'a sunar.

## 6. Varlık (Asset) Pipeline'ı

-   **Shader'lar:** `Assets/Shaders/` altındaki `.slang` dosyaları, CMake derleme süreci sırasında `slangc` derleyicisi kullanılarak platformdan bağımsız SPIR-V formatına (`.spv`) dönüştürülür ve çalışma zamanında `ShaderManager` tarafından yüklenir.
-   **Modeller ve Dokular:** `Assets/` dizinindeki 3D modeller (örn: `.obj`, `.gltf`) ve dokular (örn: `.png`, `.jpg`), `AssetSubsystem` tarafından çalışma zamanında `Assimp` ve `stb_image` gibi kütüphaneler aracılığıyla yüklenir.

## 7. Planlanan Özellikler ve Yol Haritası

Astral Engine'in geliştirme yol haritası, aşağıdaki temel özelliklerin entegrasyonunu içermektedir:

-   **Fizik Motoru (`PhysicsSubsystem`):**
    -   **Teknoloji:** **Jolt Physics**
    -   **Amaç:** Gerçek zamanlı rigid body simülasyonu, çarpışma tespiti ve fizik tabanlı oyun mekanikleri için bir fizik alt sistemi entegre etmek. Bu, oyuna dinamizm ve etkileşim katacaktır.

-   **Geliştirici Arayüzü (`UISubsystem`):**
    -   **Teknoloji:** **Dear ImGui**
    -   **Amaç:** Hata ayıklama (debugging), performans profili oluşturma (profiling) ve oyun içi düzenleyici (editor) araçları için bir kullanıcı arayüzü alt sistemi eklemek. Bu, geliştirme sürecini önemli ölçüde hızlandıracaktır.

## 8. Örnek Uygulama: `Sandbox`

Proje, `AstralEngine`'i bir kütüphane olarak nasıl kullanacağını gösteren bir `Sandbox` yürütülebilir dosyası içerir. Bu uygulama, motorun yeteneklerini test etmek ve yeni özellikleri denemek için birincil geliştirme ve test ortamıdır.