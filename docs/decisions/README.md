# Mimari Karar Kayıtları (ADR)

Her önemli karar bir dosya: bağlam, karar, sonuçlar. Kararlar silinmez; değişirse yeni
ADR eskisini "yerini aldı" diye işaretler.

| No | Karar | Durum |
|---|---|---|
| [0001](0001-library-not-framework.md) | Motor bir kütüphane, oyun main loop'un sahibi | Kabul |
| [0002](0002-module-per-folder.md) | Her modül kendi klasöründe, `include/astral/<modül>/` | Kabul |
| [0003](0003-vulkan-only-own-rhi.md) | Kendi RHI'mız, ilk ve tek backend Vulkan 1.3 | Kabul |
| [0004](0004-helping-wait-own-group.md) | Yardımlı `wait()` sadece kendi grubunu çalıştırır | Kabul |
| [0005](0005-own-ecs.md) | Kendi ECS'imiz; EnTT kullanılmaz | Kabul |
| [0006](0006-app-layer-for-games.md) | Oyunlar App + Game + System ile yazılır | Kabul |
| [0007](0007-sdl3-platform.md) | Platform katmanı SDL3 | Kabul |
| [0008](0008-bindless-bda-glsl.md) | Bindless texture, buffer adresleri, GLSL | Kabul |
| [0009](0009-render-graph-hybrid-deferred.md) | Render graph üzerinde hibrit deferred renderer | Kabul |
| [0010](0010-jolt-physics.md) | Fizik Jolt üzerine, ECS'ten bağımsız modül | Kabul |
| [0011](0011-hybrid-ray-traced-lighting.md) | Donanımsal ray query ile hibrit fotogerçekçi aydınlatma | Kabul |
| [0012](0012-render-api-contract.md) | Render API sözleşmesi; ECS, sahne ve paketleme yönü | Kabul |
| [0013](0013-ecs-change-detection-reflection.md) | Değişiklik takibi, reflection ve JSON serileştirme | Kabul |
| [0014](0014-assets.md) | Asset sistemi — AssetId, iki katmanlı yükleme, yerinde hot-reload | Kabul |
| [0015](0015-scenes-and-prefabs.md) | Sahneler ve prefab'lar — kalıcı kimlikler, fark olarak override, iç içe/varyant | Kabul |
| [0016](0016-packaging.md) | Paketleme — aynı yolla cook, kullanıma göre BC7/BC5, artımlı önbellek, `.apak`, `astral_add_game` | Kabul |
| [0017](0017-gameplay-scripting.md) | Oyun mantığı ve scripting — C++ motor + Luau kurallar, reflection'dan bağlama, dinamik component'ler | Kabul |
| [0018](0018-terrain-and-sdf-world.md) | Arazi: heightfield (Terrain3D benzeri) + seyrek SDF hacimleri; aynı SDF altyapısıyla yıkım | Kabul |
| [0019](0019-dev-tools-and-ui-overlay.md) | Geliştirici araçları: global olmayan profiler, pass başına GPU zamanı, veri olarak debug çizim, tek 2D katman (ImGui + HUD), SDF font | Kabul |
| [0020](0020-audio.md) | Ses: miniaudio üstünde `Astral::Audio`, sessiz cihaz, `AudioSource`/`AudioListener` component'leri | Kabul |
| [0021](0021-world-and-level-system.md) | Dünya ve harita: `.world`, nesne başına dosya, hücreler ve streaming, veri katmanları, bölüm örnekleri, kayan orijin | Kabul |
| [0022](0022-shader-and-material-language.md) | Shader dili `.ashader` (gdshader benzeri) ve `.material`; tek shader → raster, RT, gölge, forward | Kabul |
| [0023](0023-modular-game-code.md) | Modüler oyun kodu: özellik paketleri, SOLID kuralları, yapay zekâ dostu düzen | Kabul |
| [0024](0024-volumetric-vfx.md) | VFX: CS2 tarzı hacimsel duman (deterministik ızgara + GPU görüntü hacmi), GPU partikülleri, `.vfx` | Kabul |
| [0025](0025-physics-based-character-animation.md) | Fizik tabanlı karakter animasyonu: iskelet, IK, kaslar, aktif ragdoll, Euphoria tarzı davranışlar | Kabul |
| [0026](0026-script-host-and-features.md) | Script ana bilgisayarı, dinamik component'ler, olay kaydı ve özellik paketlerinin uygulanışı (M11 sapmaları) | Kabul |
| [0027](0027-blank-template-and-game-shell.md) | Boş oyun şablonu (`templates/blank`), standart oyun kabuğu (`run_game`), örneklerin şablon düzeninde yeniden kurulması | Kabul |
| [0028](0028-script-tooling-and-shared-features.md) | Script araçları (tip denetimi, bytecode cook), ortak özellikler, gamepad, tween/durum makinesi/kayıt | Kabul |
| [0029](0029-physics-fields-and-joints.md) | Fizik altyapısı: katmanlar, yeni şekiller, eklemler, rüzgâr/su alanları (CPU+GPU aynı işlev), kuvvetler, tetik olayları | Kabul |
| [0030](0030-refactor-rules-and-size-budget.md) | Refaktör kuralları ve boyut bütçesi: 600/80/25 sınırı, tek sorumluluk ölçütü, tutarlı ayrıştırıcı istisnası, karakterizasyon testleri, bayt bayt golden kapısı | Kabul |
| [0031](0031-hardware-independent-golden-tests.md) | Donanımdan bağımsız görüntü testleri: `golden` (lavapipe) / `golden-hw` (SSIM + güçlü piksel sınırı), cihaz kimliği, paralel kilit | Kabul |
| [0032](0032-scalable-runtime-and-parallel-scheduler.md) | Ölçeklenebilir çalışma zamanı: erişim bildiren sistemler, paralel zamanlayıcı, paralel transform ve G/Ç ayrımı (M13.6) | Kabul |
| [0033](0033-advanced-pbr-and-path-tracer.md) | Gelişmiş PBR (multi-scattering GGX, clearcoat, sheen, anizotropi, transmission, SSS, ıslaklık, POM, decal), `.ashader` `shading_model` ve Path Tracer referans modu (M13) | Kabul |
| [0034](0034-gameplay-api-and-physics-step-contract.md) | Gameplay API keşif yüzeyi, kimlik sözleşmesi ve fizik adım kuyruğu (F1) | Kabul |
| [0035](0035-standalone-ai-assisted-editor-core.md) | Bağımsız AI destekli masaüstü editör çekirdeği (E0) | Kabul |
| [0036](0036-world-authoring-and-level-instances.md) | Dünya yazarlığı, Level Instance sahipliği, veri güdümlü yerleştirme (E1) | Kabul |
| [0037](0037-ai-first-native-editor-v2.md) | AI-first native editör mimarisi, viewport hedefi, inceleme sözleşmesi (E2) | Kabul |
| [0038](0038-physics-api-v2.md) | Physics API v2: SixDOF, motorlar, kırılma, batched query (P0) | Kabul |
| [0039](0039-terrain-foundation.md) | Terrain temeli — tile heightfield, CPU mesh LOD, Jolt ve streaming | Kabul |
| [0040](0040-dropped-command-list-layout-fixups.md) | Bırakılan CommandList'lerin layout geçişi için fix-up barrier (MSR G03) | Kabul |
| [0041](0041-lossless-json-integers.md) | `Json`'da kayıpsız 64-bit tamsayı: int64/uint64 iç temsil, AJB `TagUInt64`, 8-byte field'lar (MSR C05) | Kabul |
| [0042](0042-typed-shader-parameter-words.md) | Shader parametrelerinde tipli 32-bit word ABI; `uniform_word()` doğrulayıcısı (MSR AS13) | Kabul |
| [0043](0043-deterministic-physics-command-producers.md) | Fizik komutlarında deterministik üretici sırası: faz + sistem kayıt sırası (MSR PH05) | Kabul |
| [0044](0044-staged-script-module-load.md) | Luau modül yüklemesi: bildirim aşaması, registry geri alma, `on_load()` (MSR AG09) | Kabul |
| [0045](0045-query-filter-ignore-list.md) | Fizik sorgularında birden çok yok sayılan gövde: `QueryFilter::ignore_list`, `ScriptQueryFilter` (G1.1) | Kabul |
| [0046](0046-headless-input-script.md) | Başsız çalışmada betikli girdi: `AppDesc::input_script` (G1.1) | Kabul |
| [0047](0047-spirv-disk-cache.md) | SPIR-V disk önbelleği ve paralel shader derleme | Kabul |
| [0048](0048-startup-responsiveness-and-mips.md) | Yükleme sırasında pencere tepkisi ve CPU mip üretimi | Kabul |
| [0049](0049-path-tracer-reset-telemetry.md) | Path tracer sıfırlama nedeni ve sahne dondurma | Kabul |
| [0050](0050-reflection-roughness-cutoff.md) | Ray-traced yansımalarda pürüzlülük eşiği | Kabul |
| [0051](0051-projected-texture-orientation.md) | Planar ve triplanar projeksiyonda doku yönü | Kabul |
| [0052](0052-script-budget-per-module.md) | Luau zaman bütçesi modül ve faz başına; aşımdan sonra yeniden kurulur (status §4) | Kabul |
| [0053](0053-editor-rollback-and-hierarchy-rules.md) | Editör: oluşturulan dosyaların geri alınması, ebeveyn döngüsü, silmede çocuklar köke (status §4) | Kabul |
| [0054](0054-headless-silent-mixer-clock.md) | Başsız çalışmada sessiz mikser oyun saatiyle ilerler (status §4) | Kabul |
| [0055](0055-material-shader-hardening-and-layers.md) | Material/shader sertleştirme, shader'da texture, PBR tamamlama, katman ve maske harmanı | Önerildi |
