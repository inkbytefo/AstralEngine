# Astral::App

Katman 3. Oyunların yazıldığı katman ([ADR-0006](../decisions/0006-app-layer-for-games.md)).
Rehber: [writing-a-game.md](../guides/writing-a-game.md).

`App` şunları kurar ve sahiplenir: Logger (konsol), JobSystem, World, Input, Clock, Window, Device,
Swapchain, Renderer, hazır mesh'ler. `Game` + `System`'ler bunlara `Context` üzerinden erişir.

## Dosya düzeni (M13.5 R2)

`modules/app/src/app.cpp` yalnızca sahip ve bağlayıcıdır (`App::App` servisleri bağımlılık sırasıyla kurar, `App::~App` belgelenmiş sırada yıkar, `Context` gruplu erişicileri ilgili servis arayüzünü döndürür). Her sorumluluk kendi iç servisinde (`modules/app/src/services/`) ve debug UI panelleri kendi klasöründe (`modules/app/src/panels/`) yaşar; kurucular yalnızca kullandıkları bağımlılıkları alır:

| Dosya | Sorumluluk |
|---|---|
| `include/astral/app/{scene,map,material,audio,script,physics}_service.hpp` | `Context` üzerinden erişilen küçük public servis arayüzleri (`SceneService`, `MapService`, `MaterialService`, `AudioService`, `ScriptService`, `PhysicsService`) |
| `src/app.cpp` | `App` yaşam döngüsü, grafik yığını kurulum/yıkım sırası, `Context` servis bağlamaları |
| `src/services/frame_loop.{hpp,cpp}` | Sıralı kare boru hattı (`run_frame`), sistem fazları, kare saati |
| `src/services/render_bridge.{hpp,cpp}` | `PrimitiveMesh`/`ModelRef` senkronizasyonu, sahne çıkarma, kamera, `MaterialLibrary`, offscreen okuma (`MaterialService`) |
| `src/services/physics_bridge.{hpp,cpp}` | `RigidBody`/`CharacterController`/`Joint` senkronizasyonu, alan kuvvetleri, sabit adım, temas olayları, `ScriptPhysics` (`PhysicsService`) |
| `src/services/audio_bridge.{hpp,cpp}` | `AudioSource`/`AudioListener` senkronizasyonu, ses önbelleği (`AudioService`) |
| `src/services/script_bridge.{hpp,cpp}` | `ScriptHost`, eylem/kayıt bağdaştırıcıları, girdi yönlendirme, script/eylem hot-reload |
| `src/services/world_bridge.{hpp,cpp}` | `MapRuntime` açık dünya streaming'i, kayan orijin, sahne/prefab G/Ç |
| `src/services/dev_tools.{hpp,cpp}` | `Profiler`, `DebugUi`, `DebugPanelRegistry`, varsayılan font, debug katman çizimleri, UI katman birleştirme |
| `src/debug_ui.{hpp,cpp}` | Dear ImGui backend'i, ana menü çubuğu ve açık panelleri çağıran döngü |
| `src/panels/debug_panel.{hpp,cpp}` | `DebugPanel` arayüzü, `DebugPanelRegistry`, yerleşik panel kaydı ve `entity_label` yardımcısı |
| `src/panels/json_editor.{hpp,cpp}` | Reflect edilmiş `Json` alanları için tür başına düzenleyici yardımcılar (`edit_json`) |
| `src/panels/{performance,scene,inspector,render,world,materials,demo}_panel.cpp` | Her biri tek pencereden sorumlu yerleşik debug UI panelleri |
| `src/map_runtime.*`, `src/material_library.*`, `src/input_actions.*` | Köprülerin kullandığı yardımcı alt bileşenler |

## Gruplu `Context` arayüzü ve kaldırma planı

`Context` doğrudan temel alanları (`world`, `input`, `time`, `log`, `jobs`, `screen_size()`, `quit()`, `add_system()`) taşır; servis işlemleri küçük public arayüzler üzerinden gruplanır:

