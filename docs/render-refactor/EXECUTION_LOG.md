# AstralEngine Render Refaktörü — Uygulama ve İcra Günlüğü (EXECUTION_LOG)

Bu belge, [RENDER_REFACTOR_IMPLEMENTATION_PLAN.md](../RENDER_REFACTOR_IMPLEMENTATION_PLAN.md) doğrultusunda gerçekleştirilen görevlerin adım adım icra ve doğrulama kayıtlarını içerir.

---

### Görev: G00 — Çalışma durumunu ve referansları dondur (Baseline Freeze)
- **Durum:** Tamamlandı
- **Kaynak commit ve çalışma ağacı bilgisi:**
  - Commit: `3adfa0b6f32df6712f7236c257a56334a8ca1385`
  - Kullanıcı değişiklikleri korundu: `WORK_PLAN.md`, `FINDINGS_VERIFICATION_REPORT.md`, `imgui.ini`
- **Değişen dosyalar:**
  - Yeni: `docs/render-refactor/BASELINE.md`
  - Yeni: `docs/render-refactor/EXECUTION_LOG.md`
  - Yeni artifact'lar: `artifacts/render-refactor/baseline/` (`build_release.log`, `ctest_release.log`, `contract_tests.log`, `visual_quality/`, `camera_gpu/`)
  - Motor veya test C++ kaynak kodu değiştirilmedi.
- **Uygulanan mimari karar:** Refaktör öncesi mevcut durumun dondurulması ve görsel/sözleşme referanslarının yedeklenmesi.
- **Davranış değişti mi; değiştiyse gerekçe:** Hayır, hiçbir davranış değişmedi (sıfır kod müdahalesi).
- **Çalıştırılan komutlar ve exit code:**
  - `cmake --build --preset mingw-release -j 4` -> Exit Code: `0`
  - `ctest --preset test-release --output-on-failure` -> Exit Code: `0`
  - `./build-release/EngineTests.exe --contract` -> Exit Code: `0`
- **CPU/GPU test sonucu ve artifact yolları:**
  - CTest: 23/23 geçti (%100 başarı, 20 CPU/editor, 3 GPU - 29.28 s)
  - EngineTests --contract: 2/2 suite, 43 assertion geçti (RTX 3060, Vulkan 1.4.351, Validation Layer aktif)
  - Loglar: `artifacts/render-refactor/baseline/build_release.log`, `ctest_release.log`, `contract_tests.log`
  - Görsel referanslar: `artifacts/render-refactor/baseline/visual_quality/`, `artifacts/render-refactor/baseline/camera_gpu/`
  - Rapor: `docs/render-refactor/BASELINE.md`
- **Performans ölçüldüyse koşullar ve sonuç:** G00 görevi için performans kıyaslaması yapılmadı; CTest süresi (29.28 sn) kaydedildi.
- **Bilinen eksik veya risk:** `SDFContractTests.cpp:270` satırında önceden var olan `foundChild` kullanılmayan değişken uyarısı mevcut.
- **Sonraki görev ve gerekli arayüzler:** G01 — Refaktör güvenlik testlerini gerçek GPU davranışıyla güçlendir (`RendererLifecycleGpuTests.cpp`, `RendererArchitectureTests.cpp`).

---

### Görev: G01 — Refaktör güvenlik testlerini gerçek GPU davranışıyla güçlendir
- **Durum:** Tamamlandı
- **Kaynak commit ve çalışma ağacı bilgisi:**
  - Commit: `3adfa0b6f32df6712f7236c257a56334a8ca1385`
  - Kullanıcı değişiklikleri korundu: `WORK_PLAN.md`, `FINDINGS_VERIFICATION_REPORT.md`, `imgui.ini`
- **Değişen dosyalar:**
  - Yeni: `Tests/EngineTests/src/RendererArchitectureTests.cpp`
  - Yeni: `Tests/EngineTests/src/RendererLifecycleGpuTests.cpp`
  - Değiştirildi: `Tests/EngineTests/src/main.cpp`
  - Değiştirildi: `Tests/EngineTests/CMakeLists.txt`
  - Dokümantasyon: `docs/RENDER_REFACTOR_IMPLEMENTATION_PLAN.md`, `docs/RENDER_REFACTOR_DESIGN.md`, `docs/render-refactor/EXECUTION_LOG.md`
