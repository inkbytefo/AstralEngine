# Astral Engine - Geliştirme Yol Haritası (Roadmap)

Bu belge, **Astral Engine**'in stratejik geliştirme planını, mevcut durumunu ve gelecek hedeflerini detaylandırır. Proje, modern bir oyun motoru mimarisi üzerine inşa edilmekte olup, performans ve görsel kaliteyi ön planda tutmaktadır.

---

## 🏁 Faz 1: Temel Mimari ve RHI (✅ Tamamlandı)
*Hedef: Sağlam bir çekirdek yapı ve modern bir grafik API katmanı oluşturmak.*

- [x] **Subsystem Mimarisi:** Motor yaşam döngüsü (Initialize, Update, Shutdown) yönetimi.
- [x] **Platform Katmanı:** SDL3 entegrasyonu ile Pencere ve Input yönetimi.
- [x] **RHI (Render Hardware Interface):** 
    - [x] Vulkan 1.3 Backend entegrasyonu.
    - [x] **Dynamic Rendering:** VkRenderPass ve Framebuffer bağımlılığının kaldırılması.
    - [x] **VMA (Vulkan Memory Allocator):** Verimli GPU bellek yönetimi.
    - [x] **Staging Buffers:** CPU'dan GPU'ya hızlı veri transferi.
- [x] **Log Sistemi:** Thread-safe dosya ve konsol loglama.

---

## 📦 Faz 2: Asset Pipeline ve Temel Render (✅ Tamamlandı)
*Hedef: Veri odaklı bir yapıya geçiş ve 3D modellerin görüntülenmesi.*

- [x] **Mesh Abstraction:** Vertex ve Index Buffer yönetimi.
- [x] **AssetSubsystem:** Assimp entegrasyonu ile Model (GLTF, OBJ) yükleme.
- [x] **Texture System:** `stb_image` ile doku yükleme ve GPU'ya aktarma.
- [x] **Kamera Sistemi:** View/Projection matrisleri ve serbest kamera kontrolü.
- [x] **Temel Işıklandırma:** Phong/Blinn-Phong aydınlatma modelleri.

---

## 🏗️ Faz 3: Sahne Yönetimi ve ECS (✅ Tamamlandı)
*Hedef: Karmaşık sahneleri yönetmek için hiyerarşik ve performanslı bir yapı.*

- [x] **ECS (Entity Component System):** `entt` kütüphanesi entegrasyonu.
- [x] **Transform Hiyerarşisi:** Parent-Child ilişkileri ve dünya matrisi hesaplamaları.
- [x] **Scene Serializer:** Sahnelerin YAML/JSON formatında kaydedilmesi ve yüklenmesi.
- [x] **Robust Windowing:** Vulkan fallback mekanizması ve manuel Win32 surface desteği.

---

## 🛠️ Faz 4: Editör ve Araçlar (✅ Büyük Oranda Tamamlandı)
*Hedef: Geliştiriciler için WYSIWYG (Ne görürsen onu alırsın) çalışma ortamı.*

- [x] **ImGui Entegrasyonu:** Modern bir UI katmanı.
- [x] **Editor Viewport:** Render sonucunun bir ImGui penceresinde görüntülenmesi.
- [x] **Scene Hierarchy & Inspector:** Entity yönetimi ve bileşen düzenleme.
- [x] **Content Browser:** Dosya sistemi üzerinden asset yönetimi.
- [ ] **Gizmos (Planlanan):** 3D manipülasyon araçları (Translate, Rotate, Scale).

---

## 🎨 Faz 5: PBR ve IBL (Gelişmiş Aydınlatma) (🚧 Devam Ediyor)
*Hedef: Profesyonel seviyede görsel sadakat ve modern render teknikleri.*

- [x] **PBR (Physically Based Rendering):** Metallic/Roughness iş akışı.
- [x] **Material System:** Albedo, Normal, Metallic-Roughness, AO ve Emissive harita desteği.
- [x] **Shadow Mapping:** Temel Directional Light gölgeleri.
- [x] **IBL (Image Based Lighting) Altyapısı:**
    - [x] Cubemap Doku Desteği (RHI).
    - [x] HDR Doku Yükleme (TextureImporter).
    - [x] IBL Shader'ları (Irradiance, Prefilter, BRDF LUT).
    - [x] PBR Shader Entegrasyonu.
- [x] **IBL Harita Üretim Sistemi:** Runtime generator (Subresource synchronization ve layout transition iyileştirmeleri ile).
- [x] **Skybox Sistemi:** Gökyüzü kutusu yönetimi.
- [ ] **CSM (Cascaded Shadow Maps):** Geniş alanlar için yüksek kaliteli gölgeler.
- [ ] **Post-Processing Stack:**
    - [ ] **Tone Mapping:** ACES veya Filmic ton eşleme.
    - [ ] **Bloom:** Işık patlaması efektleri.
    - [ ] **SSAO:** Ekran alanı ortam kapatma.
    - [ ] **Anti-Aliasing:** FXAA veya TAA entegrasyonu.

---

## ⚡ Faz 6: Optimizasyon ve Çekirdek İyileştirmeler (Gelecek Hedefler)
*Hedef: Performansın maksimize edilmesi ve sistem kararlılığı.*

- [ ] **Bellek Yönetimi İyileştirmeleri:**
    - [ ] **Transient Resource System:** Geçici veriler için havuz tabanlı yönetim.
    - [ ] **Resource GC:** Kullanılmayan GPU kaynaklarının otomatik temizlenmesi.
- [ ] **Multi-threading:**
    - [ ] **Job System:** Görev tabanlı paralel işleme.
    - [ ] **Parallel Command Recording:** Çoklu iş parçacığı ile render komutu kaydı.
- [ ] **Asset Pipeline Gelişmiş:**
    - [ ] **Shader Hot-Reloading:** Çalışma zamanında shader güncelleme.
    - [ ] **Astral Binary Format:** Hızlı yükleme için özel binary asset formatı.
- [ ] **Compute Shader Uygulamaları:** GPU tabanlı parçacık sistemleri ve culling.

---

## 🚀 Faz 7: Genişletilebilirlik (Uzun Vadeli)
*Hedef: Motorun tam bir oyun geliştirme platformuna dönüştürülmesi.*

- [ ] **Scripting:** C# (Mono) veya Lua entegrasyonu.
- [ ] **Physics:** Jolt Physics veya PhysX entegrasyonu.
- [ ] **Audio:** 3D uzamsal ses desteği (FMOD veya OpenAL).
- [ ] **Navigation:** NavMesh ve Pathfinding sistemleri.
