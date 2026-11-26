# App (`Astral::App`)

> **Olgunluk:** Stabil (çerçeve ve servisler geniş test edilmiş; sınırlar aşağıda) ·
> **Bağımlılıklar:** Astral::Core, Platform, Gpu, World, Script, Render, Assets, Physics, Audio, ImGui (`modules/app/CMakeLists.txt:59-69`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

`Astral::App`, motorun diğer modüllerini (pencere, GPU, renderer, ECS, fizik, ses, asset, Luau) tek bir ana döngüde birleştiren
oyun katmanıdır. Oyun yazan kişi `Game` sınıfından türer, `App::run` ile çalıştırır ve her şeye `Context` üzerinden erişir.
App; sistem zamanlayıcısını, fizik/render/ses/dünya köprülerini, atmosferi, VFX çalışma zamanını, HUD'u ve F1 hata ayıklama arayüzünü sahiplenir.
Bu modül oyun kuralı içermez; oyun kuralı `FeatureSet` paketlerinde (C++/Luau) yaşar. Editör, App'in üstündedir (`Astral::Editor`); App editöre bağlanmaz
(`tools/check_editor_isolation.py`).

## Public API

Hepsi `modules/app/include/astral/app/` altındadır; `astral/astral.hpp` (`modules/app/include/astral/astral.hpp:1`) hepsini tek include'da toplar.

### Çekirdek tipler

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Game` | Oyun taban sınıfı: `on_start`, `on_update`, `on_shutdown`, `on_debug_ui` | `modules/app/include/astral/app/app.hpp:50` |
| `App` | Ana döngünün sahibi. `run(Game&)`, `run_frames(Game&, n)` (test/araç), `context()`, `world()`, `time()`, `read_frame()` | `modules/app/include/astral/app/app.hpp:266` |
| `AppDesc` | Başlatma ayarları (tablo aşağıda) | `modules/app/include/astral/app/app.hpp:215` |
| `Context` | Oyunun/sistemin eriştiği her şey: world, input, time, log, jobs, window/device/renderer, physics, assets, audio, prefabs, profiler, scripts, `debug`, `hud`, `sun`, `environment`, `sky`, `post`, `fields`, `events`, `actions`, servis erişimcileri | `modules/app/include/astral/app/app.hpp:68` |
| `Context::add_system<S>` / `add_system(phase, name, fn, access)` | C++ sistemi kaydı (sınıf ya da lambda) | `modules/app/include/astral/app/app.hpp:133-150` |
| `Context::singleton<T>()` / `singleton_mut<T>()` | İlk `T` bileşenine const / mutable erişim | `modules/app/include/astral/app/app.hpp:153-170` |
| `Context::set_parallel_systems` | Çakışmayan sistemlerin paralel çalışmasını aç/kapat | `modules/app/include/astral/app/app.hpp:193` |
| `Phase`, `System`, `SystemAccess`, `FunctionSystem` | Frame fazları (`PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`) ve okuma/yazma bildirimi | `modules/app/include/astral/app/system.hpp:20`, `modules/app/include/astral/app/system.hpp:24`, `modules/app/include/astral/app/system.hpp:86` |
| `InputScriptEvent` | `AppDesc::input_script` girdisi: frame N başında tuş yukarı/aşağı | `modules/app/include/astral/app/app.hpp:209` |
| `ShellOptions`, `parse_shell_args`, `run_game` | Her oyunun standart `main()`'i: pencere / `--capture` / `--script-api[-only]` | `modules/app/include/astral/app/shell.hpp:26`, `modules/app/include/astral/app/shell.hpp:49` |

### `AppDesc` alanları

| Alan | Varsayılan | Not | Kaynak |
|---|---|---|---|
| `title`, `width`, `height` | "Astral", 1280, 720 | `width/height == 0` ise 1280/720'e döner | `modules/app/include/astral/app/app.hpp:216-218`, `modules/app/src/app.cpp:35-36` |
| `vsync` | true | Swapchain | `modules/app/include/astral/app/app.hpp:219` |
| `headless` | false | Pencere ve GPU yok (sunucu/test); `context.renderer == nullptr` | `modules/app/include/astral/app/app.hpp:220`, `modules/app/src/graphics_backend.cpp:15` |
| `validation` | false | Vulkan validation layer | `modules/app/include/astral/app/app.hpp:221` |
| `fixed_timestep` | 0 | > 0: her frame tam bu kadar ilerler (deterministik yakalama/test) | `modules/app/include/astral/app/app.hpp:222`, `modules/app/src/services/frame_loop.cpp:56` |
| `worker_threads` | 0 | 0 = donanım iş parçacığı − 1 | `modules/app/include/astral/app/app.hpp:223` |
| `physics_rate`, `max_physics_steps` | 60 Hz, 4 | Sabit fizik adımı ve frame başına üst sınır | `modules/app/include/astral/app/app.hpp:224-225` |
| `render` | `RenderSettings{}` | Renderer teknik/kalite ayarları | `modules/app/include/astral/app/app.hpp:226` |
| `asset_root`, `pack`, `hot_reload` | "assets", boş, true | Pack dosyası varsa ondan okunur; yoksa gevşek dosya + hot reload | `modules/app/include/astral/app/app.hpp:227-232`, `modules/app/src/app.cpp:78-88` |
| `offscreen` | false | Pencere yerine iç görüntüye çiz; `read_frame` ile oku (headless ise yok sayılır) | `modules/app/include/astral/app/app.hpp:235`, `modules/app/src/graphics_backend.cpp:16-21` |
| `debug_ui`, `debug_ui_builtin_panels`, `debug_ui_ini_path` | true, true, boş | F1 ImGui arayüzü; yerleşik paneller; düzen dosyası | `modules/app/include/astral/app/app.hpp:237-241` |
| `audio` | true | Headless/offscreen her zaman sessiz karıştırır | `modules/app/include/astral/app/app.hpp:243`, `modules/app/src/app.cpp:235` |
| `origin_shift_distance`, `world_stream_budget_ms` | 2000 m, 1 ms | Açık harita: kayan orijin ve hücre spawn bütçesi | `modules/app/include/astral/app/app.hpp:245-247` |
| `save_folder` | "saves" | `astral.save` slot klasörü | `modules/app/include/astral/app/app.hpp:249` |
| `physics_interpolation` | false | Dinamik cisimleri son iki adım arasında çiz (Transform simülasyondan biraz geride kalır) | `modules/app/include/astral/app/app.hpp:252` |
| `parallel_systems` | true | ADR-0032 paralel dalga zamanlayıcı | `modules/app/include/astral/app/app.hpp:254` |
| `input_script` | boş | Başsız çalışmada betikli girdi (ADR-0046, aşağıda) | `modules/app/include/astral/app/app.hpp:257` |

`sanitize_desc` NaN/negatif/sıfır değerleri düzeltir (`physics_rate <= 0` → 60, `fixed_timestep < 0` → 0, vb.): `modules/app/src/app.cpp:34-43`.

### `Context` servisleri

`Context` alanlarının yanında, birleşik işlemler dar arayüzlerle sunulur (`Context::scenes()` vb.: `modules/app/include/astral/app/app.hpp:173-186`).

| Servis | Ne yapar | Arayüz |
|---|---|---|
| `scenes()` | `load_scene`, `save_scene`, `spawn_prefab` (sahnenin `sun`/`environment` alanları `ctx.sun`/`ctx.environment`'a yazılır) | `modules/app/include/astral/app/scene_service.hpp:11` |
| `maps()` | `open_world` / `close_world` / `finish_world_loading`, veri katmanları, akış istatistiği, `world_origin`, `resolve_map_entity` | `modules/app/include/astral/app/map_service.hpp:12` |
| `materials()` | `create_mesh`, `create_material`, `update_material`, `load_material` (.material, hot reload'da yerinde güncellenir), `shader_error` | `modules/app/include/astral/app/material_service.hpp:11` |
| `audio_service()` | Yol ile önbelleklenen `sound()` | `modules/app/include/astral/app/audio_service.hpp:10` |
| `script_service()` | `load_script`, `write_script_api`, `load_actions` | `modules/app/include/astral/app/script_service.hpp:10` |
| `physics_service()` | Sorgular (raycast, spherecast, overlap, hız) ve sıraya alınan komutlar (impulse, force, `add_force_at`, `add_torque`, `add_angular_impulse`, `set_angular_velocity`, eklem motorları, `break_joint`); sorgular son tamamlanan adımı okur | `modules/app/include/astral/app/physics_service.hpp:28` |
| `viewport()` | Sahneyi ekran dışı bir `TextureHandle`'a çiz, alternatif `World` ve kamera geçersiz kılma (editör için) | `modules/app/include/astral/app/viewport_service.hpp:14` |

Bunlar sanal arayüzlerdir; gerçekleştirmeler `RenderBridge` (`materials()`, `viewport()`), `AudioBridge`, `PhysicsBridge` ve `app.cpp` içindeki `ContextServices`
adaptörleridir (`modules/app/src/app.cpp:99-166`, `modules/app/src/app.cpp:418-437`).

### Bileşenler ve olaylar

`components.hpp` motorun kendi bileşenlerini tanımlar ve `ASTRAL_REFLECT` ile kaydeder; `register_app_types` bunları `ctx.types`'a ekler (`modules/app/src/app.cpp:45-76`).
Başlıcaları: `MeshRenderer`, `PrimitiveMesh`, `Camera`, `Atmosphere`, `Sdf`, `TerrainRef`, `RigidBody`, `CharacterController`, `Joint`, `JointMotor`,
`WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil`, `AudioSource`, `AudioListener`, `StreamingSource`
(`modules/app/include/astral/app/components.hpp:17-262`). Olaylar: `Contact`, `TriggerEntered`, `TriggerExited`, `JointBroken`, `JointDetached`
(`modules/app/include/astral/app/components.hpp:208-244`, kayıt `modules/app/src/app.cpp:70-74`). `Time` (`ctx.time`): `fixed_delta`, `delta`, `elapsed`, `frame`
(`modules/app/include/astral/app/components.hpp:265`).

### FeatureSet

| Tip | Ne yapar | Kaynak |
|---|---|---|
| `FeatureManifest` / `parse_feature_manifest` | `feature.json` (`"format": "astral.feature"`, `version: 1`) modeli ve ayrıştırıcısı | `modules/app/include/astral/app/feature.hpp:30`, `modules/app/src/feature.cpp:30-35` |
| `FeatureContext` | `component<T>()`, `event<T>()`, `system<S>()`, `number/text/flag` ayar okuma; manifestte listelenmeyen kayıt hata sayılır | `modules/app/include/astral/app/feature.hpp:47`, `modules/app/src/feature.cpp:58-64` |
| `FeatureSet` | `add(name, install)`, `install(ctx, folder="features")`, `order(...)` | `modules/app/include/astral/app/feature.hpp:84-99` |
| `check_features` | `astral-features` aracının çekirdeği (bkz. [tools.md](../tools.md)) | `modules/app/include/astral/app/feature_check.hpp:14` |

### Diğer

| Tip | Ne yapar | Kaynak |
|---|---|---|
| `InputActions` | `.actions` dosyası (button / axis / axis2; klavye, fare, gamepad) → adlandırılmış eylemler; betikler `astral.input` ile okur | `modules/app/include/astral/app/input_actions.hpp:32` |
| `play_vfx` / `stop_vfx`, `PlayVfx`, `StopVfx`, `DisperseSmoke` | `.vfx` zaman çizelgesi oynat/durdur (olay ya da doğrudan çağrı) | `modules/app/include/astral/app/vfx.hpp:30`, `modules/app/include/astral/app/vfx.hpp:32`, `modules/app/include/astral/app/vfx.hpp:14`, `modules/app/include/astral/app/vfx.hpp:23` |
| `GameplaySmoke` | Fizik raycast'leriyle engellenen ikili doluluk ızgaralı duman (gameplay görüş engeli) | `modules/app/include/astral/app/gameplay_smoke.hpp:26` |
| `Context::hud` (`UiCanvas`) | Frame başına 2B katman: metin, dikdörtgen, görüntü; frame sonunda temizlenir | `modules/app/include/astral/app/app.hpp:97`, `modules/render/include/astral/render/text.hpp:98` |
| `Context::debug` (`DebugDraw`) | Debug çizgileri; `debug_show` bayrakları fizik/ışık/alan/hücre katmanlarını kendiliğinden çizer | `modules/app/include/astral/app/app.hpp:94`, `modules/app/include/astral/app/app.hpp:103-109` |

## Nasıl çalışır

### Yapı ve sahiplik

`App` yalnızca `Impl`'e işaret eder (`modules/app/include/astral/app/app.hpp:292-293`). `Impl` sırayla Logger, JobSystem, World, ham ve oyun girdisi, `Time`, SPIR-V önbelleği,
`GraphicsBackend`, `AssetManager`, `PrefabLibrary`, `Context`, `SystemScheduler`, `DevTools` ve altı köprüyü (`AudioBridge`, `PhysicsBridge`, `RenderBridge`,
`AtmosphereController`, `VfxRuntime`, `TerrainBridge`, `WorldBridge`, `ScriptBridge`) ile `FrameLoop`'u tutar (`modules/app/src/app.cpp:170-206`).
Yıkım sırası elle yazılmıştır: önce sistemler faz sırasında yok edilir (yıkıcılar GPU/servis kaynağı bırakabilir), sonra köprüler ters sırada, en son GPU
(`modules/app/src/app.cpp:331-359`; test: `modules/app/tests/frame_order_test.cpp`, "teardown destroys systems in phase order…").
Köprü başlıkları `src/` altındadır ve yüklenmez; halka açık yüzey yalnızca yukarıdaki başlıklardır (`cmake/CheckPublicHeaders.cmake` üçüncü parti ad sızıntısını denetler).

### Frame döngüsü sırası

`FrameLoop::run_frame` (`modules/app/src/services/frame_loop.cpp:64-91`) her frame'de şu sırayı izler:

1. `dev_tools.begin_frame()` (profiler), `route_input` (F1 ve ImGui yakalaması, ham → oyun girdisi, eylemler güncellenir: `modules/app/src/services/frame_loop.cpp:93-96`)
2. `world.update_events()` (geçen frame'in olayları bu frame okunabilir, sonra silinir)
3. Dünya akışı (`update_world`: akış kaynakları, kayan orijin kayması, hücre spawn; yalnızca açık harita ya da terrain akışı varsa: `modules/app/src/services/frame_loop.cpp:98-107`)
4. `update_assets_and_scenes`: asset yüklemeleri/hot reload, `.material`/`.ashader` değişiklikleri, **betik hot reload**, prefab senkronu, `ModelRef` parçaları (`modules/app/src/services/frame_loop.cpp:109-116`)
5. `time.delta = delta`, ardından **PreUpdate** (C++ sistemleri, sonra Luau, sonra `apply_commands`) (`modules/app/src/services/frame_loop.cpp:118-123`)
6. **`Game::on_update`** (+ `apply_commands`)
7. **Update** (C++ sonra Luau; Luau zamanlayıcıları ve davranışlar burada çalışır)
8. Terrain akış senkronu
9. **Fizik** (`run_physics`): 0..N sabit adım. Her adımda: alan dünyası yeniden kurulur → **FixedUpdate** (C++ sistemleri, Luau, `vfx.update`, `GameplaySmoke::step`) → `apply_commands` → gövde senkronu → eklem motorları → sıraya alınmış komutlar → kinematik/karakter → `physics.step(fixed)` (`modules/app/src/services/frame_loop.cpp:133-148`, `modules/app/src/services/physics_bridge.cpp:378-425`)
10. **PostUpdate**
11. Atmosfer (`AtmosphereController::update`), dönüşüm yayılımı + terrain LOD + primitif senkronu, ses senkronu
12. Render (debug overlay, ImGui, HUD, `RenderBridge::render_frame`), sonra `debug`/`hud` temizlenir, `time.elapsed`/`frame` ilerler (`modules/app/src/services/frame_loop.cpp:162-185`)

`system.hpp`'deki faz yorumu aynı sırayı özetler (`modules/app/include/astral/app/system.hpp:17-19`). Sıra `modules/app/tests/frame_order_test.cpp`
ile (0 ve 2 sabit adımlı frame'ler için) sabitlenmiştir.

### Sabit adım ve fizik köprüsü

`PhysicsBridge::step` birikimli (accumulator) sabit adım kullanır: `delta` birikir, `fixed = time.fixed_delta` (1/`physics_rate`) ile adımlanır, frame başına en fazla
`max_physics_steps` adım atılır; üst sınıra ulaşınca kalan süre `fixed`'e kırpılarak **atılır** (zaman düşer, frame beklemez)
(`modules/app/src/services/physics_bridge.cpp:385-414`). `Clock` ham delta'yı 0,25 sn'de kırpar (`modules/core/include/astral/core/clock.hpp:40`).
`physics_interpolation` açıksa dinamik cisimler son iki adımın karışımıyla yazılır (`modules/app/src/services/physics_bridge.cpp:424-426`).
Fizik komutları (`add_impulse` vb.) iş parçacığı güvenli sıraya alınır ve adım sınırında uygulanır; sıra, "üretici" sırasına (sistemlerin kayıt sırası) bağlıdır (ADR-0043,
`modules/app/include/astral/app/physics_service.hpp:57-60`). Sorgular son tamamlanan adımı okur (`PhysicsQueryState`:
`modules/app/include/astral/app/physics_service.hpp:16`). Sıfır adımlı frame'de komutlar saklanır (test: `modules/app/tests/physics_step_contract_test.cpp`).

### Sistem zamanlayıcı (ADR-0032)

`SystemScheduler` sistemleri faz başına tutar, **ardışık** ve birbirleriyle çakışmayan (`SystemAccess` yazma/okuma kesişimi yok, `exclusive` değil) sistemleri bir "dalga"
yapıp JobSystem'de paralel çalıştırır; erişim bildirmeyen sistem tek başına ana iş parçacığında koşar (`modules/app/src/services/system_scheduler.cpp:108-128`,
`modules/app/src/services/system_scheduler.cpp:139-160`). Değişiklik algılama (`added<T>`/`changed<T>`) paralel dalgada sistem başına tick ile korunur
(`modules/app/src/services/system_scheduler.cpp:38-59`). Çalışma sırasında eklenen sistemler faz sonuna ertelenir (`modules/app/src/services/system_scheduler.cpp:84-86`, `modules/app/src/services/system_scheduler.cpp:130-137`).

### Girdi betiği (ADR-0046) ve başsız çalışma

`AppDesc::input_script`, App'e kopyalanır ve frame'e göre sıralanır (`modules/app/src/app.cpp:297-299`). Her frame başında `feed_input_script` çalışır; pencere yoksa
(headless/offscreen) App kendisi `input.begin_frame()` çağırır, böylece `pressed/released` kenarları doğru olur (`modules/app/src/app.cpp:199-205`). Test:
`modules/app/tests/input_script_test.cpp`. Karar: [ADR-0046](../decisions/0046-headless-input-script.md).

Başsız (`headless`): pencere/GPU yok, fizik/betik/ses/asset çalışır, `renderer == nullptr`, render kaynağı üreten çağrılar geçersiz tutamaç döner (`modules/app/include/astral/app/material_service.hpp:17`).
Offscreen: GPU var, görüntü iç hedefe çizilir ve `App::read_frame` ile RGBA8 olarak okunur (`modules/app/src/app.cpp:393`, `modules/app/src/services/render_bridge.cpp:235-238`).
`read_frame` **yalnızca offscreen** modda çalışır; pencereli modda `false` döner (hedef/readback yalnızca `offscreen` için oluşturulur: `modules/app/src/services/render_bridge.cpp:161-166`).
`run_game` içinde `--capture` modu: `offscreen = true`, 60 Hz sabit adım, N frame, son görüntüyü `.ppm` yazar (`modules/app/src/shell.cpp:146-176`).

### Shader önbelleği (ADR-0047)

Renderer varsa App derleme servislerini kurar: disk SPIR-V önbelleği + iş sistemi (paralel derleme), açılışta 512 MiB'a kadar budanır
(`modules/app/src/app.cpp:211-219`). Klasör (`shader_cache_directory`): `ASTRAL_SHADER_CACHE=off|0` → önbellek yok; `ASTRAL_SHADER_CACHE=<klasör>` → o klasör;
aksi halde `%LOCALAPPDATA%` / `$XDG_CACHE_HOME` / `~/.cache` / temp altında `AstralEngine/shader-cache/<oyun başlığı>` (başlık harf/rakam/`-`/`_`'e indirgenir)
(`modules/app/src/shader_cache_location.cpp:47-53`, `modules/app/src/shader_cache_location.cpp:26-34`). Giriş içerik-adresli olduğu için oyunlar klasör paylaşsa da yalnızca isabet
paylaşırlar (`modules/app/src/shader_cache_location.hpp:22`). Ayrıca `Renderer::set_busy_callback` + `Window::keep_responsive` ile uzun yüklemede pencere ≤50 ms'de bir
işletim sistemine yanıt verir (`modules/app/src/app.cpp:221-232`). Testler için ctest ortamı bu değişkeni derleme klasörüne yönlendirir (bkz. [tools.md](../tools.md)).
Karar: [ADR-0047](../decisions/0047-spirv-disk-cache.md).

### Atmosfer (G1.1 sanatsal alanlar dahil)

İlk (en düşük entity indeksli) **etkin** `Atmosphere` bileşeni güneşi ve ortam gökyüzünü sürer (`modules/app/src/services/atmosphere.cpp:78-84`): saat (`hours_per_second` ile akar),
eğim, yön; fiziksel model hava kütlesinden güneş rengi/yoğunluğu hesaplar (`modules/app/src/services/atmosphere.cpp:22-56`). G1.1 alanları: `twilight_color`/`twilight_strength` (ufuk civarında
gökyüzüne karışır: etki güneş yüksekliği −0,05 çevresinde tepe yapar, ±0,25 genişlikte söner), `horizon_tint` (alçak güneşte güneş rengini çarpar), `sky_tint` (gökyüzünü her zaman çarpar), `drive_sun` / `drive_sky` (false: o alanı oyuna bırakır)
(`modules/app/include/astral/app/components.hpp:46-61`, `modules/app/src/services/atmosphere.cpp:43-54`, `modules/app/src/services/atmosphere.cpp:104-116`). Varsayılanlar saf fizik sonucunu değiştirmez (test:
`modules/app/tests/atmosphere_test.cpp`, "G1.1 default artistic controls…"). Atmosfer devre dışı kalınca yazarın ışığı geri yüklenir (`modules/app/src/services/atmosphere.cpp:83-94`); bu geri yükleme yalnızca son
sürülen kısım (`drove_sun_`/`drove_sky_`) için yapılır.

### VFX çalışma zamanı

`.vfx` (`"format": "astral.vfx"`, `version: 1`) zaman çizelgesidir: en çok 32 iz; iz türleri `smoke`, `volume`, `particles`, `light`, `decal`, `sound`
(`modules/app/src/services/vfx_runtime.cpp:17-25`, `modules/app/src/services/vfx_runtime.cpp:57-98`). `play` bir `VfxPlayback` varlığı yaratır (aynı anda en çok 64), iz başlangıçları
`start`'a göre sıralı çocuk varlıklara dönüşür; `age >= end` olunca oyun biter ve çocuklarla birlikte silinir — **döngü yoktur** (`modules/app/src/services/vfx_runtime.cpp:145-175`, `modules/app/src/services/vfx_runtime.cpp:210-243`).
Şablon, belge sürümüne göre önbelleklenir; dosya hot reload ile değişince yeniden ayrıştırılır (`modules/app/src/services/vfx_runtime.cpp:156-174`). VFX zamanı **sabit adımda** ilerler
(`FrameLoop::run_fixed_update` içinde `vfx_.update`: `modules/app/src/services/frame_loop.cpp:137`), yani `physics_rate` düşükse çözünürlük düşer ve sıfır adımlı frame'de ilerlemez.
Luau tarafından `PlayVfx`/`StopVfx` olayları gönderilir (`modules/app/src/services/vfx_runtime.cpp:194-204`).

### HUD ve F1 hata ayıklama arayüzü

`ctx.hud` (`UiCanvas`) 3B görüntünün üstünde, debug arayüzünün altında çizilir; her frame sonunda temizlenir
(`modules/app/src/services/dev_tools.cpp:82-87`, `modules/app/src/services/frame_loop.cpp:181-182`). `Context::font` yerleşik Roboto Medium'dur (Türkçe harfler dahil;
`modules/app/include/astral/app/app.hpp:98-99`); headless'ta tek bir paylaşılan Font kullanılır (`modules/app/src/services/dev_tools.cpp:15-20`).
F1 ImGui arayüzünü açar/kapar; açıkken ImGui'nin yakaladığı klavye/fare oyun girdisinden silinir (`modules/app/src/services/dev_tools.cpp:44-59`). Yerleşik paneller: Performance, Scene, Inspector, Render, World, Materials, Demo
(`modules/app/src/panels/debug_panel.cpp:77-85`); oyun kendi paneli için `Game::on_debug_ui` içinde `<imgui.h>` kullanır (ImGui türleri motorun public başlıklarında yoktur). Arayüz
yalnızca renderer varsa ve `debug_ui` açıksa kurulur; docking ve klavye gezintisi açıktır (`modules/app/src/debug_ui.cpp:134-136`). Editör bu arayüzü
`debug_ui_builtin_panels = false` ile tüm dockspace'i kendi panelleriyle doldurmak için kullanır.

## Güçlü yanlar

- **Net, test edilmiş frame sırası.** Sıra tek dosyada (`modules/app/src/services/frame_loop.cpp:64-91`) ve `frame_order_test.cpp` ile 0 ve 2 sabit adım için sabitlenmiş; yıkım sırası da testli.
- **Sıfır GPU ile tam çalışma.** `headless` modda fizik, betik, ses, asset, dünya akışı, HUD yerleşimi çalışır; 30 test dosyasının çoğu başsızdır (`modules/app/CMakeLists.txt:71-105`).
- **Deterministik yakalama.** `fixed_timestep` + `--capture` + `input_script` ile tekrarlanabilir görüntü/test üretilir (`modules/app/src/shell.cpp:162-166`).
- **Fizik komutları deterministik.** Üretici sırası paralel/seri sistemlerde aynı sonucu verir (`modules/app/tests/physics_angular_test.cpp`, `physics_step_contract_test.cpp`).
- **Dar servis arayüzleri.** `Context`'in sahneyi/haritayı/fiziği/betiği ilgilendiren işleri saf sanal arayüzlerde; test ve editör kendi uygulamasını verebilir (`modules/app/tests/context_services_test.cpp`).
- **Özellik paketi sınırı makine ile doğrulanır.** Manifest "API"dir: listelenmeyen bileşen/olay kaydı hata; bağımlılık döngüsü/eksik bağımlılık adlandırılmış hata (`modules/app/src/feature.cpp:58-64`, `modules/app/src/feature.cpp:86-129`).
- **Başarısız yeniden yükleme güvenli.** Hatalı betik/`.material` düzenlemesi son iyi sürümü yerinde bırakır (`modules/app/tests/script_app_test.cpp`, `material_app_test.cpp`).

## Zayıf yanlar ve bilinen sınırlar

- **Girdi eylemlerinde fare tekerleği ekseni yok** (vitrin yazımında gözlendi, 2026-11-27; `examples/asset_viewer` yakınlaştırmayı C++ sistemiyle yapar). `AppDesc::input_script` yalnız tuş enjekte eder; gamepad enjekte edilemez.
- **Boyut bütçesi doygun.** `AppDesc` 25/25 ve `App::Impl` 25/25 veri üyesi (sınır 25): yeni alan eklemek önce yapı bölmeyi gerektirir. `Context` 31 üyeyle açık bir istisna taşır (`tools/size_budget.json:3-7`;
  `python tools/check_sizes.py` çıktısı: `app.hpp:215 AppDesc (25 of 25)`, `app.cpp:170 App::Impl (25 of 25)`, `Context (31 > 25)`). `Context`, 31 genel alanıyla hâlâ "her şeye erişim" nesnesidir
  (gerekçe: "kaynak uyumluluğu için M14'e kadar", `tools/size_budget.json:6`).
- **`read_frame` yalnızca offscreen.** Pencereli modda ekran görüntüsü alma API'si yok (`modules/app/src/services/render_bridge.cpp:235-238`).
- **VFX döngüsüz ve sabit adıma bağlı.** Süresiz/döngülü efekt tanımlanamaz (`age >= end` → silinir); `physics_rate` ve sıfır adımlı frame'ler VFX ilerlemesini etkiler; aynı anda 64 oynatma ve 32 iz sınırı sabit kodlu
  (`modules/app/src/services/vfx_runtime.cpp:61`, `modules/app/src/services/vfx_runtime.cpp:146`, `modules/app/src/services/frame_loop.cpp:137`).
- **Zamanlayıcı "açgözlü ardışık dalga" kurar.** Erişimi bildirmeyen ya da `exclusive` bir sistem tüm ardışık paralelliği böler; farklı yerlerdeki çakışmayan sistemler yeniden sıralanmaz
  (`modules/app/src/services/system_scheduler.cpp:108-128`). Sistem erişimini doğru bildirmek geliştiricinin sorumluluğundadır; yanlış bildirim yarış üretir ve derleyici/çalışma zamanı denetlemez
  (yalnızca TSan CI işi bazı App testlerini koşturur: `../../.github/workflows/ci.yml:107-108`; Windows'ta tsan/asan koşturulmadı).
- **GPU'lu testler GPU yoksa sessizce "geçer".** Örn. `if (!app.context().renderer) return; // no GPU` (`modules/app/tests/devtools_app_test.cpp:102`): test başarılı görünür ama hiçbir şeyi doğrulamamış olur.
- **Shader önbelleği için App düzeyinde test yok.** Yalnızca klasör çözümleme birim testleri var (`modules/app/tests/shader_cache_location_test.cpp`); `setup_shader_compiles` bağlantısı (`modules/app/src/app.cpp:211-219`) ve budama doğrudan test edilmiyor.
  `ViewportService` (editör yüzeyi) App testlerinde de doğrudan kapsanmıyor; yalnızca `examples/world_editor/tests` dolaylı kullanır.
- **`FeatureSet::install` geri alma yapmaz.** Bir özellik hata verirse önceki özelliklerin kayıtları (bileşen/olay/betik) yerinde kalır; `install` ilk hatada `false` döner ve oyun `ctx.quit()` etmelidir
  (`modules/app/src/feature.cpp:154-168`, yorum `modules/app/include/astral/app/feature.hpp:9`). `FeatureContext::declare` yalnızca `component<T>()`/`event<T>()` üzerinden denetler; `ctx.types.add<T>()` ile doğrudan kayıt manifest denetimini aşar.
- **Hot reload yoklaması.** Betik/actions izleyicisi her 0,5 sn'de izlenen her dosyayı okuyup içerik özeti karşılaştırır, ana iş parçacığında (`modules/app/src/services/script_bridge.cpp:225-240`).
- **`Context` iş parçacığı sözleşmesi örtük.** Paralel sistemlerde `ctx.world` dışındaki alanlara (ör. `ctx.debug`, `ctx.hud`, `ctx.sun`) yazmanın güvenli olup olmadığı belgelenmemiş; yalnızca World bileşen erişimi ve fizik/komut kuyruğu paralel için tasarlanmış (doğrulanmadı).
- **Üçüncü parti tür sızıntısı denetimi metin aramasıdır.** `CheckPublicHeaders.cmake` yasaklı alt dizgeleri (`Jolt/`, `vulkan/`, `SDL_`…) arar; ImGui adı listede yok (ImGui, `Astral::App`'in PUBLIC bağımlılığıdır: `modules/app/CMakeLists.txt:69`; public başlıklarda ImGui türü bulunmaması yalnızca kurala dayanır).

## Testler

`modules/app/tests/` altında 30 kaynak dosya, tek yürütülebilir `astral_app_tests` (zaman aşımı 300 sn: `modules/app/CMakeLists.txt:71-107`).

| Alan | Dosya |
|---|---|
| Yaşam döngüsü, fazlar, zaman | `app_test.cpp`, `frame_order_test.cpp` |
| ECS, zamanlayıcı, paralel eşdeğerlik | `ecs_app_test.cpp`, `refactor_regression_test.cpp` |
| Servisler ve köprüler | `context_services_test.cpp`, `services_test.cpp` |
| Fizik (adım sözleşmesi, açısal komutlar, alanlar, eklemler) | `physics_app_test.cpp`, `physics_step_contract_test.cpp`, `physics_angular_test.cpp`, `fields_app_test.cpp` |
| Sahne/harita/terrain/asset/pack | `scene_app_test.cpp`, `world_app_test.cpp`, `terrain_app_test.cpp`, `assets_app_test.cpp`, `pack_app_test.cpp` |
| Betik, eylemler, girdi betiği | `script_app_test.cpp`, `actions_test.cpp`, `actions_crlf_test.cpp`, `input_script_test.cpp` |
| Atmosfer, VFX, duman, malzeme, ses | `atmosphere_test.cpp`, `vfx_runtime_test.cpp`, `gameplay_smoke_test.cpp`, `material_app_test.cpp`, `audio_app_test.cpp` |
| Debug UI, HUD, profil | `devtools_app_test.cpp`, `debug_panels_test.cpp` |
| FeatureSet, kabuk, önbellek konumu | `feature_test.cpp`, `shell_test.cpp`, `shader_cache_location_test.cpp` |

CI'daki TSan işi yalnızca `ecs_app_test`, `frame_order_test`, `physics_app_test`, `world_app_test`'i koşturur (`../../.github/workflows/ci.yml:107-108`). Bu PC'deki durum (RTX 3060, MSVC 19.51, Debug+Release):
`astral_app_tests` geçer; paralel yük altında yalnızca `pong_tests` ve `spinning_cubes_tests` (örnek oyunlar, Luau 100 ms bütçesi) kararsız (ana ajan ölçümü, bkz. [script.md](script.md)).
**Kapsanmayanlar:** pencereli `run()` döngüsü (küçültme uykusu `modules/app/src/app.cpp:378-381`), Vulkan doğrulama katmanı, `ViewportService`, SPIR-V önbelleğinin App içindeki bağlantısı, çok pencereli/pencere-yeniden-boyutlama davranışı.

## Performans

Ölçülmedi (bu belge için App'e özgü mikro-ölçüm yok). Ana ajanın verdiği ölçümler (kaynak: depoda kayıtlı değil; `examples/the_architect`, `--capture --frames 4`, RTX 3060): SPIR-V önbelleği
öncesi Debug 19,1 sn, Release 4,18 sn; sonrası Debug soğuk 11,2 / sıcak 3,95 sn, Release soğuk 2,53 / sıcak 1,76 sn. Kalan maliyet: cihaz+pencere 0,3–0,6 sn, DevTools/ImGui (Release 0,2 sn / Debug 0,65 sn), ardışık doku çözme ve yükleme.

## Kararlar

- [ADR-0006 Oyunlar için App katmanı](../decisions/0006-app-layer-for-games.md)
- [ADR-0017 Oyun betikleme](../decisions/0017-gameplay-scripting.md), [ADR-0026](../decisions/0026-script-host-and-features.md)
- [ADR-0019 Geliştirici araçları ve UI katmanı](../decisions/0019-dev-tools-and-ui-overlay.md)
- [ADR-0021 Dünya ve seviye sistemi](../decisions/0021-world-and-level-system.md)
- [ADR-0023 Modüler oyun kodu (FeatureSet)](../decisions/0023-modular-game-code.md)
- [ADR-0027 Boş şablon ve oyun kabuğu](../decisions/0027-blank-template-and-game-shell.md)
- [ADR-0030 Refaktör kuralları ve boyut bütçesi](../decisions/0030-refactor-rules-and-size-budget.md)
- [ADR-0032 Ölçeklenebilir çalışma zamanı ve paralel zamanlayıcı](../decisions/0032-scalable-runtime-and-parallel-scheduler.md)
- [ADR-0034 Gameplay API ve fizik adım sözleşmesi](../decisions/0034-gameplay-api-and-physics-step-contract.md)
- [ADR-0043 Deterministik fizik komut üreticileri](../decisions/0043-deterministic-physics-command-producers.md)
- [ADR-0045 Sorgularda birden çok yok sayılan gövde](../decisions/0045-query-filter-ignore-list.md)
- [ADR-0046 Başsız çalışmada betikli girdi](../decisions/0046-headless-input-script.md)
- [ADR-0047 SPIR-V disk önbelleği](../decisions/0047-spirv-disk-cache.md)
- [ADR-0048 Yükleme sırasında pencere tepkisi](../decisions/0048-startup-responsiveness-and-mips.md)
