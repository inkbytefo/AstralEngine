# Changelog

Biçim: [Keep a Changelog](https://keepachangelog.com/). Sürümler [SemVer](https://semver.org/).

## [Unreleased]

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
