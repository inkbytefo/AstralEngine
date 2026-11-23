# AstralEngine Yetenek Envanteri (F0)

**Tarih:** 2026-11-23 · **Taban SHA:** `eeeeea1` (+ F0 doğrulama değişiklikleri) · **Kanıt raporu:** [f0-verification-report.md](f0-verification-report.md)

**Durum anahtarı:** `implemented` (kaynak + test doğrulandı) · `partial` (temel mevcut, kapsam/özellik eksik) · `planned` (yol haritasında) · `unsupported` (bilinçli kapsam dışı)

## Modül Bağımlılık Grafiği (CMake Doğrulanmış)

`build/debug/astral_module_graph.json` çıktısı `tools/module_layers.json` ile `layer_check` ve `layer_check_self_test` altında 100% uyumlu doğrulandı.

| Katman | Modül | Hedef / Alias | Doğrudan Bağımlılıklar | Public Başlık | Test Dosyası |
|---|---|---|---|---:|---:|
| 0 | `core` | `astral_core` / `Astral::Core` | — | 11 | 12 |
| 1 | `shader` | `astral_shader` / `Astral::Shader` | `core` | 2 | 3 |
| 1 | `gpu` | `astral_gpu` / `Astral::Gpu` | `core` | 4 | 6 |
| 1 | `platform` | `astral_platform` / `Astral::Platform` | `core`, `gpu` | 3 | 4 |
| 2 | `world` | `astral_world` / `Astral::World` | `core` | 13 | 15 |
| 2 | `script` | `astral_script` / `Astral::Script` | `core`, `world` | 3 | 3 |
| 2 | `physics` | `astral_physics` / `Astral::Physics` | `core` | 2 | 6 |
| 2 | `render` | `astral_render` / `Astral::Render` | `core`, `gpu`, `shader` | 9 | 10 |
| 2 | `audio` | `astral_audio` / `Astral::Audio` | `core` | 1 | 2 |
| 2 | `assets` | `astral_assets` / `Astral::Assets` | `core`, `world`, `render` | 4 | 5 |
| 3 | `app` | `astral_app` / `Astral::App` | tüm alt modüller + `Astral::ImGui` | 13 | 21 |

Kaynak taraması (`modules/`, `tools/`, `examples/`, `templates/`, `bench/`): **0** `TODO`/`FIXME`/`HACK`/`XXX` kaydı; `code_size_check --strict` **0** bütçe aşımı, **1** kayıtlı istisna (`Context` üye sayısı, `tools/size_budget.json`).

---

## Alt Sistem Capability Matrisi

### Core, Platform, GPU ve Audio

