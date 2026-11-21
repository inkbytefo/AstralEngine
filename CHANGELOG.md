# Changelog

Biçim: [Keep a Changelog](https://keepachangelog.com/). Sürümler [SemVer](https://semver.org/).

## [Unreleased]

## [0.5.0] — 2026-11-21 — M4

### Added
- `Astral::Physics` (Jolt): kutu/küre/kapsül/silindir; statik/kinematik/dinamik body; sensör; CCD;
  impulse/kuvvet/hız; raycast; temas olayları; karakter kontrolcüsü.
- App: `RigidBody`, `CharacterController` component'leri; `Phase::FixedUpdate`; sabit zaman adımı
  (`physics_rate`, `max_physics_steps`); `ctx.physics`, `ctx.contacts()`, `entity_of(RayHit)`.
- `examples/physics_demo`; kurulan pakette Jolt bağımlılığı.

## [0.4.0] — 2026-11-21 — M3

### Added
- `RenderGraph`: bildirilen erişimlerden otomatik bariyer ve render pass, geçici texture havuzu,
  kullanılmayan pass ayıklama.
- Hibrit deferred renderer: gölge haritası (PCF), mesh G-buffer, SDF raymarch (küre/kutu/torus/kapsül/
  silindir; union/subtract/intersect/smooth), GGX PBR, SDF yumuşak gölgeleri, nokta ışıklar, gök/yer
  ambient ve gökyüzü yansıması, ACES tonemap.
- App: `Sdf` ve `PointLight` component'leri, `MeshRenderer` materyal alanları, `ctx.sun`, `ctx.environment`.
- `rotation_of(Mat4)`, `examples/sdf_scene`.
- GPU: formatsız storage image yazımı (HDR compute hedefleri), derinlik texture örnekleme.

### Changed
- `Renderer::render(cmd, target, camera, RenderScene)`; `DirectionalLight` artık ambient taşımıyor (`Environment`).
- `Device::report_error()` (üst katmanlar cihazın logger'ını kullanır).

### Fixed
- GPU: yalnızca transfer amaçlı texture'lar için geçersiz image view oluşturuluyordu; aynı durumda art
  arda yazmalar arasında bariyer eksikti.
- Render (M3 incelemesi): kamera bir SDF şeklinin içine girince tüm kare NaN oluyordu; büyük mesh'lerin
  gölge kutusunun dışında kalması; büyük sahnelerde gölgelerin nesneden kopması (texel ölçekli bias,
  ışık uzayında sıkı sığdırma); bir pass'te aynı texture'ın hem okunup hem yazılması artık reddediliyor.

## [0.3.0] — 2026-11-21 — M2

### Added
- `Astral::World`: archetype ECS — `create/destroy/add/remove/get/has`, `each`, `parallel_each`,
  `Commands`, kaynaklar, `EntityRef`; `Transform`/`Parent`/`WorldTransform` hiyerarşisi.
- `Astral::Render`: instanced forward renderer (vertex pulling), derinlik, yönlü ışık; küp/düzlem/küre.
- `Astral::App`: `App`, `Game`, `System`, `Context`, fazlar, headless mod, sabit zaman adımı,
  `MeshRenderer`, `Camera`, `Time`; `<astral/astral.hpp>`.
- `examples/spinning_cubes`, oyun yazma rehberi, `ASTRAL_ASSERT`, asan preset (LSan bastırmalarıyla).
- `CommandList::on_submitted`, kurulan pakette `astral_add_shaders`.

### Changed
- `Swapchain::present()` artık komut listesini alıp gönderiyor: `swapchain.present(cmd)`.

### Fixed
- World (M2 incelemesi): farklı .cpp dosyalarındaki aynı isimli component'lerin aynı depoyu paylaşması;
  dünyadaki bir değeri kopyalarken serbest bırakılmış belleğe erişim; `parallel_each` içinden
  `commands()` yarış durumu; derin/döngüsel hiyerarşilerde yanlış sonuç.
- Render/App: kare halkasının GPU okurken üzerine yazılabilmesi; kapanışta system'lerin silinmiş
  renderer'a erişmesi; `run()`'ın iki kez çağrılması.
- GPU (M1 incelemesi): gönderilmemiş listedeki kaynağın erken silinmesi (use-after-free); pipeline
  olmadan draw/dispatch çökmesi; sınır dışı kopyalar; bırakılan listenin durum izini bozması;
  acquire/present kilitlenmeleri; WSI geçiş senkronizasyonu; Wayland'de yeniden boyutlama;
  anında gönderimlerde bariyer eksikliği; submit hatasında kilitlenme.

## [0.2.0] — 2026-11-21 — M1

### Added
- `Astral::Gpu`: Vulkan 1.3 RHI — `Device`, handle tabanlı buffer/texture/pipeline, bindless texture
  tablosu, buffer device address, timeline semaphore, gecikmeli silme, `CommandList`, `Swapchain`.
- `astral_add_shaders`: GLSL → SPIR-V, programa gömülü; `<astral/bindless.glsl>`.
- `Astral::Platform`: SDL3 `Window`, `Input` (tuş/fare, pressed/released, `wasd()`).
- `Astral::Core`: `math.hpp` (Vec, Mat4, Quat, Vulkan projeksiyonları); `HandlePool` tag tipi ve `for_each`.
- `examples/hello_triangle`, pencere+present entegrasyon testi, public header kural testi.
- CI: lavapipe + Xvfb ile GPU/pencere testleri, Windows'ta Vulkan SDK.

### Changed
- Yol haritası: World + App, render graph'tan önceye alındı (ADR-0006).

### Fixed
- M0 inceleme borçları: Threads bağımlılığı, kurulum dizinleri, üretilen sürüm, atomic log seviyesi.

## [0.1.0] — 2026-11-21 — M0

### Added
- Modüler CMake sistemi: `astral_add_module`, `astral_add_tests`, preset'ler (debug/release/tsan).
- `Astral::Core`: `Logger` + sink'ler, `HandlePool`, `Clock`, `JobSystem`.
- Kurulum/export: `find_package(Astral CONFIG COMPONENTS Core)`, tüketici testi.
- `examples/hello_core`, GitHub Actions CI (GCC, Clang, MSVC, TSan).
- Dokümantasyon sistemi: mimari, yol haritası, süreç, modül dokümanları, ADR'ler.

### Fixed
- `JobSystem::wait()` başka grubu bekleyen bir job yüzünden kilitlenebiliyordu (ADR-0004).
