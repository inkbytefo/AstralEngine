# Astral Engine - Kod İnceleme ve Refactoring Analiz Raporu

Bu rapor, Astral Engine projesinin mevcut durumunu, kod kalitesini ve yapısal sorunlarını detaylandırır.

## Analiz Özeti

| Dosya / Bileşen | Tip | Açıklama |
| :--- | :--- | :--- |
| `Source/Core/MemoryManager` | **Dead** | Hiçbir sistem tarafından kullanılmıyor. Frame-based allocation için planlanmış ancak atıl durumda. |
| `Source/test_sdl3.cpp` | **Dead** | Geliştirme aşamasından kalma, ana build veya test suite'e dahil değil. |
| `ECSSubsystem` | **Missing** | `main.cpp`'de include edilmesine rağmen dosya dizinde yok. Build'i tamamen kırıyor. |
| `MovementComponent` | **Redundant** | `Components.h` içinde tanımlı ancak hiçbir sistem (System) tarafından işlenmiyor. |
| `ScriptComponent` | **Placeholder** | Mimari belgesinde "planlama aşamasında" olarak belirtilmiş; şu an sadece boş bir veri yapısı. |
| `SceneEditorSubsystem` | **Hatalı** | 100+ derleme hatası barındırıyor. `entt` ve `imgui` API'leri ile uyumsuz (muhtemelen eski versiyondan kalma). |
| `Scene.h` / `Scene.cpp` | **Incomplete** | `ISubsystem` arayüzünü uygulamıyor, bu da motorun modüler yapısıyla çelişiyor. |
| `main.cpp` | **Broken** | Olmayan `ECSSubsystem.h` referansı nedeniyle derleme başarısız oluyor. |

## Detaylı Bulgular

### 1. Aktif Kullanılan Kodlar
- **Core**: `Logger`, `ThreadPool`, `UUID` sistemleri aktif ve sağlıklı çalışıyor.
- **Subsystems**: `Platform`, `Asset`, `Renderer`, `UI` sistemleri temel fonksiyonlarını yerine getiriyor (ancak Editor ile entegrasyon noktaları zayıf).
- **Renderer**: Vulkan RHI katmanı aktif; `VulkanDevice` ve `VulkanResources` temel render döngüsünü sağlıyor.

### 2. Kullanılmayan ve Gereksiz Kodlar (Dead/Redundant)
- **MemoryManager**: Klasik bir Singleton Singleton örneği ancak motorun geri kalanında `std::unique_ptr` ve manual `new`/`delete` tercih edilmiş.
- **Placeholder Logic**: `SceneEditorSubsystem.cpp` içinde "SaveScene" ve "LoadScene" gibi kritik fonksiyonlar sadece `Logger::Info` basan boş gövdelerden ibaret.

### 3. Duplicate Logic (Tekrarlanan Mantık)
- **UI Panelleri**: `SceneEditorSubsystem` içindeki `RenderViewportPanel`, `RenderSceneHierarchy` vb. fonksiyonlar ImGui state yönetimini (`ImGui::Begin`/`End`) her seferinde manuel yapıyor. Bunlar için bir `EditorPanel` arayüzü oluşturulmalıdır.
- **Transform Hesaplamaları**: Hem `TransformComponent` hem de `SceneEditorSubsystem` içinde benzer matris çarpımları (`translation * rotation * scale`) görülüyor.

### 4. Kritik Hatalar
- **Build Durumu**: Proje şu anki haliyle derlenemez. `SceneEditorSubsystem` içindeki `entt::registry::each` ve `ImGui::DockSpaceOverViewport` kullanımları güncel kütüphane sürümleriyle uyumsuz.

---

## Temizlik ve Refactoring Planı

### Adım 1: Acil Onarım (Build Fix)
- **ECSSubsystem**: `main.cpp`'deki hatalı referansı temizle. `Scene` sınıfını bir subsystem haline getirecek olan `SceneSubsystem` yapısını oluşturun.
- **Editor Fix**: `SceneEditorSubsystem` içindeki API uyumsuzluklarını gider. `entt::registry` kullanımını `view<>` pattern'ına uygun hale getirin.

### Adım 2: Ölü Kod Temizliği
- **Silinecekler**: `Source/Core/MemoryManager.h/cpp`, `Source/test_sdl3.cpp`.
- **Placeholder'lar**: `MovementComponent` ve `ScriptComponent` eğer aktif bir geliştirme planı yoksa `Source/ECS/Experimental/` altına taşınmalı veya silinmelidir.

### Adım 3: Yapısal Refactoring (Logic Unification)
- **Subsystem Standardizasyonu**: `Scene` sınıfını `ISubsystem`'den türet ve `Engine`'e kayıtlı hale getir.
- **UI Base Class**: `EditorPanel` abstract sınıfını oluştur. Her panel (Outliner, Details, Viewport) bu sınıftan türemeli ve `OnDraw()` metodunu override etmeli.

### Adım 4: Riskler ve Performans
- **Breaking Changes**: `ECSSubsystem` referansının kaldırılması Sandbox uygulamalarında değişikliğe yol açabilir.
- **Kazanç**: Kod tabanında %10-15 küçülme, build süresinde (hatalı dosyaların onarımı sayesinde) stabilite ve tip-güvende artış.

> [!TIP]
> Refactoring sonrası `Tests/` dizinindeki birim testlerin çalıştırılması ve `build.log`'un sıfır hata ile tamamlanması hedeflenmelidir.