- **Uygulanan mimari karar:**
  - CPU mimari/sözleşme testleri (`RendererArchitectureTests`) ile GPU yaşam döngüsü testleri (`RendererLifecycleGpuTests`) ayrıştırıldı.
  - `main.cpp` içerisine `--renderer` bayrağı eklendi ve bilinmeyen CLI bayraklarının sessizce tüm testleri çalıştırması engellendi (hata vererek çıkış yapması sağlandı).
  - CTest'e `EngineTests.RendererArchitecture` (CPU) ve `EngineTests.GPU.RendererLifecycle` (GPU, 180s) kaydedildi.
- **Davranış değişti mi; değiştiyse gerekçe:**
  - Hayır, motorun çalışma zamanı davranışı değiştirilmedi. Bilinmeyen test argümanlarında hata ile çıkış yapılması güvenliği sağlandı.
- **Çalıştırılan komutlar ve exit code:**
  - `cmake --build --preset mingw-release -j 4` -> Exit Code: `0`
  - `./build-release/EngineTests.exe --renderer` -> Exit Code: `0` (42 assertion doğrulandı)
  - `./build-release/EngineTests.exe --bilinmeyen-bayrak` -> Exit Code: `1` (Hata basıldı, testler tetiklenmedi)
  - `./build-release/RendererLifecycleGpuTests.exe` -> Exit Code: `0` (30 assertion doğrulandı)
  - `ctest --test-dir build-release -R RendererLifecycle --output-on-failure` -> Exit Code: `0`
  - `ctest --preset test-release --output-on-failure` -> Exit Code: `0` (25/25 test geçti)
  - `./build-release/EngineTests.exe --contract` -> Exit Code: `0` (43 assertion doğrulandı)
- **CPU/GPU test sonucu ve artifact yolları:**
  - `MissingCameraProducesOpaqueBlack`: Kamerasız çıktı piksellerinin tamamının RGB=0, A=255 olduğu doğrulandı.
  - `SelectionConsumedOnce`: İlk ConsumeSelectionResult isabet dönerken ikinci ardışık çağrının hasHit=false olduğu doğrulandı.
  - `ResizeSequenceAndEdgeDispatch`: 64x64 -> 127x65 -> 64x64 geçişi ve 8'in katı olmayan dispatch sağ-alt kenar pikselleri doğrulandı.
  - `TaaTransitionCycle`: TAA açık -> kapalı -> açık geçişlerinde stabilite doğrulandı.
  - `DebugModeCycle`: Mod 0..5 ve 0'a dönüş stabil render ve opak alfa ile doğrulandı.
  - `SceneTransitionNoHistoryBleed`: Sahne A (kırmızı) -> Boş Sahne -> Sahne B (mavi) geçişinde geçmiş rengin sızmadığı doğrulandı.
- **Performans ölçüldüyse koşullar ve sonuç:**
  - CTest toplam süre: 32.87 sn (21 CPU: 2.12 sn, 1 Editor: 0.04 sn, 4 GPU: 30.71 sn).
- **Bilinen eksik veya risk:** Yok. Tüm geçiş testleri Vulkan 1.4 validation layer açıkken sıfır hatayla geçti.
- **Sonraki görev ve gerekli arayüzler:** G02 — ShaderInterop ABI'sini ayır (`include/Astral/Renderer/ShaderInterop.hpp`).

---

### Görev: G02 — ShaderInterop ABI'sini ayır
- **Durum:** Tamamlandı
- **Kaynak commit ve çalışma ağacı bilgisi:**
  - Commit: `3adfa0b6f32df6712f7236c257a56334a8ca1385`
  - Kullanıcı değişiklikleri korundu: `WORK_PLAN.md`, `FINDINGS_VERIFICATION_REPORT.md`, `imgui.ini`
- **Değişen dosyalar:**
  - Yeni: `include/Astral/Renderer/ShaderInterop.hpp`
  - Değiştirildi: `include/Astral/Renderer/ComputePipeline.hpp`
  - Değiştirildi: `include/Astral/Renderer/SDFRenderer.hpp`
  - Değiştirildi: `Tests/EngineTests/src/RendererArchitectureTests.cpp`
  - Dokümantasyon: `docs/RENDER_REFACTOR_IMPLEMENTATION_PLAN.md`, `docs/render-refactor/EXECUTION_LOG.md`