| Erişici | Arayüz | Metotlar |
|---|---|---|
| `ctx.scenes()` | `SceneService` | `load_scene`, `save_scene`, `spawn_prefab` |
| `ctx.maps()` | `MapService` | `open_world`, `close_world`, `finish_world_loading`, `world_layers`, `world_streaming`, `world_origin` |
| `ctx.materials()` | `MaterialService` | `create_mesh`, `create_material`, `update_material`, `load_material`, `shader_error` |
| `ctx.audio_service()` | `AudioService` | `sound` |
| `ctx.script_service()` | `ScriptService` | `load_script`, `write_script_api`, `load_actions` |
| `ctx.physics_service()` | `PhysicsService` | `contacts` |

- Mevcut public işaretçi alanları `ctx.audio` (`AudioEngine*`), `ctx.scripts` (`ScriptHost*`) ve `ctx.physics` (`PhysicsWorld*`) kaynak uyumluluğu için aynı ad ve tiplerle korunur; servis erişicileri `audio_service()`, `script_service()` ve `physics_service()` adlarını kullanır.
- **Kaldırma planı (M14):** Eski düz metotlar (`ctx.load_scene`, `ctx.open_world`, `ctx.create_material`, `ctx.sound`, `ctx.load_script`, `ctx.contacts` vb.) `[[deprecated("use ctx.<group>().<fn>")]]` tek satırlık yönlendiriciler olarak bir faz boyunca korunur ve **M14** başında kaldırılır.

## Debug UI panelleri ve kayıt (`DebugPanelRegistry`)

F1 debug arayüzü artık tek bir büyük çizim fonksiyonu yerine `DebugPanel` arayüzü ve `DevTools`'un sahip olduğu `DebugPanelRegistry` ile çalışır (`modules/app/src/panels/debug_panel.hpp`):

- Her yerleşik pencere (`Performance`, `Scene`, `Inspector`, `Render`, `World`, `Materials`, `ImGui demo`) `DebugPanel` türevi olarak kendi dosyasında yaşar (`name()`, `default_open()`, `draw(Context&, DebugPanelState&, bool& open)`).
- `DevTools` kurucusu yerleşik panelleri `register_builtin_panels(panels_)` ile menü sırasıyla kaydeder; `DebugUi::draw_panels` üst menü çubuğunu ve açık olan tüm panelleri `DebugPanelRegistry::draw_open` döngüsüyle çizer.
- **Özel panel ekleme (iç API):** Oyun/özellik veya motor içi araçlar `DebugPanel` uygulayıp `DevTools::register_panel(std::unique_ptr<DebugPanel>)` ya da `DevTools::panels().add<MyPanel>(...)` ile kendi panelini kaydedebilir (aynı adlı panel varsa yerinde güncellenir; panel otomatik olarak `Windows` menüsünde görünür). Oyun tarafı için public `Game::on_debug_ui(Context&)` kancası olduğu gibi çalışmaya devam eder.

## AppDesc

| Alan | Varsayılan | |
|---|---|---|
| `title`, `width`, `height` | "Astral", 1280×720 | pencere |
| `vsync` | true | |
| `headless` | false | pencere/GPU yok — sunucu ve testler |
| `validation` | false | Vulkan validation (yavaş, hata ayıklama) |
| `fixed_timestep` | 0 | >0 ise her kare tam bu kadar ilerler (deterministik testler) |
| `worker_threads` | 0 | 0 = çekirdek sayısı − 1 |
| `physics_rate` | 60 | fizik adımı / saniye |
| `max_physics_steps` | 4 | kare başına en fazla adım |
| `render` | `{}` | `RenderSettings`: ray tracing, GI, denoiser, TAA, bloom ([render.md](render.md)) |
| `asset_root` | "assets" | gevşek asset klasörü (geliştirme, hot-reload) |
| `pack` | boş | `.apak` (göreli = exe'nin yanında); **varsa** asset'ler ondan okunur, yoksa `asset_root` |
| `hot_reload` | true | gevşek dosyalar değişince yeniden yükle |
| `offscreen` | false | pencere yerine iç görüntüye çiz (`App::read_frame`) |

## Kare

`poll → PreUpdate → on_update → Update → fizik (FixedUpdate system'leri + adım, 0..4 kez) → PostUpdate → update_transforms → render`

Fizik component'leri: `RigidBody`, `CharacterController` ([physics.md](physics.md)).

Kamera: ilk aktif `Camera` component'li entity'nin `WorldTransform`'u (−Z'ye bakar); yoksa varsayılan.

