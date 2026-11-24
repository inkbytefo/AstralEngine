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
| [0039](0039-terrain-foundation.md) | Terrain temeli — tile heightfield, CPU mesh LOD, Jolt ve streaming | Kabul |