- **Uygulanan mimari karar:**
  - Sekiz GPU struct'ı (`SelectionDataGPU`, `SDFPushConstants`, `TAAPushConstants`, `CameraUBOData`, `DebugCompositePushConstants`, `LightGPU`, `LightBufferHeader`, `DeferredLightingPushConstants`) `ComputePipeline.hpp` dosyasından ayrılarak saf veri sözleşmesi başlığı olan `ShaderInterop.hpp` içine taşındı.
  - Tüm struct'lar için `alignas(16)`, `sizeof`, `alignof` ve kritik `offsetof` değerlerini zorunlu kılan compile-time `static_assert` kontrolleri eklendi.
  - `ComputePipeline.hpp`, geriye dönük uyumluluk için `ShaderInterop.hpp` dosyasını include ederek korundu.
  - `RendererArchitectureTests.cpp` içerisine 28 adet ABI doğrulama assertion'ı eklendi.
- **Davranış değişti mi; değiştiyse gerekçe:**
  - Hayır, hiçbir GPU struct boyutu, bellek yerleşimi veya GLSL sözleşmesi değişmedi. Yalnızca modüler sahiplik ayrıldı.
- **Çalıştırılan komutlar ve exit code:**
  - `cmake --build --preset mingw-release -j 4` -> Exit Code: `0`
  - `./build-release/EngineTests.exe --renderer` -> Exit Code: `0` (70 assertion doğrulandı)
  - `./build-release/EngineTests.exe --contract` -> Exit Code: `0` (43 assertion doğrulandı)
  - `ctest --preset test-release --output-on-failure` -> Exit Code: `0` (25/25 test geçti)
- **CPU/GPU test sonucu ve artifact yolları:**
  - `SelectionDataGPU`: 32 bayt, 16 bayt hizalı, `hitPoint` ofseti 16 doğrulandı.
  - `SDFPushConstants`: 128 bayt, 16 bayt hizalı, `cameraRight` ofseti 96, `cameraUp` ofseti 112 doğrulandı.
  - `TAAPushConstants`: 96 bayt, 16 bayt hizalı, `colorParams` ofseti 16, `changedRect0` ofseti 32 doğrulandı.
  - `CameraUBOData`: 160 bayt, 16 bayt hizalı, `prevViewProj` ofseti 64, `prevCameraPosition` ofseti 128, `jitter` ofseti 144 doğrulandı.
  - `DebugCompositePushConstants`: 16 bayt, 16 bayt hizalı doğrulandı.
  - `LightGPU`: 48 bayt, 16 bayt hizalı, `direction` ofseti 16, `color` ofseti 32 doğrulandı.
  - `LightBufferHeader`: 16 bayt, 16 bayt hizalı doğrulandı.
  - `DeferredLightingPushConstants`: 128 bayt, 16 bayt hizalı, `shadowAOParams` ofseti 96, `qualityParams` ofseti 112 doğrulandı.
- **Performans ölçüldüyse koşullar ve sonuç:**
  - CTest toplam süre: 30.84 sn (21 CPU: 2.17 sn, 1 Editor: 0.12 sn, 4 GPU: 28.63 sn).
- **Bilinen eksik veya risk:** Yok. Tüm ABI sözleşmeleri hem derleme anında hem çalışma anında doğrulanmıştır.
- **Sonraki görev ve gerekli arayüzler:** G03 — Kare girdilerini isimlendir, legacy öncelikleri kayıt altına al (`include/Astral/Renderer/RenderFrameSettings.hpp`).

---

### Görev: G03 — Kare girdilerini isimlendir, legacy öncelikleri kayıt altına al
- **Durum:** Tamamlandı
- **Kaynak commit ve çalışma ağacı bilgisi:**
  - Commit: `3adfa0b6f32df6712f7236c257a56334a8ca1385`
  - Kullanıcı değişiklikleri korundu: `WORK_PLAN.md`, `FINDINGS_VERIFICATION_REPORT.md`, `imgui.ini`