Render component'leri: `MeshRenderer{mesh, material}` (ikisi de paylaşılan kaynak handle'ı;
`ctx.materials().create_mesh`/`ctx.materials().create_material` headless'ta da güvenli; App her entity'nin önceki karedeki
transform'unu kendisi tutar → nesne başına hareket vektörü), `Sdf` (SDF şekli + sıra; konum/dönüş
entity'den), `PointLight` (konum entity'den), `RectLight` (alan ışığı; konum entity'den, eksenleri
entity'nin dönüşü/ölçeğiyle döner — yalnız ray-traced yolda). Güneş ve ortam: `ctx.sun`, `ctx.environment`.

## Yaşam süresi

`run()`/`run_frames()` App başına bir kez. Kapanışta önce system'ler yok edilir (destructor'larında
renderer/GPU kaynağı bırakabilirler), sonra renderer, swapchain, device ve pencere. Küçültülmüş
pencerede döngü işlemciyi yormaz.

## Oyun çatısı (M11)

Karar: [ADR-0023](../decisions/0023-modular-game-code.md), [ADR-0026](../decisions/0026-script-host-and-features.md).
Script modülü: [script.md](script.md).

| Başlık | Ne |
|---|---|
| `feature.hpp` | `FeatureManifest` + `parse_feature_manifest`; `FeatureSet::add(ad, install)` / `install(ctx)` (manifestleri `assets/features/<ad>/feature.json`'dan okur, bağımlılık sırasıyla kurar, döngü/eksik bağımlılık/ad uyuşmazlığı/manifestte olmayan kayıt hata verir, sonra `actions` ve `scripts` dosyalarını yükler); `FeatureContext` (`component<T>()`, `event<T>()`, `system<S>(faz)`, `number/text/flag` ayarları) |
| `feature_check.hpp` | `check_features(kaynaklar, manifestler)`; CLI: `astral-features check <kaynak klasörü> <manifest klasörü>` (Pong'da CTest `pong_features_check`) |
| `input_actions.hpp` | `InputActions`: `.actions` dosyası (`button`, `axis`, `axis2`, `mouse_delta`) → adlandırılmış eylemler; `down/pressed/released/value/axis`. Gamepad: `"gamepad": ["A"]`, `"gamepad_axis": "RightTrigger"`, `"stick": "left"`, `"dpad": true`, `"stick_scale"`; ölü bölge 0.15 |

`Context` script üyeleri: `scripts`, `events` (`EventRegistry`), `actions`, `api_owners` ve `ctx.script_service()` (`load_script(yol)` — hata listesi döner,
dosya değişince yeniden yüklenir; `load_actions(yol)`; `write_script_api(klasör)`).
Script'ler her fazda C++ sistemlerinden sonra çalışır; `Time` script'e `ctx.delta/fixed_delta/time` olarak gider.
Hot reload yalnız geliştirmede (paket yokken) ve `AppDesc::hot_reload` açıkken, 0.5 sn'de bir içerik karşılaştırarak.

Standart oyun kabuğu (`shell.hpp`): `parse_shell_args` + `run_game(options, AppDesc, game, varsayılan_kare)`; `--capture`, `--frames`,
`--width`, `--height`, `--script-api` ortak bayraklar, gerisi `options.has/value/positional` ile oyuna kalır. Capture'da `offscreen` +
sabit 60 Hz adım, yakalamayı üreten GPU adaptör kimliği raporlanır (`ctest -L '^golden$'` / `ctest -L '^golden-hw$'`,
[ADR-0031](../decisions/0031-hardware-independent-golden-tests.md)); pencerede Esc çıkar. `templates/blank` ve bütün örnekler bunu kullanır.

Kurulum örneği (Pong): `FeatureSet game; game.add("pong_rules", install_rules); game.add("pong_audio",
install_audio); if (!game.install(ctx)) ctx.quit();`

## Fizik alanları, kuvvetler ve olaylar (M12)

Karar: [ADR-0029](../decisions/0029-physics-fields-and-joints.md); ayrıntı [physics.md](physics.md#alanlar-ve-kuvvetler-m12). Yeni component'ler (hepsi reflect edilir, sahne dosyalarında ve script'te kullanılır):
`WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil`, `Joint`; olaylar `Contact`, `TriggerEntered`, `TriggerExited` (`ctx.events`'te kayıtlı: `astral.on("astral::TriggerEntered", ...)`).
`Context::fields` (`FieldWorld`): sistemlerden `ctx.fields.wind_at(pos, ctx.time.elapsed)`, `water_at`. `AppDesc::physics_interpolation`. Script'lerin fizik erişimi `ScriptPhysics` arayüzüyle
(App uygular): [script.md](script.md#fizik-ve-alanlar).

## Bilinen sınırlar

- Pencere yeniden boyutlanınca swapchain kendini yeniler; olay callback'i yok.
- Offscreen modda her kare GPU'yu bekler (yakalama için basit; gerçek zamanlı kullanım için değil).
- `PrimitiveMesh` materyalleri önbellekte kalır, hiç silinmez (tanım başına bir materyal).
- Sahne dosyası render kalite ayarlarını taşımaz (GI hacmi mesh'lere kendiliğinden oturur).

## ECS entegrasyonu (M5a)

- Her `System::update` öncesi dünyanın değişiklik referansı o sistemin son çalışma tick'ine ayarlanır:
  sistem içinde `query<...>().changed<T>()` = "bu sistem son çalıştığından beri".
- Kare başında `world.update_events()`; `ctx.world.send(...)` / `EventReader<T>`.
- `ctx.types`: `astral::Transform`, `Parent`, `Camera`, `PointLight`, `RectLight`, `Sdf` kayıtlı;
  oyun kendi component'lerini `ctx.types.add<T>()` ile ekler. `save_world(ctx.world, ctx.types)`.

## Sahneler ve prefab'lar (M5c)

- `ctx.scenes().load_scene("levels/arena.scene")`: sahneyi dünyaya ekler; belgedeki `sun` / `environment` varsa
  `ctx.sun` / `ctx.environment`'ı değiştirir. `ctx.scenes().save_scene(path)`: dünyayı (+ güneş/ortam) asset
  köküne yazar.
- `ctx.scenes().spawn_prefab("enemies/goblin.prefab", Transform{...})`: örneği **hemen** açar (parçalar dönüşte var).
  `world.create(Transform{}, PrefabInstance{...})` da olur; parçalar bir sonraki kare başında gelir.
- Kare başı: `assets->update()` → `prefabs->refresh()` → `sync_prefabs` → `sync_models`. Prefab dosyası
  diskte değişince örnekler override'larıyla yeniden açılır.
- `PrimitiveMesh{shape, material}`: dosyaya yazılabilen mesh. App `MeshRenderer` üretir, aynı tanımlı
  materyalleri paylaşır, component değişince günceller, silinince kaldırır.
- `ctx.types` artık `Name`, `PrimitiveMesh`, `ModelRef` de içeriyor; `ctx.prefabs` hep var.
- `AppDesc::offscreen = true`: pencere yerine `width×height` iç görüntüye çizer, `App::read_frame(rgba)`
  son kareyi verir (yakalama, görüntü testleri). `examples/scene_viewer --capture` bunu kullanır.

## Asset'ler (M5b)

- `ctx.assets` her zaman var; yollar `AppDesc::asset_root`'a göre (varsayılan `assets`), `hot_reload` açık.
- `ModelRef{"props/crate.glb"}`: App modeli yükler, hazır olunca entity'nin altına node ağacını (Transform +
  Parent + MeshRenderer) kurar; model yeniden yüklenip yapısı değişirse yeniden kurar; `ModelRef` silinir
  veya entity yok edilirse parçaları kaldırır. Parçalar kaydedilmez — yalnız `ModelRef` kaydedilir.
- Yükleme asenkron: ilk karelerde model henüz yok olabilir; yükleme ekranı için `ctx.assets->wait_all()`.

## Geliştirici araçları, HUD ve ses (M8)

Kararlar: [ADR-0019](../decisions/0019-dev-tools-and-ui-overlay.md), [ADR-0020](../decisions/0020-audio.md).

| `Context` alanı | Ne |
|---|---|
| `profiler` | CPU kapsam zamanlayıcıları: `ASTRAL_PROFILE_SCOPE(ctx.profiler, "ad")`. App kendi aşamalarını (assets, PreUpdate, Update, physics, PostUpdate, transforms, audio, render, debug ui) ölçer |
| `debug` | `DebugDraw`: çizgi, kutu, küre, kapsül, silindir, ok, eksen, ızgara; `{.on_top, .seconds}` |
| `debug_show` | `physics`, `lights`, `world_cells`: hazır debug katmanları |
| `show_debug_ui` | F1 paneli (Dear ImGui): performans, sahne, inspector, render ayarları |
| `hud` | `UiCanvas`: `rect`, `frame`, `image`, `text` (`TextStyle{size, color, align, shadow}`) |
| `font` | Varsayılan SDF font (gömülü Roboto, Türkçe karakterler) |
| `screen_size()` | HUD için ekran boyutu (piksel) |
| `audio` | `AudioEngine`; headless/offscreen'de sessiz mikser |
| `audio_service().sound(yol)` | Asset yolundan sesi bir kez yükler ([audio.md](audio.md)) |

- `Game::on_debug_ui(ctx)`: oyunun kendi ImGui pencereleri (`#include <imgui.h>`, `Astral::ImGui`).
- `AppDesc::debug_ui` (varsayılan açık; kapalıysa ImGui hiç kurulmaz), `AppDesc::audio`.
- Debug UI açıkken klavye/fare ImGui'ye gidiyorsa oyunun `ctx.input`'u o kareyi görmez; fare yakalama F1
  kapanınca geri gelir.
- Yeni component'ler: `AudioSource`, `AudioListener` (reflect edilmiş, sahne dosyasında kullanılabilir).

## Açık dünya (M9)

Karar: [ADR-0021](../decisions/0021-world-and-level-system.md). Örnek: `examples/open_world`.

| API | Ne |
|---|---|
| `ctx.maps().open_world("maps/x.world")` | Haritayı açar (öncekini kapatır): `always_loaded` nesneler hemen gelir, hücreler kaynakların çevresinde stream edilir. Kaynak harita (geliştirme: `.ent` klasörü bellekte cook edilir) ya da cook edilmiş harita (`x.cells/*.cell`) |
| `ctx.maps().close_world()` | Haritanın nesnelerini siler |
| `ctx.maps().finish_world_loading()` | Menzildeki her hücre yüklenene kadar bekler (yükleme ekranı, testler, yakalamalar) |
| `ctx.maps().world_layers()` | `MapLayers*`: `set("gece", true)`; değişiklik bir sonraki karenin başında uygulanır |
| `ctx.maps().world_streaming()` | `StreamingStats` (yüklü, yükleniyor, bekliyor, son karede eklenen/çıkarılan) |
| `ctx.maps().world_origin()` | Yerel sıfırın dünya konumu (double). Kayınca `OriginShifted` olayı gelir |
| `StreamingSource` | Bu entity'nin çevresi yüklenir (yoksa aktif kamera). İlki kayan orijini de sürer |
| `AppDesc::origin_shift_distance` | Orijin kayma eşiği (varsayılan 2000 m) |
| `AppDesc::world_stream_budget_ms` | Kare başına hücre ekleme süresi (varsayılan 1 ms, en az bir hücre) |
| `debug_show.world_cells`, F1 → World | Yüklü hücreler ve yükleme yarıçapı; orijin, hücre sayıları, katman kutucukları |

Kare sırası: girdi → olaylar → **dünya streaming'i** (orijin kayması, katmanlar, hücre ekle/çıkar) →
asset'ler → sistemler. Hücre dosyaları ana iş parçacığında okunur, JobSystem'de çözülür, ana iş
parçacığında bütçeyle eklenir. Orijin kayınca kök `Transform`'lar, fizik gövdeleri, kinematik geçmişi ve
`RenderHistory` birlikte kayar; renderer'ın zamansal geçmişi sıfırlanır.

**Bilinen sınırlar (M9):** kayan orijin yalnızca bir harita açıkken çalışır. Orijin kayınca TAA/GI
geçmişi bir kare sıfırlanır (kısa bir yumuşama). Hücre okuma (dosya G/Ç) ana iş parçacığında; büyük
hücrelerde G/Ç de iş parçacığına taşınmalı. Gökyüzü arka planı `environment.sky`'ı izlemiyor (M14).

## Materyaller ve shader'lar (M10)

Karar: [ADR-0022](../decisions/0022-shader-and-material-language.md). Örnek: `examples/shader_showcase`.

| API | Ne |
|---|---|
| `ctx.materials().load_material("materials/x.material")` | `.material` (ebeveyn zinciri uygulanmış) + `.ashader`; ilk kullanımda yüklenir, önbelleğe alınır. Geçersiz handle: dosya/derleme hatası (loglanır) ve headless |
| `PrimitiveMesh::material_file` | Doluysa `material` yerine bu dosya |
| `ctx.sky` / `ctx.post` | `shader_type sky` materyali / `shader_type post` materyalleri (sırayla) |
| `ctx.materials().shader_error()` | Özel yüzeyler RT shader'larına derlenemediyse neden |
| F1 → Materials | Uniform ipuçlarından (range, source_color) otomatik kaydırıcılar ve renk seçiciler; değişiklik canlı, dosyaya yazılmaz |

**Hot reload:** `.ashader` ve `.material` dosyaları 0,25 sn'de bir kontrol edilir (paketten okuyorsan
kontrol yok). Materyal handle'ları sabit kalır (yerinde güncellenir). Derlenmeyen düzenleme loglanır
(`dosya:satır`) ve son çalışan sürüm kalır; aynı hata her kontrolde tekrar loglanmaz.
`AppDesc` zamanı: `TIME` = `ctx.time.elapsed`.

**Bilinen sınırlar:** `MaterialOverride` (örnek başına değer) yok: aynı shader'ın farklı değerleri için ayrı
`.material` (ebeveynli) kullan. glTF materyalleri yerleşik standart yolda kalır (`standard.ashader` örneği
olarak yüklenmiyor). Doku dosyaları hot reload edilmez. Shader derleme ana iş parçacığında (yüklemede ve
düzenlemede kısa takılma).
- Tek gamepad (ilk bağlı olan), titreşim yok. Script'ler her karede ve fazda çalışır (profiler'da "scripts" kapsamı); script başına süre
  ayrımı yok. `.actions` ve script dosyalarının hot reload'u içerik karşılaştırır (dosya sistemi izleme yok).