| Yetenek | Durum | Kaynak Dosyalar | Testler | Çalıştırma Kanıtı | Kod vs ADR / Not |
|---|---|---|---|---|---|
| Log, HandlePool, Clock, Bytes, Math | `implemented` | `modules/core/include/astral/core/*.hpp` | `astral_core_tests` | Debug & Release PASS | `HandlePool` 32-bit generation sarımı |
| JobSystem (yardımlı bekleme, `parallel_for`) | `implemented` | `modules/core/include/astral/core/jobs.hpp` | `astral_core_tests` | Debug & Release PASS | ADR-0004 uyumlu; work-stealing yok (tek kuyruk) |
| Json (metin + ikili) & `ASTRAL_REFLECT` | `implemented` | `modules/core/include/astral/core/json.hpp`, `reflect.hpp` | `astral_core_tests` | Debug & Release PASS | İç içe reflect alanları script'te görünmez |
| CPU Profiler (`ASTRAL_PROFILE_SCOPE`) | `implemented` | `modules/core/include/astral/core/profiler.hpp` | `astral_core_tests`, `astral_app_tests` | Debug & Release PASS | Global state yok |
| SDL3 Pencere, Klavye/Fare, Gamepad, UTF-8 Metin | `implemented` | `modules/platform/include/astral/platform/*.hpp` | `astral_platform_tests` | Debug & Release PASS | **Doküman sapması:** `docs/modules/platform.md` "gamepad/metin yok" diyor, kodda mevcut (M8/M11) |
| Çoklu pencere / dosya diyalogu / IME | `planned` | `modules/platform/include/astral/platform/window.hpp` | — | — | E0 multi-viewport prototipinde değerlendirilecek |
| Vulkan 1.3 RHI (Bindless, BDA, Timeline, Swapchain) | `implemented` | `modules/gpu/include/astral/gpu/*.hpp` | `astral_gpu_tests` | Debug & Release PASS (RTX 3060) | Tek-thread kayıt; kök CMake Vulkan SDK gerektirir |
| Vulkan Ray Tracing (BLAS/TLAS, Ray Query) | `implemented` | `modules/gpu/src/ray_tracing.cpp` | `astral_gpu_tests`, `astral_render_tests` | Debug & Release PASS | BLAS statik (refit/skinned BLAS A0'da) |
| GPU Timestamp Sorguları | `implemented` | `modules/gpu/src/queries.cpp` | `astral_gpu_tests`, `astral-render-bench` | PASS (`timestamps=1`) | **Doküman sapması:** `docs/modules/gpu.md` üst satırda "timestamp yok", altta "M8 Timestamp" yazıyor |
| `Astral::Audio` (miniaudio, 3D, sessiz cihaz) | `implemented` | `modules/audio/include/astral/audio/audio.hpp` | `astral_audio_tests`, `astral_app_tests` | Debug & Release PASS | Streaming dosyadan değil bellek tamponundan okur; efekt/occlusion yok |

### Render ve Shader

| Yetenek | Durum | Kaynak Dosyalar | Testler | Çalıştırma Kanıtı | Kod vs ADR / Not |
|---|---|---|---|---|---|
| RenderGraph (bariyer, havuz, ayıklama) | `implemented` | `modules/render/include/astral/render/render_graph.hpp` | `astral_render_tests` | Debug & Release PASS | ADR-0009/0012 uyumlu |
| Hibrit Deferred + SDF + RT (DDGI, ReSTIR DI, SVGF, TAA, Bloom) | `implemented` | `modules/render/src/passes/*.cpp`, `paths/*.cpp` | `astral_render_tests`, `astral-render-bench` | Unit PASS; `golden-hw` 5/12 PASS, 7/12 FAIL | **F0-TRACK-01:** M13 sonrası lavapipe golden referansları güncel değil |
| M13 Gelişmiş PBR (Clearcoat, Sheen, Aniso, Transmission, SSS, Decal) | `implemented` | `modules/render/shaders/include/astral/bsdf.glsl`, `passes/decal_pass.cpp` | `m13_pbr_test.cpp`, `material_showcase_tests` | Debug & Release PASS | ADR-0033 uyumlu; `m13_pbr_rt` 3.01 ms (RTX 3060) |
| Progresif Path Tracer Referansı | `implemented` | `modules/render/src/passes/path_tracer_pass.cpp`, `shaders/path_tracer.comp` | `m13_pbr_test.cpp`, `astral-render-bench` | PASS (`rt_vs_pt_rmse=2.29`, 0.905 ms/spp) | ADR-0033 uyumlu |
| `.ashader` (`surface`, `sky`, `post`) & `.material` Kalıtımı | `implemented` | `modules/shader/include/astral/shader/*.hpp`, `modules/render/src/surface_shader*.cpp` | `astral_shader_tests`, `shader_showcase_tests` | Debug & Release PASS | `particle`/`volume`/`ui` ayrıştırılır, derlenmez (M14); SPIR-V disk önbelleği yok |
| DebugDraw, SDF Metin, HUD (`UiCanvas`), ImGui `UiLayer` | `implemented` | `modules/render/include/astral/render/debug_draw.hpp`, `text.hpp`, `ui.hpp` | `astral_render_tests`, `devtools_app_test.cpp` | Debug & Release PASS | — |

### World, Scene, Map ve Assets

| Yetenek | Durum | Kaynak Dosyalar | Testler | Çalıştırma Kanıtı | Kod vs ADR / Not |
|---|---|---|---|---|---|
| Archetype ECS, Değişiklik Takibi, `Commands`, `Events` | `implemented` | `modules/world/include/astral/world/world.hpp` | `astral_world_tests` | Debug & Release PASS | `World` aidiyet kimliği (`WorldId`) eksik → F1 |
| Dinamik Component & `EventRegistry` & `TypeRegistry` | `implemented` | `modules/world/include/astral/world/serialize.hpp`, `event_registry.hpp` | `astral_world_tests`, `astral_script_tests` | Debug & Release PASS | Şema sürümü / migration hook eksik → F1 |
| Paralel Transform Yayılımı | `implemented` | `modules/world/include/astral/world/transform.hpp` | `astral_world_tests`, `astral-bench` | PASS (100k entity: 10.9 → 7.09 ms, 1.54×) | Dirty-flag alt ağaç atlama henüz yok |
| Sahne/Prefab (`astral.scene` v1, override, nesting, reload) | `implemented` | `modules/world/include/astral/world/scene.hpp` | `astral_world_tests`, `scene_app_test.cpp` | Debug & Release PASS | ADR-0015 + ADR-0034 (`DocumentEntityId`) |
| Harita/Streaming & Level Instance (`astral.map` v1, `.ent`, `.cell`, `MapLayers`, `MapState`, `LevelInstanceMember`, `CrossCellRef`, Floating Origin) | `implemented` | `modules/world/include/astral/world/map*.hpp`, `level_instance.hpp`, `streaming.hpp`, `origin.hpp` | `astral_world_tests`, `world_app_test.cpp`, `open_world_tests`, `world_editor_tests`, `astral-bench` | Debug & Release PASS (2×2 km E1 kabul + uçuş decode p50 0.279 ms) | ADR-0036: `.scene` tabanlı Level Instance + `CrossCellRef` eklendi; tam `.world`-in-`.world` bilinçli kapsam dışı |
| glTF/GLB Statik Model & PBR Texture Yükleyici | `implemented` | `modules/assets/src/gltf.cpp`, `asset_manager.cpp` | `astral_assets_tests`, `model_viewer_tests` | Debug & Release PASS | `skin`, `animation`, `morph` uyarıyla atlanır → A0 |
| `astral-cook` (AMDL, BC7/BC5, ikili JSON, Luau bytecode) & `.apak` (zstd) | `implemented` | `modules/assets/include/astral/assets/cook.hpp`, `pack.hpp` | `astral_assets_tests`, `astral_cook_cli`, `tests/consumer` | Debug & Release PASS | Memory-map/kısmi okuma ve prefab şablon cook yok |
| Animasyon (`Skeleton`, `Pose`, `Clip`, IK, Skinning) | `planned` | — (`gltf.cpp` uyarıyla atlıyor) | — | — | M20 / A0 |

### Gameplay, Scripting ve Fizik

| Yetenek | Durum | Kaynak Dosyalar | Testler | Çalıştırma Kanıtı | Kod vs ADR / Not |
|---|---|---|---|---|---|
| `FeatureSet` / `FeatureContext` / `feature.json` / `astral-features check` | `implemented` | `modules/app/include/astral/app/feature*.hpp`, `tools/` | `feature_test.cpp`, `*_features_check` | Debug & Release PASS | Reflection + binding tek keşif yüzeyi → F1 |
| `Astral::Script` (Luau 0.748, sandbox, hot reload, `astral.each`, `astral-luau-check`) | `implemented` | `modules/script/include/astral/script/*.hpp` | `astral_script_tests`, `script_app_test.cpp`, `*_script_types` | Debug & Release PASS | `api.cpp` çağrılabilir API metinleri elle yazılıyor; standalone `templates/blank` içinde `*_script_types` kurulmuyor (`F0-TRACK-03`) |
| `App` Paralel Sistem Zamanlayıcısı (`SystemAccess`, `ScopedTick`) | `implemented` | `modules/app/include/astral/app/app.hpp`, `src/app.cpp` | `ecs_app_test.cpp`, `astral-bench` | PASS (100k entity, 30 sistem: 1.63×) | CI TSan adımı bu fazda eklendi (`F0-TRACK-02`) |
| `Context` Gruplu Servisler vs Deprecated Düz Metotlar | `partial` | `modules/app/include/astral/app/app.hpp` | `context_services_test.cpp` | Debug & Release PASS | Deprecated düz forward'lar hâlâ `app.hpp`'de → F1'de temizlenecek |
| `Astral::Physics` (Jolt v5.7.0: RigidBody, Character, 16 katman, cast/overlap + batch, 7 eklem + SixDOF/SwingTwist/motor/kırılma, `ShapeHandle` + `ASSH` v1 cooking, `PhysicsMaterial` tablosu, `FieldWorld`) | `implemented` | `modules/physics/include/astral/physics/*.hpp`, `modules/physics/src/*.cpp` | `astral_physics_tests`, `fields_app_test.cpp`, `physics_playground_tests`, `physics_lab_tests` | Debug & Release PASS | ADR-0038 (P0) tamamlandı |
| App `PhysicsBridge` / `PhysicsService` Sabit Adım, Komut Kuyruğu & Streaming Detach/Attach | `implemented` | `modules/app/include/astral/app/physics_service.hpp`, `src/services/physics_bridge.cpp`, `script_physics.cpp` | `frame_order_test.cpp`, `physics_app_test.cpp`, `physics_step_contract_test.cpp`, `physics_lab_tests` | Debug & Release PASS | ADR-0034 & ADR-0038 uyumlu; Luau/C++ motor/kırılma/batch/teleport ve streaming detach/re-attach doğrulandı |

### Araçlar, Editör ve Sunucu

| Yetenek | Durum | Kaynak Dosyalar | Testler | Çalıştırma Kanıtı | Kod vs ADR / Not |
|---|---|---|---|---|---|
| App İçi ImGui Debug UI (F1, `DebugPanelRegistry`, 7 panel) | `implemented` | `modules/app/src/debug_ui.cpp`, `src/panels/*.cpp` | `debug_panels_test.cpp`, `devtools_app_test.cpp` | Debug & Release PASS | ImGui `v1.92.9` (non-docking); docking E0'da |
| Bağımsız Masaüstü Editör + AI Öneri / Transaction / Undo-Redo & Dünya Kompozisyonu (`WorldAuthoringSession`, Placement Tools) | `implemented` | `modules/editor_core/`, `modules/editor/`, `examples/world_editor/` | `astral_editor_core_tests`, `world_editor_tests`, `editor_isolation_check` | Debug & Release PASS | ADR-0035 & ADR-0036: tek pencere docking, `.world`/`.ent` düzenleme, Level Instance override vs edit-in-context, M15 `IHeightSampler` kancası |
| Headless Sunucu Çalışma Modu (`AppDesc::headless`) | `partial` | `modules/app/include/astral/app/app.hpp` | `hello_headless_tests`, tüm `*_tests` | Debug & Release PASS | Çalışma zamanı GPU/pencere istemez; kök CMake derlemesi Vulkan SDK ister (`modules/gpu/CMakeLists.txt`) |
| Çok Oyunculu Transport / Replikasyon / Dedicated Server Paketi | `planned` | — | — | — | N0 |
