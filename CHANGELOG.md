# Changelog

Biçim: [Keep a Changelog](https://keepachangelog.com/). Sürümler [SemVer](https://semver.org/).

## [Unreleased]

## [0.3.0] — 2026-11-21 — M2

### Added
- `Astral::World`: archetype ECS — `create/destroy/add/remove/get/has`, `each`, `parallel_each`,
  `Commands`, kaynaklar, `EntityRef`; `Transform`/`Parent`/`WorldTransform` hiyerarşisi.
- `Astral::Render`: instanced forward renderer (vertex pulling), derinlik, yönlü ışık; küp/düzlem/küre.
- `Astral::App`: `App`, `Game`, `System`, `Context`, fazlar, headless mod, sabit zaman adımı,
  `MeshRenderer`, `Camera`, `Time`; `<astral/astral.hpp>`.
- `examples/spinning_cubes`, oyun yazma rehberi, `ASTRAL_ASSERT`, asan preset.

### Changed
- `Swapchain::present()` artık komut listesini alıp gönderiyor: `swapchain.present(cmd)`.

### Fixed
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
