# AstralEngine Kapsamlı Mimari ve Kod Analiz Raporu

**Tarih:** 06 Eylül 2026  
**Durum:** Tamamlandı / Doğrulandı  
**Kapsam:** Motor Mimarisi, SDF Pipeline, ECS Veri Modeli, Render Geçişleri, Dual Representation Drift Analizi

---

## 1. Yönetici Özeti

AstralEngine; analitik SDF (Signed Distance Fields), CSG işlemleri, Vulkan 1.3/1.4 compute shader'ları, veri odaklı (Data-Oriented) SparseSet ECS ve temporal filtreleme tekniklerini merkeze alan çağdaş bir araştırma/oyun motorudur.

Kod tabanında yapılan derinlemesine inceleme sonucunda:
- ECS, generational handle güvenliği, little-endian binary serileştirme (v3), frame scheduling ve analitik SDF matematik çekirdeği (`SDFKernel.inl`) **yüksek kalitede** tasarlanmıştır.
- Ancak motorun kalbinde, **eski prototip veri yapısı (`SDFEditGPU`) ile yeni nesil snapshot yapısı (`SDFPrimitiveRecord`) arasında ciddi bir mimari kırılma (Dual Representation Drift)** bulunmaktadır.
- Bu ikilik sebebiyle, temporal kimlik (`surfaceId`) çalışma zamanında sahte dizinlere indirgenmekte, dinamik nesnelerin hareket geçmişi yok sayılmakta ve `BrickGrid` dirty tracking sistemi çalışma zamanında tamamen devre dışı kalıp her kare tüm ızgarayı sıfırdan hesaplamaktadır (`FullRebuild`).
- `SDFRenderer` sınıfı 1.787 satır ve ~76.5 KB boyutuyla monolitik hale gelmiştir; `QualitySettings` shader push constant'larına hardcoded yazılmış, `IUpscaler` arayüzü ise uygulamasız bırakılmıştır.

---

## 2. Derinlemesine Teknik İnceleme ve Doğrulamalar

### 2.1. Dual Representation Drift (En Kritik Sorun)

Projede iki ayrı GPU primitif yapısı mevcuttur:

1. **`SDFEditGPU`** ([`include/Astral/Renderer/SDFEdit.hpp`](include/Astral/Renderer/SDFEdit.hpp)):
   - Konum, kuaterniyon rotasyon, ölçek, materyal ve önceki kare dönüşüm verilerini (`prevPosX/Y/Z`, `prevRotation`, `prevScale`) tutar (128 Bayt).
2. **`SDFPrimitiveRecord`** ([`include/Astral/Geometry/SDFSceneSnapshot.hpp`](include/Astral/Geometry/SDFSceneSnapshot.hpp) & [`shaders/SDFScene.glsl`](shaders/SDFScene.glsl)):
   - Ters dönüşüm matrisi (`invTransform`, 64B), şekil parametreleri (`dimensions`, 16B), materyal/pürüzlülük (`albedoRoughness`, 16B), metalik/blend/ölçek katsayısı (`metallicParams`, 16B), primitif tipi, operasyon, CSG sırası ve deterministik yüzey kimliği (`surfaceId`) tutar (128 Bayt).

#### Ortaya Çıkarılan 5 Temel Hata ve Tutarsızlık:

1. **Çalışma Zamanı Döngüsü Eski Yolda Kalmış Durumda:**
   - [`src/Core/Application.cpp:L274-L280`](src/Core/Application.cpp) içinde `m_RenderExtractionSubsystem->GetLastExtractedEdits()` çağrılarak `std::vector<SDFEditGPU>` alınmakta ve `SDFRenderer::UpdateEdits`'e iletilmektedir.
   - Modern `SDFSceneSnapshot` sınıfı sadece birim testlerde ve `SDFWorldQuery` içinde kullanılmaktadır.

2. **Temporal Identity (`surfaceId`) Çalışma Zamanında Sahtedir:**
   - [`src/Renderer/SDFRenderer.cpp:L1129-L1130`](src/Renderer/SDFRenderer.cpp) içinde `UpdateEdits(vector<SDFEditGPU>)` overload'u çağrıldığında:
     ```cpp
     rec.csgOrder = static_cast<uint32_t>(i);
     rec.surfaceId = static_cast<uint32_t>(i + 1);
     ```
   - Entity ID, Generation ve Instance bilgilerini FNV1a ile hash'leyen deterministik `SDFSurfaceKey.Hash()` bypass edilerek nesnelere rastgele `i + 1` kimliği atanmaktadır. Sahneden bir nesne silindiğinde veya sıralama değiştiğinde arkasındaki tüm nesnelerin yüzey kimliği kayar; TAA geçmiş reddi ve nesne seçimi (picking) çöker.

