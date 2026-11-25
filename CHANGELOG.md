# Changelog

Biçim: [Keep a Changelog](https://keepachangelog.com/). Sürümler [SemVer](https://semver.org/).

## [Unreleased]

### Fixed (MSR Dalga 1 — docs/status/master-stabilization-roadmap.md)
- `gpu`: başarısız submit sonrası present (A01), `CommandList` move-assign yaşam süresi (G01), açık listelerin kullandığı kaynakların erken serbest bırakılması (G02), AS backing buffer double destroy (G04), BC kopya boyut kontrolü (G05), timestamp aralık taşması (G06), başarısız swapchain yeniden oluşturma (G07), image view/semaphore hataları (G08), başka cihazın listesini submit (G09).
- `assets`: glTF index/aralık/stride kontrolleri, bozuk URI escape, node döngüsü ve çift ebeveyn, ayna matris işareti; AMDL index taşması ve dosyaya sığmayan kayıt sayıları (A11, A12, AS01–AS05).
- `world`: `parallel_each` worker'larına `ScopedTick` taşınması (C01), okuma sırasında aynı olay türünü gönderme (C02), sayı/enum yüklemede aralık kontrolü (C03, C04), move-assign edilemeyen component'lere `add` (C06); başarısız hücre yüklemesi artık Attached sayılmıyor, geri çekilmeyle yeniden deneniyor (A09). **API:** `CellLoader::attach` artık `bool` döner; `StreamingStats::failed_last_update`.
- `script`: callback içinden `astral.on`/`astral.system` kaydı (AG07), hatalı kayıtta ref sızıntısı (AG08), başarısız modülün kalan timer'ları (AG11), kendini içeren/çok derin tablo dönüşümü (AG12).
- `render`: başarısız transient allocation'ın pool'a girmesi (REN02), `GrowBuffer` büyüme hatası (REN03), material offset taşması (REN05), tek boyutlu mip'ler (REN06), à-trous iterasyon sınırı (REN07), mesh index aralığı (REN08). **API:** `RenderSettings::max_denoise_iterations`.
- `physics`: karakter kapsül boyu sözleşmesi (PH09), field noise sayı dönüşümü (PH07).
- `app`/`platform`: silinen entity'nin looping sesi durduruluyor (A10); sol/sağ/orta dışındaki fare tuşları yok sayılıyor (AG05).

### Added (P0 — Physics API v2 & Jolt v5.7.0, ADR-0038)
- `physics`: JoltPhysics **`v5.7.0`** yükseltmesi; dar `PhysicsCapabilities` / `supports()` yetenek sorgusu (sıcak yolda gövde başına sıfır virtual dispatch); `SixDOF` ve `SwingTwist` eklemleri, `JointLimitsDesc` açısal/doğrusal limitler, `JointMotorDesc` (`Off`/`Velocity`/`Position`) ve çalışma zamanı motor/limit setter'ları; `break_force` / `break_torque` kırılma eşiği, `joint_reaction()` ve `broken_joints()` (`JointBrokenEvent`); paylaşımlı `ShapeHandle` + sürümlü `ASSH` v1 ikili shape cooking (`cook_physics_shape`, `inspect_cooked_shape`, `create_shape_from_cooked`); `PhysicsMaterialDesc` / `PhysicsCombineMode` (`Average`/`Min`/`Multiply`/`Max`) yüzey materyal tablosu; paralel toplu sorgular (`raycast_batch`, `shape_cast_batch`, `overlap_batch`); gövde `wake`/`sleep` ve `shift_origin()` sırasında dünya çapalı eklem kaydırma.
- `app` & `script`: `Joint` (7 tip + limit/kırılma), `JointMotor`, `JointBroken` ve `JointDetached` ECS tipleri; `PhysicsService` kuyruğuna motor/kırılma/teleport/wake/sleep komutları ve paralel `raycast_batch`; `DebugShow::physics_joints` debug çizimi; streaming hücre unload/reload için `JointDetached` + otomatik re-attach; Luau `astral.physics.*` bağlamaları (`raycast_batch`, `teleport`, `wake`, `sleep`, `set_joint_motor_*`, `break_joint`) ve `API.md`/`astral.d.luau` 1:1 paritesi.
- Örnekler, testler & benchmark: `examples/physics_lab` kabul sahnesi ve `physics_lab_tests` (motorlu eklemler, kırılma, materyal birleştirme, cooked shape, batched raycast, 120 gövde / 60 eklem stres testi, sabit adım replay determinizmi), `physics_v2_test.cpp`, `physics_step_contract_test.cpp` P0 senaryoları, `bench/main.cpp` `bench_physics_p0` ve `docs/decisions/0038-physics-api-v2.md`.

### Added (E2 — AI-First Native Editör, Offscreen Viewport & Asenkron AI İnceleme, ADR-0037)
- `platform`: `Key` enum sonuna eksik düzenleme/noktalama/Super tuşları, `CursorShape` ve `Window::clipboard_text()` / `set_clipboard_text()` / `set_cursor()` eklendi.
- `render`: Dear ImGui `v1.93.0-docking` güncellemesi; `Renderer::create_render_target()` / `resize_render_target()` / `native_target()` / `render_ui()`, `pack_ui_texture()` / `unpack_ui_texture()`, `ui.frag` içinde sRGB-doku çift gama düzeltmesi (`FLAG_TEXTURE_LINEAR`).
- `app`: `ViewportService` (`Context::viewport()`), `AppDesc::debug_ui_ini_path`; `DebugUi` içinde `#ifdef IMGUI_HAS_DOCK` docking düzeltmesi, `NavEnableKeyboard`, pano/imleç köprüsü ve mantıksal/fiziksel DPI ölçeklemesi.
- `editor_core`: `PersistPolicy::ManualSave`, kirli durum (`is_dirty`), sürükleme birleştirme (`coalesce_key`), dış dosya çakışma algılama (`has_external_conflict`), `.astral_autosave` kurtarma, path-traversal koruması, `WorldAuthoringSession::undo()`/`redo()`, asenkron 7 durumlu `AgentTaskManager` (`AgentPolicy`, seçili komut onayı, revizyon) ve `CodexCliProvider`.
- `editor` & `examples/world_editor`: PIMPL `EditorWorkspace` + modüler `EditorPanel` kayıt defteri (`shell/`, `theme/`, `viewport/`, `services/`, `panels/`), sakin koyu nötr + mat mor `UIStyle` token'ları, **Focus / Advanced** çalışma alanı düzenleri, canlı offscreen **3D Viewport**, CPU ray picking, 3D Translate/Rotate/Scale gizmosu, izole **Play/Pause/Stop** dünyası, bağlamsal reflection Inspector, `Ctrl+K` Komut Paleti, gerçek 3D **Önce/Sonra** AI önizlemesi ve `AstralEditor` hedefi (`--capture`, `--advanced`).
- Dokümanlar & testler: `docs/decisions/0037-ai-first-native-editor-v2.md`, `docs/editor/README.md`, `docs/modules/editor.md`, genişletilmiş `editor_core_test`, `world_authoring_test`, `devtools_app_test` (GPU viewport testi) ve `world_editor_test` (E2 kabul testi).

### Added (E1 — Dünya kompozisyonu ve Level Instance, ADR-0036)
- `world`: `level_instance.hpp` / `level_instance.cpp` — `LevelInstanceMember`, `CrossCellRef`, `LevelInstancePlacement`, `LevelInstanceLimits`, deterministik `level_instance_entity_id()`, `make_level_instance_entity_file()` / `parse_level_instance_entity()`, `expand_level_instance()` / `expand_map_level_instances()`, `set_level_instance_override()`, `register_level_instance_types()`; yerleşim yapay entity parenting'i olmadan asset referans transform'u ile açılır, döngü/derinlik/entity sınırları teşhis edilir.
- `assets` & `app`: `cook_one_world()` (`astral-cook`) ve `MapRuntime::open()` kaynak `.world` açılışında Level Instance'ları tutarlı biçimde genişleterek hücrelere böler.
- `editor_core`: `WorldAuthoringSession` (`astral/editor/world_authoring.hpp`), `.world` + `<harita>.entities/<id>.ent` düzenleme, `IHeightSampler` / `FlatHeightSampler` (M15 arazi kancası), veri güdümlü `PlacementToolDef` (`Road`, `Building`, `Foliage`, `Rock`, `Prefab`, JSON ayrıştırma + spline yerleştirme), açık `edit_instance_override()` vs `edit_source_scene_in_context()`, `inspect_cells()` ve `cook_current_map()`.
- `editor`: `EditorWorkspace::open_world_map()` / `reload_world_preview()` (WorldView önizlemesi oynanabilir `ctx.maps()` sahipliğinden ayrı) ve `World Authoring` ImGui paneli (cook hücreleri, veri katmanları, streaming ownership, floating origin, Level Instance override/edit-in-context, yerleştirme paleti).
- Testler & ADR: `level_instance_test.cpp`, `world_authoring_test.cpp`, `examples/world_editor/tests/world_authoring_e1_test.cpp` (2×2 km harita, 2 bağımsız Level Instance, veri güdümlü yerleştirme, override vs edit-in-context hot-reload birleşimi, cook eşitliği, hücre unload/reload sonrasında `LevelInstanceMember` ve `CrossCellRef` korunumu); `docs/decisions/0036-world-authoring-and-level-instances.md`.

### Added (E0 — Bağımsız AI destekli masaüstü editör çekirdeği, ADR-0035)
- `editor_core` (Katman 3, GPU/pencere/ImGui bağımsız): `EditorDocument`, `DocumentRevision`, atomik `LocalEditorFileSystem` / `IEditorFileSystem`, sürümlü keşfedilebilir `EditorCommand` kataloğu (`build_command_schema`), alan-düzeyi `diff_documents`, `EditorSession` (eski revizyon reddi, kısmi yazma/hata rollback, bayt-bayt undo/redo, `AuditLog` JSONL), `IAgentProvider`, `PrefabAssistantAgent`, geçici izole ECS `run_proposal_preview` ve `ProposalReview`.
- `editor` (Katman 4): `EditorWorkspace` dockable masaüstü çalışma alanı (Dear ImGui `v1.92.9-docking` + `AppDesc::debug_ui_builtin_panels`), Asset Browser, Hierarchy, transactional Inspector, AI Assistant & Diff Review, Console & Profiler.
- `examples/world_editor` + `world_editor_tests`: ajan prefab önerisi → fark ve geçici ECS önizleme → atomik onay → canlı sahne yeniden yükleme → bayt-bayt undo/redo kabul testi.
- `tools/check_editor_isolation.py` (`editor_isolation_check`) ve `tests/consumer/CMakeLists.txt` izolasyon doğrulaması (oyun/sunucu hedefleri editöre bağlanmaz).
- Dokümanlar: `docs/decisions/0035-standalone-ai-assisted-editor-core.md`, `docs/modules/editor.md`, güncellenmiş `tools/module_layers.json` ve `docs/architecture.md`.

### Added (F1 — Gameplay API ve fizik adım sözleşmesi, ADR-0034)
- world: `WorldId`, `WorldEntityRef`, `World::id()/ref()/owns()` ile çalışma zamanı World aidiyeti; `#w<id>:<idx>.<gen>` çapraz-world güvenli canlı referanslar; `DocumentEntityId` (sahne/prefab kalıcı belge adresi); `MapEntityStatus`/`MapEntityLookup` ve `MapState::register_known()/lookup()` (yüklenmemiş/canlı/yeniden yüklenmiş/yok edilmiş durumları).
- world: `TypeRegistry::set_schema_version()` ile component şema sürümü (`$schema_version`), uyumlu okuma, migration kancası ve ileri-sürüm hata teşhisi (`TypeRegistry::save_entry`/`load_entry`).
- script: `ScriptApiCatalog` (`script_api_catalog()`) ve `ApiDiscoverySummary` (`collect_api_discovery()`) tek keşif yüzeyi; `generate_script_api()` ve `api_parity_test.cpp` (Luau VM ↔ katalog ↔ `astral.d.luau` / `API.md` ↔ `TypeRegistry` 1:1 eşitlik testi).
- app: dar `PhysicsService` komut/sorgu/olay arayüzü (`add_impulse`, `add_force`, `set_linear_velocity`, `set_angular_velocity`, `raycast`, `spherecast`, `overlap_*`, `linear_velocity`, `query_state`, `pending_command_count`); `PhysicsBridge` içinde thread-safe ve deterministik sıralı komut kuyruğu (sabit adım sınırında `apply_commands()` + `sync_bodies()` sonrasında uygulanır); `MapService::resolve_map_entity()`.
- `examples/physics_playground`: `playground::ContactStats` + Luau `astral::Contact` ve `TriggerEntered` + kuyruklu `astral.physics.force` testleri; `physics_step_contract_test.cpp` (0/çok alt adım, gecikmiş spawn+komut, geçersiz entity, paralel=seri eşitliği, doğrudan `PhysicsWorld` regresyonu).

### Removed (F1)
- app: `Context` üzerindeki eski `[[deprecated]]` düz servis forward metotları kaldırıldı (tüm tüketiciler gruplu servis erişimcilerini kullanır).

### Added (F0 — Doğrulama ve capability envanteri)
- `docs/status/capabilities.md`: CMake modül grafiği, public API, loader'lar, testler ve ADR sapmalarıyla tam yetenek matrisi.
- `docs/status/f0-verification-report.md`: Windows MSVC Debug/Release (`-Werror`), kurulu paket/tüketici, `templates/blank`, `golden-hw`, `astral-bench` ve `astral-render-bench` (RTX 3060) taban raporu ile 5 takip kaydı (`F0-TRACK-01..05`).
- `bench/main.cpp`: p50/p95/p99 yüzdelikleri, RSS bellek raporu ve 16×16 hücreli gerçek uçuş profilli arka plan streaming worker decode ölçümü.
- `.github/workflows/ci.yml`: `tsan` işine `astral_app_tests` headless paralel zamanlayıcı alt kümesi eklendi.

### Fixed (F0)
- `templates/blank/CMakeLists.txt`: bağımsız kurulum testinde CMake 4.x ve MSVC 14.51 ile kırılan `doctest v2.4.11` → `v2.5.3` ve `DOCTEST_USE_STD_HEADERS=ON` olarak güncellendi.

## [0.17.0] — 2026-11-22 — M12 (fizik altyapısı ve alanlar)

### Added
- physics: çarpışma katmanları (`BodyDesc::layer`, `set_layer_collision`), `QueryFilter`, `shape_cast`, `overlap`, `surface` kimliği, `ContactEvent::sensor`;
  şekiller `convex_hull`, `mesh`, `height_field`, `compound`; eklemler (`create_joint`: Fixed, Point, Hinge, Slider, Distance); `angular_velocity`, `add_force_at`, `add_torque`, `gravity()`.
- physics: `fields.hpp` (`FieldWorld`, `WindDesc`, `WindVolume`, `WaterVolume`, saf işlevler) + `shaders/include/astral/fields.glsl`; CPU/GPU eşitlik testi (`tests/integration`).
- app: `WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil`, `Joint`; olaylar `Contact`, `TriggerEntered`, `TriggerExited`; `Context::fields`; `AppDesc::physics_interpolation`; `debug_show.fields`.
- script: `astral.physics.*`, `astral.fields.*` (`ScriptPhysics`), API belgesi ve tip tanımı güncel.
- `examples/physics_playground` + görüntü testi; ADR-0029; plan `2026-11-22-m12-physics.md`.

### Changed
- `PhysicsWorld::raycast` üçüncü parametresi `QueryFilter` (`BodyHandle` ignore yerine `{.ignore = body}`); `ShapeDesc` yeni türler ve dolgu alanları taşır (sona eklendi).

### Added
- `templates/blank`: boş oyun şablonu (özellik paketi + Luau kuralı + başsız test + paketleme + sınır denetimi); motor içinde ve
  kurulu motora karşı bağımsız derlenir (CI'da ikisi de).
- app: standart oyun kabuğu `parse_shell_args` / `run_game` / `ShellOptions` (`--capture`, `--frames`, `--width`, `--height`, `--script-api`).
- cmake: `astral_check_features(<ad> SOURCES ... MANIFESTS ...)`.
- script: `astral.quat` (axis_angle, mul, rotate, normalize).
- Görüntü testleri: blank, hello_triangle, spinning_cubes, sdf_scene.
- ADR-0027.

### Added (M11 follow-ups, ADR-0028)
- script: `astral-luau-check` (Luau çözümleyicisiyle tip denetimi), `astral_check_scripts` (CTest `<oyun>_script_types`), `astral.d.luau` `keyof`/`index`
  ile adlara göre tipli; `--script-api-only`; Luau bytecode cook (`compile_script`, `astral-cook`); `astral.tween/ease/lerp`, `astral.state_machine`,
  `astral.save/load/has_save` (`ScriptStorage`, `AppDesc::save_folder`).
- platform/app: gamepad (`Input`, SDL3 olayları, `.actions` anahtarları); `fly_camera`, pong ve physics_demo eylemlerine gamepad eşlemesi.
- `features/` ortak özellikler + `tools/sync_features.py` (CTest `shared_features_in_sync`).

### Fixed
- render: ışın izleme gölge/yansıma ışınlarının yüzeyden başlangıç payı sabitti (2 mm); derinlikten yeniden kurulan konumun hatası uzaklıkla
  büyüdüğü için uzak zemin kamera konumuna göre rastgele aydınlık/kendi gölgesinde çıkıyordu (open_world'de kamera 1e-4 m kayınca zemin
  parlaklığı 20 ile 150 arasında zıpladı). Pay artık kameraya uzaklıkla ölçekleniyor (`ray_bias`); `open_world_nudged` görüntü testi eklendi.
  open_world ve pong referans görüntüleri yenilendi.

### Changed
- Bütün örnekler şablon düzeninde yeniden yazıldı (src/ + assets/features/ + tests/): hello_headless (eski hello_core), hello_triangle
  (artık `App` + `.ashader`), spinning_cubes, sdf_scene, physics_demo (özellikler: player, shooter, rain, lifetime), rt_showcase,
  model_viewer, scene_viewer, shader_showcase (fly_camera), pong, open_world (fly_camera, day_night). Her biri `<ad>_tests` ve (özellik
  varsa) `<ad>_features_check` testine sahip.

## [0.16.0] — 2026-11-22 — M11 (oyun kodu ve Luau scripting)

### Added
- world: ham component erişimi (`add_raw/get_raw/has_raw/remove_raw/each_raw`), `FieldInfo`/`describe_fields`
  alan tabloları, `TypeRegistry::add_dynamic` (şemadan component), `live_save_context/live_load_context`,
  `EventRegistry` (adla olay gönder/oku, dinamik olaylar).
- script (yeni modül): `ScriptHost` (Luau 0.748), `astral` API'si (system, behaviour, on, send, query, spawn,
  find, define_component/event, after/every/cancel, random, input, log), sandbox, zaman bütçesi, hot reload,
  `Script` component'i, `generate_script_api` (`astral.d.luau`, `API.md`).
- app: `FeatureSet`/`FeatureContext`/`feature.json`, `check_features` + `astral-features` aracı, `InputActions`
  (`.actions`), `ctx.scripts/events/actions/load_script/load_actions/write_script_api`, script fazları.
- `examples/pong`: üç özellik paketi (`pong_rules` Luau, `pong_scorelights`, `pong_audio`), `pong_features_check`
  testi, `pong --script-api <klasör>`.
- Belgeler: ADR-0026, `modules/script.md`, `AGENTS.md`, rehber "özellik paketleri ve Luau".

### Changed
- `TypeRegistry::Entry` işlevleri `std::function`; `Pong` kuralları C++'tan Luau'ya taşındı
  (`pong::install(ctx)` artık `bool` döner, `Match` bir component, `Ball::velocity` ve AI durumu reflect edilir).

## [0.15.0] — 2026-11-22 — M10 (shader dili ve materyaller)

### Added
- shader (yeni modül): `.ashader` ayrıştırıcısı (`parse_ashader`, uniform ipuçları, varying'ler, `layout_uniforms`),
  `compile_glsl` (glslang 15.4.0, SPIR-V 1.5, `#line` ile hata eşleme).
- render: `compile_surface_shader` / `compile_screen_shader`, `Renderer::create_shader`, `MaterialDesc::shader` /
  `params` / `textures`, `RenderScene::time` / `sky` / `post`, şeffaf (`blend_mix`/`blend_add`) ileri geçiş,
  sky ve post geçişleri, ışın izleme compute shader'larında özel `surface()` (`compile_rt_variants`,
  `Renderer::shader_error`), `.material` çözümü (`resolve_material`, `evaluate_uniforms`).
- gpu: `GraphicsPipelineDesc::additive_blend`.
- app: `MaterialLibrary` (hot reload, son çalışan sürüm), `ctx.load_material`, `ctx.sky`, `ctx.post`,
  `ctx.shader_error`, `PrimitiveMesh::material_file`, F1 → Materials paneli; `TIME` artık
  `ctx.time.elapsed`.
- assets: cook `.ashader`'ı doğrular (hata `dosya:satır`), `.material` ikili JSON olur.
- `examples/shader_showcase` (yaprak, cam, lav, hologram, sky, vinyet), görüntü testi `shader_showcase`.
- Belgeler: `modules/shader.md`, `guides/shaders-and-materials.md`, ADR-0022 "M10'da uygulanan".

### Changed
- Core sürüm testi artık proje sürümünü CMake'ten alıyor.

## [0.14.0] — 2026-11-22 — M9 (dünya ve harita sistemi)

### Added
- world: `map.hpp` (`.world` = `"astral.map"`, `.ent` = `"astral.entity"`, `read_map_source`,
  `assign_cells`, `cook_map`, `read_cell`), `streaming.hpp` (`CellStreamer`, `CellLoader`),
  `map_state.hpp` (`MapLayers`, `MapState`, `CellMember`, `CellDiff`), `origin.hpp` (`FloatingOrigin`,
  `WorldPosition`, `OriginShifted`, `shift_roots`), `save_entities`.
- assets: cook `.world` + `<harita>.entities/` → dizin + `<harita>.cells/*.cell`.
- physics: `PhysicsWorld::shift_origin`.
- app: `ctx.open_world` / `close_world` / `finish_world_loading` / `world_layers` / `world_streaming` /
  `world_origin`, `StreamingSource`, `AppDesc::origin_shift_distance`, `AppDesc::world_stream_budget_ms`,
  F1 → World paneli, `debug_show.world_cells`.
- `tools/make_test_world.py`, `examples/open_world` (4×4 km), görüntü testleri `open_world`,
  `open_world_night`.

### Fixed
- app: `AppDesc::asset_root` geçici bir string'den verildiğinde `save_scene` bozuk yola yazıyordu
  (yol artık App kurulurken kopyalanıyor).
- core: sürüm testi sabit `0.12.0` bekliyordu (0.13.0'da kırıktı); artık CMake proje sürümünü okuyor.

## [0.13.0] — 2026-11-22 — M8 (geliştirici araçları, metin/HUD, ses)

### Added
- M8.1 profiler: `Profiler` / `ASTRAL_PROFILE_SCOPE` (core), GPU timestamp sorguları (gpu), render graph'ta
  pass başına GPU süreleri (`Renderer::gpu_timings`).
- M8.2 debug çizim: `DebugDraw` + `RenderScene::debug_lines`; App'te `ctx.debug`, fizik/ışık katmanları.
- M8.3 debug UI: Dear ImGui 1.92.9 kendi arka ucumuzla (`Astral::ImGui`), F1 paneli (performans, sahne,
  inspector, render ayarları), `Game::on_debug_ui`; world'de `component_to_json` / `component_from_json`.
- M8.4 metin/HUD: SDF font atlası (`Font`, gömülü Roboto, Türkçe), `UiCanvas` (`ctx.hud`), tek 2D katman
  (`UiLayer`); Pong'da ekranda skor ve sonuç.
- M8.5 ses: `Astral::Audio` (miniaudio 0.11.26), `AssetManager::read`, `ctx.audio`, `ctx.sound`,
  `AudioSource` / `AudioListener`; Pong'da `Bounced` olayı, `pong::Sounds`, kodla üretilen sesler.
- M8.6 görüntü testleri: `tests/golden` (`astral-golden` karşılaştırma aracı, toleranslı piksel farkı + fark
  görüntüsü), `astral_add_golden_test`; scene_viewer, rt_showcase ve Pong için referans görüntüler
  (`ctest -L golden`, güncelleme: `ASTRAL_UPDATE_GOLDEN=1`).
- Platform: metin girişi, `executable_directory`. Core: `bytes.hpp`.
- Belgeler: ADR-0019…0024 (geliştirici araçları, ses, dünya/harita sistemi, `.ashader` dili, modüler oyun
  kodu, hacimsel VFX), `guides/scenes-and-worlds.md`, `modules/audio.md`, yol haritası v4.

### Changed
- Yol haritası v4: M9 dünya/harita sistemi, M10 shader dili, M11 modüler oyun kodu + Luau, M12 fizik
  alanları, M14 atmosfer + hacimsel VFX; sonraki fazlar birer kaydı (M22'ye kadar).
- Karakter fiziği planı (ADR-0025, Euphoria tarzı aktif ragdoll): M20'ye IK eklendi, M21 karakter
  fiziğine ayrıldı (ragdoll, kaslar, davranışlar), araçlar/kumaş/soft body yeni M23'e taşındı.

## [0.12.0] — 2026-11-21 — M7 (ilk oyun: 3D Pong)

### Added
- `examples/pong`: 3D Pong; kurallar `pong.hpp/.cpp`'de, saha/raketler/top/skor ışıkları `court.scene` ve
  prefab'larda (prefab varyantı, override'lı örnekler, oyunun kendi reflect edilmiş component'leri).
  `--demo` (AI'ye karşı AI), `--capture`. `pong_tests`: headless, deterministik kural testleri. `pong_package`.

### Fixed
- `astral/astral.hpp` artık `Window` (`ctx.window`) ve sahne API'sini (`load_scene`, `find_by_name`...) içeriyor.

### Changed
- Yol haritası: M7 kılıç arenası yerine 3D Pong; kılıç arenası yer tutucu. Sıradaki M8: ses + metin/HUD.

## [0.11.0] — 2026-11-21 — M6 (paketleme)

### Added
- `astral-cook` (tools/) ve `cook_directory`: glTF → cook edilmiş model (`AMDL`, texture'lar BC7 sRGB / BC7 /
  BC5 + mip zinciri; buffer ve görüntüler içine katılır), `.scene`/`.prefab`/`.json` → ikili JSON, diğerleri
  kopya; içerik hash'li artımlı önbellek (`.astral-cook.json`), silinen kaynakların çıktısı silinir.
- `.apak`: `write_pack`, `Pack` (dosya başına zstd, boyut/hash doğrulaması, thread-safe okuma).
- Assets: `cook_image`, `decompress_image`, `cook_model_images`, `write_cooked_model` / `read_cooked_model`;
  `AssetManagerDesc::pack`, `AssetManager::from_pack`; gevşek klasörde cook edilmiş dosyalar içerikten tanınır;
  BC desteklemeyen GPU'da RGBA8'e açma.
- GPU: `Format::BC5Unorm/BC7Unorm/BC7Srgb`, `Feature::TextureCompressionBC`, `is_compressed`, `texture_data_size`.
- Render: `ImageFormat`, `ImageData::levels` (hazır mip zinciri), public `build_mips`, `Renderer::supports(ImageFormat)`.
- Core: `ByteWriter` / `ByteReader`; `Json::to_binary` / `from_binary` / `is_binary`.
- Platform: `executable_directory()`.
- App: `AppDesc::pack` (göreli = exe'nin yanı; varsa kullanılır, yoksa `asset_root`).
- CMake: `astral_add_game(name SOURCES … ASSETS dir)` + `<name>_package` (exe + `.apak` → `build/packages/<name>/`),
  `ASTRAL_PACKAGE_GAMES`; kurulu pakette `Astral::astral_cook` ve `AstralGame.cmake`.
- `examples/scene_viewer` artık `astral_add_game` ile; sahneye cook edilen texture'lı kasa modeli eklendi.

### Changed
- Normal map'lerin Z'si shader'da X/Y'den kurulur (BC5 için; birim uzunluktaki RGBA8 haritalarda sonuç aynı).

### Fixed
- `tests/consumer` `find_package(Astral 0.5)` istiyordu (`SameMinorVersion` ile 0.10'da bulunamazdı) → 0.11.
- GCC 13 `-O2`'de test yardımcısındaki yanlış pozitif `-Wstringop-overflow` (release `-Werror` derlemesini kırıyordu).

## [0.10.0] — 2026-11-21 — M5c (sahne ve prefab)

### Added
- World: `astral/world/scene.hpp` — `astral.scene` v1 biçimi; `load_scene` / `save_scene` (kalıcı kayıt
  kimlikleri, `SceneId`), prefab örnekleri (`PrefabInstance`, `PrefabPart`) alan bazlı override'larla
  (`components`/`parts` yamaları, `removed`, `deleted`; farklar kayıt anında hesaplanır), iç içe prefab ve
  varyant, örnek içine referans (`"2/5"`), `PrefabLibrary` (şablon önbelleği, `refresh`), `instantiate_prefab`,
  `sync_prefabs` (kodla oluşturulan örnekler; hot-reload override'ları korur), `Name`, `find_by_name`, `find_part`.
- Core: `ASTRAL_REFLECT_ENUM`, `enum_name`, `enum_from_name`; `Json::erase`.
- Assets: `load_document` / `document` (senkron JSON belgeleri, hot-reload, sürüm sayacı).
- App: `ctx.load_scene` / `save_scene` / `spawn_prefab`, `ctx.prefabs`, `PrimitiveMesh` component'i,
  `AppDesc::offscreen` + `App::read_frame`. `SdfPrimitive` / `SdfOperation` / `Primitive` isimle yazılır.
- `examples/scene_viewer` (RGB oda prefab'lı bir `.scene` dosyası olarak; pencere + `--capture`, çalışırken
  sahne ve prefab dosyalarını yeniden yükler).

### Changed
- `SaveContext` / `LoadContext` referansları geri çağrıyla çözüyor (`ref`, `resolve`); `TypeRegistry::Entry`
  `remove` ve `transcode` kazandı. `register_world_types` artık `Name`'i de kaydediyor.
- JSON çıktısı: enum'lar isimle, float'lar en kısa geri dönüşen ondalıkla, kısa sayı dizileri tek satırda.

## [0.9.0] — 2026-11-21 — M5b (asset'ler)

### Added
- `Astral::Assets`: `AssetId` (kalıcı, yol tabanlı), `AssetManager` (asenkron yükleme JobSystem'de,
  ana thread'de GPU aktarımı, `wait_all`, hot-reload), `load_gltf` (.gltf/.glb, metallic-roughness materyaller,
  texture'lar, node hiyerarşisi, eksik normal üretimi), `decode_image`/`load_image` (stb_image).
- Render: texture'lı materyaller (base color, metallic-roughness, türev tabanlı TBN ile normal map, emissive)
  hem raster hem RT vuruşlarında; `create_texture`/`update_texture` (CPU mip zinciri), `update_mesh`.
- GPU: mip başına `write_texture`; materyal sampler'ında anizotropik filtre; örneklenebilir texture'lar
  her zaman yüklenebilir (transfer hedefi).
- App: `ctx.assets`, `ModelRef` component'i (node ağacını entity'lere açar, kaydedilmez parçalar),
  `AppDesc::asset_root`, `hot_reload`. World: `TypeRegistry::exclude<T>()`.
- `examples/model_viewer` (pencere + `--capture`), `tools/make_test_model.py` (prosedürel test kasası).

## [0.8.0] — 2026-11-21 — M5a (ECS tamamlama)

### Added
- Core: `Json` (sıra koruyan, istisnasız, hata konumlu ayrıştırıcı; tam geri dönüşen sayılar),
  `ASTRAL_REFLECT` / `ASTRAL_REFLECT_EMPTY`, `for_each_field`, `stable_hash`.
- World: satır başına eklenme/değişme tick'leri; `query<...>()` builder (`with`, `without`, `added`,
  `changed`, `each`, `parallel_each`, `count`); `tick`/`advance_tick`/`set_change_baseline`/`clear_trackers`;
  olaylar (`send`, `Events<T>`, `EventReader<T>`, `update_events`); `for_each_entity`;
  `TypeRegistry`, `save_world`, `load_world` (entity referansları yeniden eşlenir).
- App: sistem başına değişiklik referansı, kare başı olay yaşlandırma, `ctx.types` (motor component'leri kayıtlı).
- Render/App veri tiplerine reflection (`PointLight`, `RectLight`, `SdfShape`, `Sdf`, `Camera`, `MaterialDesc`…).

### Changed
- Const olmayan erişim (`each<T>`, `get<T>`, değer değiştiren `add`) component'i değişmiş olarak işaretler;
  salt okuma için `const T` kullanın.

## [0.7.0] — 2026-11-21 — Render API v2

### Changed (kırıcı)
- `DrawItem` → `MeshInstance{mesh, material, transform, previous_transform}`; materyal artık satır içi değil,
  paylaşılan `MaterialHandle` (`create_material`, `update_material`, `destroy`, `default_material`).
  `MaterialDesc` glTF metallic-roughness modeli; `emissive` artık renkli ışıma (Vec3).
- App: `MeshRenderer{mesh, material}`; `Context::create_mesh`, `create_material`, `update_material`
  (headless'ta güvenli).

### Added
- Nesne başına hareket vektörleri (G-buffer'da hız hedefi); TAA ve denoiser bunları kullanır.
  App her `MeshRenderer` entity'sinin önceki transform'unu kendiliğinden tutar.
- `Renderer::set_settings` / `settings()`: ray tracing, GI, denoiser vb. çalışma anında değişir;
  raster yolunda oluşturulmuş mesh'lere RT açılınca BLAS kurulur.
- `DebugView::Motion`; ADR-0012 (render API sözleşmesi, ECS/sahne/paketleme planı).

### Fixed
- Sürüm testi 0.6.0 yükseltmesinde güncellenmemişti.

## [0.6.0] — 2026-11-21 — M3.5 (fotogerçekçi render)

### Added
- GPU: ray tracing — `create_blas`, `create_tlas`, `CommandList::build_tlas`,
  `acceleration_structure_address`; shader'lar TLAS'a adresle ulaşır (ray query).
- Render: ray-traced yol ([ADR-0011](docs/decisions/0011-hybrid-ray-traced-lighting.md)) — RT gölgeli
  doğrudan ışık (güneş konisi, ampul yarıçapı, `RectLight` alan ışıkları), DDGI prob ızgarası (sonsuz sekme),
  yakın alan RTGI (temas gölgesi + renk taşması), GGX yansıma ışınları (aynadan pürüzlüye), SVGF tarzı
  denoiser (zamansal + varyans güdümlü à-trous), TAA (Halton jitter, YCoCg kırpma), bloom (CoD:AW/RDR2
  yöntemi), `DebugView`, `Renderer::reset_history()`.
- `RenderSettings`: `ray_tracing`, `gi`, `denoise_iterations`, `max_history`, `taa_blend`, `bloom_strength`,
  `near_gi_radius`, `max_ray_distance`, `debug_view`. `PointLight::source_radius`,
  `DirectionalLight::angular_radius`, `RenderScene::rect_lights`.
- App: `RectLight` component'i, `AppDesc::render`.
- `examples/rt_showcase` (RGB oda; pencere veya `--capture` ile headless görüntü), `docs/research/rage.md`.

### Changed
- RT destekli cihazlarda varsayılan yol ray-traced; `RenderSettings::ray_tracing = false` raster yolu verir.

### Fixed
- Build: modüllerin kendi GLSL include'ları (`scene.glsl` vb.) bağımlılık olarak izlenmiyordu; değişince
  shader'lar yeniden derlenmiyor, C++/GPU veri düzeni sessizce ayrışıyordu. Artık glslc depfile ile izleniyor.

## [0.5.0] — 2026-11-21 — M4

### Added
- `Astral::Physics` (Jolt): kutu/küre/kapsül/silindir; statik/kinematik/dinamik body; sensör; CCD;
  impulse/kuvvet/hız; raycast; temas olayları; karakter kontrolcüsü.
- App: `RigidBody`, `CharacterController` component'leri; `Phase::FixedUpdate`; sabit zaman adımı
  (`physics_rate`, `max_physics_steps`); `ctx.physics`, `ctx.contacts()`, `entity_of(RayHit)`.
- `examples/physics_demo`; kurulan pakette Jolt bağımlılığı.

### Fixed (M4 incelemesi)
- Silinip yeniden oluşturulan body'lerde "temas bitti" olayının yanlış body'ye gitmesi.
- Kopyalanan `RigidBody`/`CharacterController`'ın orijinalin body'sini paylaşması / orijine ışınlanması.
- FixedUpdate system'lerinin temasları hiç görmemesi.
- Karakterlerin raycast ve sensörlere görünmemesi, birbirinin içinden geçmesi.
- Geçersiz boyutlarda Debug'da sessiz çökme; düşük FPS'te kinematik body'lerin sarsıntılı hareketi.

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