- **Değişen dosyalar:**
  - Yeni: `include/Astral/Renderer/RenderFrameSettings.hpp`
  - Değiştirildi: `include/Astral/Renderer/SDFRenderer.hpp`
  - Değiştirildi: `src/Renderer/SDFRenderer.cpp`
  - Değiştirildi: `Tests/EngineTests/src/RendererArchitectureTests.cpp`
  - Değiştirildi: `Tests/EngineTests/src/RendererLifecycleGpuTests.cpp`
  - Dokümantasyon: `docs/RENDER_REFACTOR_IMPLEMENTATION_PLAN.md`, `docs/render-refactor/EXECUTION_LOG.md`
- **Uygulanan mimari karar:**
  - `RenderFrameSettings` açık veri yapısı oluşturuldu (12 alan: `camera`, `jitter`, `quality`, `frameId`, `width`, `height`, `normalMode`, `debugMode`, `selectedHitIndex`, `useGrid`, `optimizedShadows`, `taaEnabled`, `exposure`).
  - `SDFRenderer::Render` için `void Render(vk::CommandBuffer cmd, const RenderFrameSettings& settings)` overload'u eklendi.
  - Mevcut çok parametreli `Render(...)` imzası tersine uyumluluk için korundu; durum ve parametreleri `RenderFrameSettings` içine paketleyerek yeni overload'a delege etmesi sağlandı.
  - `time`, `width` ve `height` parametrelerinin kullanılmadığı açıkça dökümante edildi; yeni çağrıda verilen boyutların tahsis edilmiş render target (`m_Width`, `m_Height`) ile uyumu kontrol edildi.
  - Öncelik kuralları kayıt altına alındı: `exposure` (>0 ise açık, <=0 ise persistent fallback), `debugMode` (açık ayar baskın), `taaEnabled` (açık ayar baskın), `quality` (`shadowMaxSteps > 0` ise explicit, değilse persistent fallback), `selectedHitIndex` (!=-1 ise explicit, değilse persistent), `camera` (`has_value` ise explicit ve UBO senkronizasyonu, aksi halde persistent).
- **Davranış değişti mi; değiştiyse gerekçe:**
  - Hayır. Mevcut legacy çağrılar tam olarak aynı şekilde derlenir ve yeni overload ile bayt düzeyinde (`memcmp`, diff=0) özdeş pikseller üretir.
- **Çalıştırılan komutlar ve exit code:**
  - `cmake --build --preset mingw-release -j 4` -> Exit Code: `0`
  - `./build-release/EngineTests.exe --renderer` -> Exit Code: `0` (89 assertion doğrulandı, +19 assertion)
  - `./build-release/RendererLifecycleGpuTests.exe` -> Exit Code: `0` (32 assertion doğrulandı, +2 assertion)
  - `./build-release/EngineTests.exe --contract` -> Exit Code: `0` (43 assertion doğrulandı)
  - `ctest --preset test-release --output-on-failure` -> Exit Code: `0` (25/25 test geçti)
- **CPU/GPU test sonucu ve artifact yolları:**
  - `RenderFrameSettings` 12 alanının varsayılan değerleri CPU testi ile doğrulandı.
  - Kalite fallback kuralı (`shadowMaxSteps > 0` vs `== 0`) test edildi.
  - `exposure`, `debugMode`, `selectedHitIndex` öncelik çözümleme kuralları test edildi.
  - `LegacyVsExplicitPixelIdentity`: Legacy `Render(...)` ile yeni `Render(cmd, settings)` çıktıları aynı sahnede okunarak karşılaştırıldı; `memcmp` ile %100 bayt eşdeğerliği (0 piksel farkı) kanıtlandı.
- **Performans ölçüldüyse koşullar ve sonuç:**
  - CTest toplam süre: 31.54 sn (21 CPU: 2.28 sn, 1 Editor: 0.13 sn, 4 GPU: 29.22 sn).
- **Bilinen eksik veya risk:** Yok.
- **Sonraki görev ve gerekli arayüzler:** G04 — `ComputeProgram` ve shader yüklemeyi merkezileştir (`include/Astral/Renderer/ComputeProgram.hpp`, `src/Renderer/ComputeProgram.cpp`).