3. **Geometrik Ölçekleme ve Ters Matris Tutarsızlığı:**
   - `SDFSceneSnapshot::Extract` küre, kapsül ve silindirler için rijit rotasyon/öteleme matrisi kurup ölçeği `dimensions` ve `conservativeScale`'e aktarırken;
   - `SDFRenderer::UpdateEdits(vector<SDFEditGPU>)` ölçeği doğrudan `rec.dimensions = glm::vec4(e.scale, 0.0f)` yapmakta ve `conservativeScale = 1.0f` olarak sabitlemektedir. Bu nedenle sahne `Snapshot` ile verilirse farklı, `SDFEditGPU` ile verilirse farklı görünmektedir.

4. **Kayıp Dinamik Hareket Geçmişi (Ghost Motion History):**
   - `RenderExtractionSystem.cpp:L164-L171` satırlarında her kare CPU'da `renderedPosition`, `renderedRotation`, `renderedScale` hesaplanarak `SDFEditGPU`'ya yazılır.
   - Fakat shader'a giden `SDFPrimitiveRecord` içinde bu alanlar yoktur.
   - [`shaders/SDFGBuffer.glsl:L209`](shaders/SDFGBuffer.glsl) satırında `vec3 prevHitPos = hitPos;` atanarak dinamik nesne hareketi sıfırlanır, yalnızca kamera hareketi reproject edilir.

5. **`BrickGrid` Dirty Tracking Çalışma Zamanında Ölü:**
   - [`src/Renderer/BrickGrid.cpp:L99-L194`](src/Renderer/BrickGrid.cpp) içindeki AABB dirty tracking ve early-out sistemi yalnızca `Build(std::span<const SDFEditGPU>)` fonksiyonunda mevcuttur.
   - Ancak `SDFRenderer.cpp:L1154` `Build(records)` (`SDFPrimitiveRecord`) çağırmaktadır.
   - [`BrickGrid.cpp:L276-L278`](src/Renderer/BrickGrid.cpp):
     ```cpp
     void BrickGrid::Build(std::span<const SDFPrimitiveRecord> records) {
         FullRebuild(records); // Her kare 16.384 hücre CPU'da baştan hesaplanır!
     }
     ```
   - Akıllı kısmi güncelleme sistemi çalışma zamanında hiç devreye girmemektedir.

6. **Tuzak Fonksiyon (`ExtractAndUploadRenderData`):**
   - [`src/Core/RenderExtractionSystem.cpp:L219`](src/Core/RenderExtractionSystem.cpp) `memcpy(mappedGpuBuffer, uploadBuffer.data(), outEditCount * sizeof(SDFEditGPU))` çağrısı yapmaktadır. Shader bu belleği `SDFPrimitiveRecord` olarak okuduğu için bu fonksiyon çağrılırsa görüntü tamamen bozulacaktır.

---

### 2.2. Monolitik `SDFRenderer` (~76.5 KB, 1.787 Satır)
- [`src/Renderer/SDFRenderer.cpp`](src/Renderer/SDFRenderer.cpp) G-Buffer, Deferred PBR, IBL, TAA resolve, Picking buffer, BrickGrid buffer, Camera UBO, Light buffer ve tüm senkronizasyon bariyerlerini tek başına yönetmektedir.
- Her render geçişinin (pass) descriptor pool tahsisleri, layout tanımları ve pipeline state'leri iç içe geçmiştir.

### 2.3. CSG, Smooth Blend ve Temporal History
- [`include/Astral/Renderer/SDFTemporalHistory.hpp`](include/Astral/Renderer/SDFTemporalHistory.hpp) CPU tarafında test edilen zengin güven formüllerine (`normalConfidence`, `attributionConfidence`, `lightingConfidence`) sahiptir.
- Fakat GPU tarafında [`shaders/TAAResolve.glsl:L158-L176`](shaders/TAAResolve.glsl) materyal dokusunu (`g_Material`) bağlamaz; `surfaceId` hard rejection GPU'da fiilen çalışmaz.
- [`src/Geometry/SDFChangeSet.cpp:L294`](src/Geometry/SDFChangeSet.cpp) satırında `%70` ekran alanı geçildiğinde global invalidation tetikleyen kaba bir eşik (`totalArea >= 0.70f`) bulunmaktadır.

