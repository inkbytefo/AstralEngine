# F0 Doğrulama ve Benchmark Raporu

**Tarih:** 2026-11-23 · **İncelenen Taban:** `master` · `eeeeea1` (+ F0 doğrulama düzeltmeleri)

## 1. Sabitlenen Ortam ve Toolchain

| Bileşen | Değer |
|---|---|
| İşletim Sistemi | Windows 11 x64 (`10.0.26300`) |
| CPU / RAM | AMD Ryzen 3 3100 4-Core Processor · 23.9 GB RAM |
| GPU / Sürücü | NVIDIA GeForce RTX 3060 (12 GB, discrete, vendor `0x10de`) · Sürücü `621.14` · Vulkan API `1.4.357` |
| Derleyici | MSVC `19.51.36260` for x64 (Visual Studio 18 Community) |
| Derleme Araçları | CMake `4.2.1` · Ninja `1.13.2` · Python `3.14.4` · Vulkan SDK `1.4.335.0` |
| Hedef Proje Sürümü (`CMakeLists.txt`) | `0.17.0` (*Not: M13.5/M13.6 roadmap sürüm etiketleri 0.18.0/0.19.0 — takip kaydı F0-TRACK-04*) |

---

## 2. CI vs Yerel Doğrulama Matrisi

| İş / Kontrol | CI (`.github/workflows/ci.yml`) | Yerel Koşu (Windows MSVC) | Sonuç | Not / Gerekçe |
|---|---|---|---|---|
| `windows-msvc-debug` (`cmake --preset debug`) | Release + Debug (`-LE golden`) | `cmake --preset debug` + tüm CTest | **54/54 non-golden PASS**; 12 `golden` SKIP; 5/12 `golden-hw` PASS, 7/12 FAIL | Tüm birim, entegrasyon, feature, script tip, katman, boyut ve smoke testleri geçti (`code_size_check`: 0 aşım, 1 istisna) |
| `windows-msvc` (`Release`, `-DASTRAL_WARNINGS_AS_ERRORS=ON`) | Release (`-WX`) | `cmake --preset release -DASTRAL_WARNINGS_AS_ERRORS=ON` | **54/54 non-golden PASS** (56.13 s), 0 uyarı | `layer_check`, `layer_check_self_test`, `code_size_check`, `shared_features_in_sync`, `astral_public_headers_clean` PASS |
| Kurulum + `tests/consumer` (`find_package`) | `linux` job | `cmake --install` + `astral_consumer` + `consumer_game` + `consumer_game_package` | **PASS** | Hem gevşek hem `.apak` paket modunda çalıştı |
| Kurulum + `templates/blank` (bağımsız) | `linux` job | Kopya + `find_package(Astral)` + build + `ctest` | **PASS (2/2)** | **Düzeltildi:** `templates/blank/CMakeLists.txt` içindeki eski `doctest v2.4.11` CMake 4.x'te kırılıyordu → `v2.5.3` + `DOCTEST_USE_STD_HEADERS=ON` yapıldı |
| `linux-gcc` / `linux-clang` (Release, `-Werror`, lavapipe `golden`) | `ubuntu-24.04` (`g++-13`, `clang++-18`) | *Atlandı (CI-only)* | — | Yerel makinede Linux/WSL/lavapipe kurulu değil; GitHub Actions koşusu gerekli |
| `asan` (`CMakePresets.json`) | Preset tanımlı (CI job'ı yok) | *Atlandı (CI-only)* | — | GCC/Clang `-fsanitize=address,undefined` Linux ortamı gerektirir |
| `linux-tsan` (`core/world/physics/script` + `app` headless) | `ubuntu-24.04` | *Atlandı (CI-only)* | — | F0'da `ci.yml` `tsan` işine `astral_app_tests` headless alt kümesi (`ecs_app_test`, `frame_order_test`, `physics_app_test`, `world_app_test`) eklendi |
| `linux-clang-include-hygiene` | `ubuntu-24.04` (`continue-on-error: true`) | *Atlandı (CI-only)* | — | `clang-tidy-18` Linux ortamı gerektirir |

---

## 3. Golden Görüntü Testleri

- **`ctest -L "^golden$"` (lavapipe):** 12/12 **Skipped** (`NVIDIA GeForce RTX 3060` donanım GPU algılandığı için `MODE=software` bilinçli olarak atlanır — ADR-0031).
- **`ctest --preset release -L "^golden-hw$"` (RTX 3060, 320×180, SSIM ≥ 0.95 & strong diff >32 ≤ %2.5):** **5/12 PASS, 7/12 FAIL** (Debug ve Release aynı sonucu verir):

| Sahne | Durum | SSIM (min 0.95) | Strong Diff >32 (max %2.5) | Max Diff | Mean Diff |
|---|---|---:|---:|---:|---:|
| `open_world` | PASS | — | — | — | — |
| `open_world_night` | PASS | — | — | — | — |
| `open_world_nudged` | PASS | — | — | — | — |
| `blank` | PASS | — | — | — | — |
| `sdf_scene` | PASS | — | — | — | — |
| `scene_viewer` | **FAIL** | 0.9502 | %13.550 (7805 px) | 77 | 15.206 |
| `rt_showcase` | **FAIL** | 0.9542 | %12.470 (7183 px) | 82 | 14.698 |
| `pong` | **FAIL** | 0.6232 | %33.799 (19468 px) | 170 | 28.830 |
| `shader_showcase` | **FAIL** | 0.8798 | %36.675 (21125 px) | 123 | 25.041 |
| `hello_triangle` | **FAIL** | 0.9445 | %0.248 (143 px) | 39 | 11.553 |
| `spinning_cubes` | **FAIL** | 0.9715 | %2.507 (1444 px) | 62 | 8.944 |
| `physics_playground` | **FAIL** | 0.9683 | %5.979 (3444 px) | 90 | 14.146 |

**Kök neden analizi:** `tests/golden/references/*.png` referansları M11–M12 döneminde lavapipe ile üretildi; ardından `15ab10a`, `6a0d5c9` ve `c1de0e7` (M13 BSDF/renk birleştirmesi, ReSTIR DI, DDGI/denoiser/bloom düzeltmeleri) görüntü çıktısını bilerek değiştirdi ancak lavapipe referansları yeniden üretilmedi (`F0-TRACK-01`).

---

## 4. Performans Tabanı (Release · MSVC 19.51 · `eeeeea1`)

### 4.1 CPU & Headless Ölçek (`astral-bench`)

Donanım: AMD Ryzen 3 3100 (4C/8T), 23.9 GB RAM · Derleme: Release (`/O2 /Ob2`)

| Benchmark | Ölçek / Sahne | Örnek | p50 (ms) | p95 (ms) | p99 (ms) | Hızlanma | Bellek (RSS) |
|---|---|---:|---:|---:|---:|---:|---:|
| `bench_ecs` (seri) | 100.000 entity · 30 sistem (5 dalga) | 12 | 10.354 | 10.806 | 10.806 | 1.00× | 11.0 MB |
| `bench_ecs` (paralel) | 100.000 entity · 30 sistem (5 dalga) | 12 | 6.362 | 6.782 | 6.782 | **1.63×** | 11.0 MB |
| `bench_transforms` (seri) | 100.000 entity · derinlik 4 | 20 | 10.900 | 11.294 | 11.294 | 1.00× | 27.5 MB |
| `bench_transforms` (paralel) | 100.000 entity · derinlik 4 | 20 | 7.086 | 8.515 | 8.515 | **1.54×** | 27.5 MB |
| `bench_script` (`astral.each`) | 10.000 entity · Luau 0.748 | 10 | 3.644 | 4.873 | 4.873 | — | 12.6 MB |
| `bench_streaming` (burst) | 257 hücre · 15.360 entity (paralel decode) | 1 | 14.658 (toplam) | — | — | — | 50.2 MB |
| `bench_streaming` (uçuş: worker decode) | 16×16 hücre (2048×2048 m), 120 kare, 146 attach / 146 detach / 0 cancel | 146 hücre | **0.279** | **0.425** | **0.631** | — | 50.2 MB |
| `bench_streaming` (uçuş: ana thread tick) | aynı uçuş profili (`max_attach=4`) | 120 kare | **0.029** | **0.050** | **0.053** | — | 50.2 MB |
| `bench_shader` (`.ashader` → SPIR-V) | surface shader derleme (glslang) | 10 | 21.353 | 22.715 | 22.715 | — | 34.2 MB |

### 4.2 GPU Referans Profili (`astral-render-bench`)

Donanım: NVIDIA GeForce RTX 3060 12 GB · Sürücü `621.14` · Çözünürlük: `1280×720` · `warmup=16`, `measure=16` · Hibrit RT (`rt=1`, `timestamps=1`)

| Sahne | Kalite / İçerik | GPU Kare (ms) | FPS | Wall (ms) | Not / Metrik |
|---|---|---:|---:|---:|---|
| `cornell_area` | 560 prob, 1 rect, 3 SDF, 24 pass | 7.126 | 140.3 | 7.553 | `rt.lighting` 4.023 ms, `denoise.atrous` 1.676 ms |
| `material_grid` | 216 prob, 25 PBR küre, 23 pass | 2.611 | 383.0 | 3.168 | `rt.lighting` 1.078 ms |
| `thin_wall` | 288 prob, ince duvar, 23 pass | 4.774 | 209.5 | 5.225 | `rt.lighting` 2.107 ms |
| `high_lights_ris` | 384 prob, 32 nokta + 4 rect, 8 SDF (RIS) | 16.400 | 61.0 | 16.966 | `rt.lighting` 12.713 ms |
| `high_lights` | 384 prob, 32 nokta + 4 rect, 8 SDF (ReSTIR DI) | 18.224 | 54.9 | 18.601 | `restir` 6.281 ms + `rt.lighting` 8.304 ms |
| `many_lights_ris` | 384 prob, 128 nokta + 4 rect (RIS) | 7.483 | 133.6 | 8.123 | 1-kare ham RMSE 32.903 |
| `many_lights_restir` | 384 prob, 128 nokta + 4 rect (ReSTIR DI) | 8.536 | 117.1 | 9.096 | 1-kare ham RMSE **28.288** |
| `moving_camera` | `denoise_iter=0`, 20 kare | — | — | — | `spatial_hf_noise = 0.848` |
| `moving_mirror` | 16 kare hareketli kamera | 2.637 | 379.2 | — | `mirror_motion_rmse = 6.871` |
| `open_world_scale` | 256 prob, 2000 m açıklık | 2.514 | 397.8 | 3.073 | `rt.lighting` 0.705 ms |
| `m13_pbr_rt` | 192 prob, 7 M13 PBR materyal + 1 decal | 3.008 | 332.5 | 3.562 | `decals` 0.061 ms |
| `m13_pbr_pt` (64 SPP) | 6 sekme yansız path tracer | 0.905 / spp | — | — | `rt_vs_pt_rmse = 2.29`, `mean_luma rt=208.3 pt=208.0` |

---

## 5. Açık Takip Kayıtları ve Riskler

| ID | Alan | Sorun / Risk | Önerilen Aksiyon |
|---|---|---|---|
| **F0-TRACK-01** | Golden / Render | 7/12 `golden-hw` testi M13 sonrası eski lavapipe referansları nedeniyle başarısız oluyor (`scene_viewer`, `rt_showcase`, `pong`, `shader_showcase`, `hello_triangle`, `spinning_cubes`, `physics_playground`). | Linux/lavapipe ortamında `ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$'` ile referansları görsel inceleme sonrası yenile. |
| **F0-TRACK-02** | CI / Sanitizer | `asan` preset'i CI işine bağlı değil; `tsan` için yeni eklenen `astral_app_tests` headless alt kümesi bir sonraki Linux CI koşusunda doğrulanmalı. | Linux CI koşusunu ve (isteğe bağlı) haftalık/PR `asan` işini etkinleştir. |
| **F0-TRACK-03** | Kurulum / Scripting | Bağımsız `templates/blank` kurulu pakete karşı `blank_tests` ve `blank_features_check`'i çalıştırıyor ancak `blank_script_types` (`astral-luau-check`) kurulu hedef olarak eklenmemiş. | `AstralInstall.cmake` / `AstralGame.cmake` içinde `Astral::astral_luau_check` export kontrolünü tamamla (F1). |
| **F0-TRACK-04** | Sürüm / Doküman | `CMakeLists.txt` `VERSION 0.17.0` (roadmap'te M13.6 = 0.19.0); `docs/modules/platform.md` ve `gpu.md` "Bilinen sınırlar" bölümlerinde tamamlanmış özellikler (gamepad, metin girişi, timestamp) eski satır olarak kalmış. | Sürüm politikasını hizala ve modül sınır notlarını temizle. |
| **F0-TRACK-05** | Sunucu Derlemesi | `AppDesc::headless` GPU'suz çalışsa da kök CMake `modules/gpu` üzerinden Vulkan SDK gerektirir. | N0 dedicated server paketleme fazında SDK'sız sunucu derleme seçeneğini değerlendir. |