### 2.4. `BrickGrid` ve Sahne Sınırları
- Izgara boyutu sabittir: $32 \times 16 \times 32 = 16.384$ hücre.
- Hacim sınırları sabittir: `MinBounds: {-12, -1, -12}`, `MaxBounds: {12, 11, 12}`.
- Hücre boyutu $0.75\text{ m}$'dir. Bu kutunun dışındaki nesneler boşluk atlama (empty-space skipping) avantajından faydalanamaz.

### 2.5. Sabit `MAX_SDF_EDITS = 256` Limiti
- [`include/Astral/Renderer/SDFEdit.hpp:L69`](include/Astral/Renderer/SDFEdit.hpp) ve tüm shader'larda `#define MAX_EDITS 256` sabiti geçerlidir.
- Raymarch adımı ve gölge ışını başına 256 primitif döngüsü lineer $\mathcal{O}(N)$ maliyet üretir; hiyerarşik bir ivmelendirme (BVH/SVO) olmadan sahne ölçeklenemez.

### 2.6. `IUpscaler` Durumu
- [`include/Astral/Renderer/IUpscaler.hpp`](include/Astral/Renderer/IUpscaler.hpp) soyut arayüzü tanımlanmıştır.
- Kod tabanında bu arayüzü uygulayan (`FSR2`, `DLSS`, hatta `NativeTAA`) hiçbir somut sınıf bulunmamaktadır.

### 2.7. Editor, Asset ve Materyal Sistemi
- `AssetManager` sadece dosya yolu ve UUID meta verisi tutar; somut `Asset` türevleri (`Texture`, `Material`) yoktur.
- Materyal sistemi `SDFComponent` içerisindeki `albedo`, `roughness`, `metallic` alanlarından ibarettir (doku haritaları, emisyon, normal map yoktur).
- Asset hot-reload veya multi-scene streaming bulunmamaktadır.

### 2.8. Serileştirme (SceneSerializer)
- [`src/Scene/SceneSerializer.cpp`](src/Scene/SceneSerializer.cpp) 1.095 satırdır.
- `TransformComponent`, `VelocityComponent`, `HealthComponent`, `TagComponent`, `HierarchyComponent`, `SDFComponent` için manuel binary okuma/yazma blokları mevcuttur. Yeni bileşen eklendiğinde elle serializer yazılması zorunludur.

### 2.9. `QualitySettings` Entegrasyon Eksikliği
- [`include/Astral/Renderer/QualitySettings.hpp`](include/Astral/Renderer/QualitySettings.hpp) yapısı tanımlanmıştır.
- Ancak [`src/Renderer/SDFRenderer.cpp:L1427-L1430`](src/Renderer/SDFRenderer.cpp) satırlarında `shadowMaxSteps (96.0f)`, `shadowK (24.0f)`, `aoSamples (8.0f)`, `aoRadius (1.0f)` değerleri **sabit sayılarla hardcoded** olarak push constant'a yazılmaktadır. `QualitySettings` parametresi renderer'a aktarılmamaktadır.

---

## 3. Öncelikli İyileştirme Yol Haritası

1. **Aşama 1: Temsili Tekilleştirme (Representation Unification) [EN KRİTİK]**
   - `RenderExtractionSubsystem` doğrudan `SDFSceneSnapshot` / `SDFPrimitiveRecord` üretecek şekilde güncellenmeli.
   - `Application.cpp`, `SDFRenderer`'a doğrudan `SDFSceneSnapshot` iletmeli.
   - `SDFRenderer::UpdateEdits(vector<SDFEditGPU>)` geçici yaması kaldırılmalı veya doğrudan snapshot dönüştürücüsüne bağlanmalı; `surfaceId = i + 1` sahteliği sonlandırılmalı.
   - `RenderExtractionSystem.cpp` içindeki tehlikeli `ExtractAndUploadRenderData` fonksiyonu düzeltilmeli veya kaldırılmalı.

2. **Aşama 2: `BrickGrid` Akıllı Güncellemesini `SDFPrimitiveRecord`'a Taşımak**
   - `BrickGrid::Build(std::span<const SDFPrimitiveRecord>)` fonksiyonuna dirty tracking ve kısmi AABB güncellemesi eklenmeli; her kare 16.384 hücreyi baştan hesaplayan `FullRebuild` döngüsü kırılmalı.

3. **Aşama 3: `QualitySettings`'i Canlı Bağlamak**
   - `SDFRenderer::Render` fonksiyonuna `QualitySettings` referansı geçirilmeli ve push constant'lardaki hardcoded değerler bu yapıdan okunmalı.

4. **Aşama 4: SDFRenderer'ı Modüler Hale Getirmek**
   - `SDFGeometryPass`, `DeferredLightingPass`, `TAAResolvePass` ve `PickingPass` olarak ayrıştırılmalı.
