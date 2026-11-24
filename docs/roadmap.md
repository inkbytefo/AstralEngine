# Yol Haritası

**Plan v5 · 23 Kasım 2026**

**İncelenen taban** `inkbytefo/AstralEngine` · `master` · `c1de0e7`

**Durum** ✅ tamamlandı · 🚧 sürüyor · 🔜 sıradaki · ⬜ planlı

AstralEngine, C++20 kütüphaneleri ve isteğe bağlı App katmanıyla açık dünya, fizik tabanlı karakterler, deformasyonlu araçlar ve çok oyunculu oyunlar için ortak altyapı sunmayı hedefler. Editörde ajanlar değişiklik önerir, kullanıcı farkı ve önizlemeyi inceleyip kayda onay verir.

## Neredeyiz

| Faz | Kapsam | Durum |
|---|---|---|
| M0–M7 | Core, SDL3, Vulkan RHI, ECS, App, render graph ve hibrit RT, Jolt, glTF, sahne/prefab, cook/paketleme, Pong | ✅ |
| M8–M10 | Debug araçları, ses ve HUD, dünya streaming, shader ve materyal dili | ✅ |
| M11–M12 | FeatureSet ve Luau, sabit adımlı fizik köprüsü, sorgular, eklemler, rüzgâr ve su alanları | ✅ |
| M13 | Gelişmiş PBR, deferred decal ve progresif path tracer referansı | ✅ |
| M13.5–M13.6 | Modül ayrıştırması, test ve denetimler, paralel zamanlayıcı ve transform, arka planda hücre G/Ç, benchmark'lar | ✅ |

Tamamlanma işaretleri depo tarihçesinden gelir. Bu revizyonda kaynak ve test yapılandırması incelendi. Derleme, CTest, sanitizer, Windows Debug, golden ve performans sonuçları F0'da ilgili commit ve ortamla yeniden kaydedilecek.

### Kaynaktaki başlangıç noktaları

| Alan | Mevcut uygulama | Sonraki iş |
|---|---|---|
| Gameplay | `FeatureSet` ve `FeatureContext` App'te. Script API ve tipler reflection/bağlamalardan üretiliyor. Luau kuvvet, impuls, sorgu, alan ve olaylara erişiyor. [Feature API](../modules/app/include/astral/app/feature.hpp) · [Script API](../modules/script/include/astral/script/script.hpp) | F1 sözleşme, komut zamanlaması ve yaşam döngüsü |
| Fizik | Jolt **v5.3.0** sabitlenmiş. Fixed, Point, Hinge, Slider ve Distance mevcut. SixDOF, motorlu eklemler ve kırılma olayı eksik. `PhysicsService` App yüzü, `PhysicsWorld` ECS'den bağımsız alt API. [Sürüm](../modules/physics/CMakeLists.txt) · [Fizik](modules/physics.md) | P0 fizik sözleşmesini genişletir |
| İçerik | `.world` = `astral.map` v1. `<harita>.entities/<id>.ent` = `astral.entity` v1, cook çıktısı `.cell`. `.scene` = `astral.scene`, `.prefab` = tek köklü aynı belge biçimi. ECS `World` canlı depo, `astral.world` tam ECS kayıt biçimi. Prefab override, nesting ve reload, haritada hücre durumu ve referans temelleri mevcut. [Harita API](../modules/world/include/astral/world/map.hpp) · [Sahne API](../modules/world/include/astral/world/scene.hpp) · [Hücre durumu](../modules/world/include/astral/world/map_state.hpp) | E1 düzenleme belgesi ve Level Instance sahipliği |
| Araçlar | App içi ImGui debug UI, reflection Inspector, scene/world/material ve profiler panelleri mevcut. Bağımsız editör hedefi, belge transaction'ı ve insan onay akışı planlı. [Debug UI](../modules/app/src/debug_ui.cpp) · [Paneller](../modules/app/src/panels) | E0 mevcut bileşenleri değerlendirir |
| Animasyon | glTF loader skin, animation ve morph'u uyarıyla atlıyor. [Model API](../modules/assets/include/astral/assets/model.hpp) · [Yükleyici](../modules/assets/src/gltf.cpp) | A0 import, cook, poz ve render teslimlerini uçtan uca ekler |
| Sunucu | `AppDesc::headless` penceresiz/GPU'suz çalışır. Kök derleme Vulkan SDK ister. Transport, replication ve dedicated server paketi planlı. [App](../modules/app/include/astral/app/app.hpp) · [GPU CMake](../modules/gpu/CMakeLists.txt) | N0 çalışma zamanı ve dağıtım bağımlılıklarını ayrı doğrular |

### Mimari ve kabul kuralları

- Oyun döngüsünü tüketici yönetir, App standart kabuğu sağlar. ECS verisi C++ ve Luau için ortaktır. Component veriyi, System davranışı, Feature kurulumu ve paket sınırını taşır. Haberleşme component ve olayla, erişim yansıtılan şema ve dar servislerle yürür. Script durumunu kaydedilebilir component'lerde tut. [Mimari](architecture.md) · [ADR-0023](decisions/0023-modular-game-code.md) · [ADR-0026](decisions/0026-script-host-and-features.md)
- Public header'lar motor tipleri ve handle kullanır. Jolt/Vulkan/Luau modül içinde kalır. Yeni CMake kenarlarını mimari tablo ve `tools/module_layers.json` ile birlikte güncelle. App köprüleri modülleri birleştirir. Editör App'i tüketebilir, oyun ve sunucu paketleri editörü linklemez.
- Fizik komutları sabit adım sınırında uygulanır. Simülasyon pozu, animasyon hedefi ve render interpolasyonu ayrı tutulur. Seri/paralel ECS eşitliği, sabit girdili fizik replay ve ağ düzeltme davranışı ayrı test edilir. Platformlar arası ağ tutarlılığı snapshot ve reconciliation ile sağlanır.
- Her faz sözleşme ve gerekiyorsa ADR, ilgili unit/integration/headless test, çalışır örnek, kullanıcı dokümanı ve ölçüm çıktısı verir. Oyun API'si değişirse C++/Luau örnekleri, uygulanabilir Inspector görünümü, üretilen `API.md` ve `astral.d.luau` birlikte doğrulanır. Görsel fazlar golden ve uygun olduğunda M13 path tracer referansını kullanır. [Faz kapanışı](process.md)
- Performans sonuçlarına commit, donanım, sürücü, derleyici, çözünürlük, sahne, kalite ayarı, örnek sayısı, p50/p95/p99 ve bellek eklenir. RTX 3060 12 GB görsel referans profili, düşük donanım/CPU ve headless sunucu profilleri ayrı ölçülür. Sayısal bütçeler ilgili fazdan önce F0 ölçümleri ve hedef platform kararıyla belirlenir.

## Öncelik ve bağımlılıklar

M14–M23 kimlikleri korunur. A0, M20'nin teslim planı, S1 M21'in teslim planıdır. V0, V1 ve S2, M23'ün ayrı teslimleridir. M23 ayrıca M19 ile ortak kırılma entegrasyonunu kapsar. P0 bu plandaki Physics API v2 fazıdır. Tarihçedeki M13.6/P0 benchmark işini ifade eder.

| Faz | Kapsam | Başlıca ön koşul | Durum |
|---|---|---|---|
| F0 | Doğrulama ve capability envanteri | Mevcut master | ✅ |
| F1 | Gameplay API ve fizik adım sözleşmesi | F0, M11–M12 | ✅ |
| E0 | Bağımsız AI destekli editör çekirdeği | F0, M8, F1 kimlik/şema sözleşmesi | ✅ |
| E1 | Dünya düzenleme ve Level Instance | E0, M9, F1. Arazi araçları M15 ile birleşir | ✅ |
| E2 | AI-First Native Editör UX, Offscreen Viewport & Asenkron AI İnceleme ([ADR-0037](decisions/0037-ai-first-native-editor-v2.md)) | E0, E1 | ✅ |
| P0 | Physics API v2 ([ADR-0038](decisions/0038-physics-api-v2.md)) | F1, M12 | ✅ |
| M14 | Atmosfer, hacimsel VFX ve GPU partikülleri | F1, M10, M12–M13 | 🔜 |
| M15 | Heightfield arazi ve streaming | F1, M9–M10, M12 | Temel ✅ · katmanlar ve düzenleme ⬜ |
| M16 | Okyanus ve göller | M14–M15, P0 su örnekleme sözleşmesi | ⬜ |
| M17 | Bulutlar ve hava durumu | M13–M14 | ⬜ |
| M18 | Etkileşimli su ve nehir | M16 | ⬜ |
| M19 | SDF dünya ve yıkım | M15, P0 | ⬜ |
| M20 / A0 | Animasyon, IK ve skinned RT | F1, M5b, M10 | ⬜ |
| M21 / S1 | Pasif ve aktif ragdoll | P0, A0 | ⬜ |
| M22 | HLOD, bitki örtüsü ve GPU culling | M15, E1 hücre sözleşmesi. SDF kapsaması M19'a bağlı | ⬜ |
| M23 / V0 | Rigid vehicle | F1. Kontrat/prototip P0 ile paralel, entegrasyon P0 sonrası | ⬜ |
| M23 / V1 | Node-beam araç | V0, P0, solver ve çarpışma prototipi | ⬜ |
| M23 / S2 | Cloth, soft body ve hair | P0, M12. Karakter kumaşı ve saç bağlama için A0 | ⬜ |
| M23 | Parçalanma ve fizik entegrasyonu | M19, P0 ve M23 alt teslimleri | ⬜ |
| N0 | Sunucu otoriteli co-op | F1, M9. Gelişmiş fizik profilleri S1/S2/V1 olgunlaştıkça | ⬜ |

F0/F1 sonrasında M14 ve M15, E0 ile paralel ilerleyebilir. A0, P0 ve V0 prototipinin dosya sahipliği ayrılır. N0 protokol tasarımı F1 kimlik çalışmasıyla birlikte başlar. İlk co-op testi gelişmiş solver'ları beklemez.

## Temel altyapı ve editör

### F0 — Repo doğrulama ve yetenek envanteri

1. SHA, toolchain/SDK ve ortamı sabitle. `.github/workflows/ci.yml` ile MSVC Debug, Linux GCC/Clang Release ve TSan işlerini karşılaştır. Debug/Release configure-build-CTest, ASan/UBSan preset'i, `layer_check`, `code_size_check`, paket ve kurulu tüketici testlerini kaydet. TSan mevcut CI'da Core/World/Physics/Script hedeflerine sınırlı. App zamanlayıcısı için TSan kapsamasını ayrıca doğrula.
2. Lavapipe ile `ctest -L '^golden$'`, donanım GPU ile `ctest -L '^golden-hw$'` çalıştır. Atlanan işleri nedenleriyle, CI sonuçlarını SHA ve run bağlantısıyla kaydet. `astral_benchmarks_smoke` duman testi, Release `astral-bench` ölçek ölçümü, `astral-render-bench` GPU ölçümü verir. Streaming worker decode ölçümünü gerçek uçuş profiliyle tamamla.
3. CMake grafiği, public API, loader'lar, testler ve TODO'ları eşleştir. `docs/status/capabilities.md` içinde her satıra `implemented / partial / planned / unsupported`, kaynak dosya, test ve çalıştırma kanıtı ekle. Koddaki davranış ve ADR hedefi farklıysa ikisini işaretle.
4. README durum tablosunu bu yol haritasıyla eşleştir. Kapanmayan regresyon, eksik doğrulama ve riskler için ayrı takip kayıtları aç. Tarihsel ✅ durumlarını koru.

**Çıkış** Yeniden üretilebilir CI/yerel rapor, capability matrisi, benchmark tabanı ve takip kayıtları.

### F1 — Gameplay sözleşmesi ve fizik köprüsü

- `FeatureSet`, `feature.json`, `FeatureContext`, reflection/event registry ve `astral` bağlamalarını tek keşif yüzeyinde eşleştir. Reflection veri alanı ve tiplerin kaynağı, bağlama metadata'sı çağrılabilir API'nin kaynağı olsun. Inspector, API belgesi ve script tipleri arasındaki sapmayı test et. Şema sürümü, uyumlu okuma, migration ve içerik hatası teşhisini tanımla.
- Runtime `Entity`/handle nesli ve World aidiyeti, sahne/prefab belge kimliği ve harita referansı ayrı sözleşmelere sahip olsun. Yok edilen, henüz yüklenmeyen ve yeniden yüklenen entity durumlarını, component ekleme/silme sırasını ve kayıtta handle yeniden bağlamayı doğrula.
- Bugünkü `FrameLoop` C++ FixedUpdate'i, ardından Luau'yu çalıştırır. `PhysicsBridge` ECS komutlarını uygular, gövdeleri senkronlar, kinematik/karakter/alan kuvvetlerini hazırlar, step yapar ve kare sonunda temasları olaylara dönüştürür. FixedUpdate temas görünümü önceki adıma aittir. [FrameLoop](../modules/app/src/services/frame_loop.cpp) · [PhysicsBridge](../modules/app/src/services/physics_bridge.cpp)
- App servisinde komut, sorgu ve olay yüzlerini daralt. Tick girdilerini sabit sırayla topla, ECS komutları ve gövde eşlemesi sonrası step öncesinde uygula. Sorgunun hangi tamamlanmış durumdan okuduğunu belirt. Tick sonrası olay ve mevcut karelik temas davranışını uyumluluk testleriyle tanımla. Doğrudan `PhysicsWorld` kullanan C++ tüketicilerinin adım kontrolü korunur. Paralel sistemlerin doğrudan solver mutasyonu yerine sıralı komut teslimini doğrula.

**Çıkış** `physics_playground` üzerinde Luau kuvveti ve temas dinleme, sıfır/çok alt adımlı kare, gecikmiş komut, geçersiz entity, kayıt/yükleme ve C++ tüketici regresyonları.

### E0 — AI destekli masaüstü editör çekirdeği

- Ayrı editör hedefi, yeniden kullanılabilir reflection/asset/preview servisleri. ImGui docking ve multi-viewport'u mevcut wrapper, SDL3/Vulkan, DPI ve Linux/Windows pencere davranışıyla prototiple. Sade masaüstü çalışma alanında viewport, World/Scene/Prefab tarayıcı, hierarchy, Inspector, konsol ve profiler.
- Belge komutları kullanıcı ve ajan için ortak, sürümlü ve keşfedilebilir olsun. Seçili ajan ve hedef entity/asset üzerinden öneri al. Kalıcı kimlik ve belge revizyonuyla doğrula, değişiklik farkı ve önizleme göster. Onayla tek transaction kaydet, hata halinde rollback yap, undo/redo ve audit log tut. Önizlemeyi geçici ECS örneğinde çalıştır. Eski revizyon ve kısmi dosya kaydı senaryolarını test et.
- Editor araçlarının komut/şema testleri GPU'suz çalışabilir. Oyun ve sunucu paketlerinde editör hedefi ve ajan entegrasyonu bulunmadığını paket/tüketici testiyle doğrula.

**Çıkış** `examples/world_editor` iş akışında ajan prefab önerir, kullanıcı inceler ve onaylar, dosya tekrar yüklenir ve undo/redo aynı içeriği üretir.

### E1 — Dünya kompozisyonu ve içerik üretimi

- `.world` ve nesne başına `.ent` kaynak belgelerini düzenle. Cook hücrelerini, veri katmanlarını, streaming ownership'i ve floating origin'i göster. M15 arazi fırçası ve yükseklik çarpışmasıyla birleştir. Yol, bina, bitki, kaya ve prefab yerleştirmeyi veri güdümlü araçlar olarak ekle.
- İlk Level Instance modeli dönüştürülmüş bir sahne referansı ve instance kapsamlı override kullanır. Mevcut prefab diff/migration mekanizmasını değerlendir. Kaynak sahne, yerleştirilen instance ve iç entity kimliklerini ayrı tut. Edit-in-context kaynak düzenlemesi ile instance override kaydını açıkça sun. Ardından nesting, döngü teşhisi, hücre sahipliği, cross-cell refs ve büyük instance sınırlarını test et.
- Scene → World yükleme, WorldView/preview ve oynanabilir harita sahipliğini ayrı tut. Tam harita alt örneği kapsamı E1 ADR'sinde kararlaştırılır. Asset referansı üzerinden kurulan yerleşimi entity parenting'e bağlama. Dosya diff'i, hot reload sırasında override birleşimi ve cook tutarlılığı için regresyon ekle.

**Çıkış** 2×2 km düzenlenebilir harita, iki bağımsız Level Instance, kaynak düzenleme/override, hot reload ve hücre unload/reload sonrasında kimlik ve referansların korunması.

### P0 — Physics API v2

- Jolt v5.3.0 üzerinde SixDOF ve swing-twist, açısal limitler, motor/drive target, eklem kırılma olayı ve debug Inspector. Sürüm yükseltmeyi hedef özellik, uyumluluk ve karşılaştırmalı ölçüm gerekçelendirirse ayrı iş olarak ele al.
- Shape cooking ve sürümlü kaynaklar, fizik materyali/yüzey eşlemesi, batched query, F1 komut hattı, body/joint ownership ve streaming detach/attach. Sabit adım ve origin shift sırasında gövde/eklem yaşam döngüsünü test et.
- `PhysicsWorld` Core'a bağlı kalır, ECS ve Luau adaptörleri App katmanında kalır. Backend yeteneklerini dar sorgular üzerinden bildir. Ortak arayüz sınırını ikinci uygulamanın gerçek ihtiyaçlarıyla doğrula ve sıcak yolda body başına virtual dispatch ekleme. Jolt job havuzunu değiştirmeden önce worker çekişmesini ölç.

**Çıkış** `physics_lab` motorlu eklem demosu, limit ve stres testi, kırılma/streaming olayı, fixed-step replay ve eski C++/Luau davranışı için regresyon.

## Görsel dünya

| Faz | Uygulama | Kabul |
|---|---|---|
| M14 | Fiziksel gökyüzü, gün döngüsü, aerial perspective, IBL/DDGI, froxel sis. Hacim ve partikül shader türleri, `.vfx`, GPU partikülleri ve M13 decal pass üzerinden izler. Gameplay dumanı CPU görüş ızgarası ve fizik sorgusuyla, GPU seyrek hacmi ortak alan girdisiyle çalışır. Parametreler E0 Inspector'a açılır. [ADR-0024](decisions/0024-volumetric-vfx.md) | Koridoru dolduran ve engellerden yön değiştiren duman, görüş sorgusu, rüzgârlı toz ve gün döngüsü |
| M15 | `.aterrain` tile’ları, CPU mesh LOD, heightfield collision ve streaming. Sonraki adımlar materyal katmanları, delikler, E1 araçları ve ölçüme göre GPU clipmap. [ADR-0039](decisions/0039-terrain-foundation.md) | Dört tile geçişi, paket eşliği ve 2×2 km streaming ölçümü |
| M16 | FFT okyanus, göller, kırılma, RT yansıma, emilim, köpük, su altı ve araziyle kıyı. P0 ile sabit tick'te örneklenen dalga ve kaldırma sözleşmesi. | Dalgalı kıyıda tekne ve yüzen kasalar, CPU/GPU örnekleme hatası ve fizik kararlılığı |
| M17 | Hacimsel bulutlar, bulut gölgeleri, yağış, sis, birikinti ve M13 ıslaklık. FieldWorld ile rüzgâr, ileri aerodinamik, balistik ve kanat kaldırma. S2 rüzgâr girdisini paylaşır. | Açık havadan fırtınaya geçiş ve ıslanan arazi, alan ve GPU bütçesi |
| M18 | GPU sığ su, iz, sıçrama ve nehirler. Gameplay için CPU akış/yüzdürme örneği. S2/V0 su tepkisi aynı sözleşmeyi tüketir. | Nesne suya düşünce dalga ve nehirde sürüklenme, tick/çözünürlük profili |
| M19 | Seyrek SDF tuğlaları, GPU mesh çıkarımı, çalışma anında CSG, çarpışma temsilinin güncellenmesi ve hücre deltasına yazma. V1 yapısal deformasyon ayrı solver'da, M23 kırılma/fragment sözleşmesi ortak. | Krater, tünel, delinen duvar ve unload/reload sonrasında kalan dünya değişikliği |
| M22 | Hücreden cook edilen HLOD, büyük ölçekli bitki örtüsü, GPU-driven culling ve meshlet LOD. E1 world authoring ve streaming ile aynı hücre kimliği. | Birkaç km² dünyada LOD geçişi, CPU/GPU kare süresi yüzdelikleri, streaming ve bellek bütçesi |

## Animasyon ve fizik simülasyonu

### M20 / A0 — Animasyon çekirdeği

- [ADR-0025](decisions/0025-physics-based-character-animation.md) çizgisinde `Astral::Animation` saf CPU çekirdeği: Skeleton, Pose, Clip, blend/graph, root motion, retargeting ve IK. Assets glTF skin, klip, morph ve rig mapping'i import/cook eder. Render skinning/morph, motion vector ve skinned BLAS güncellemesini sahiplenir. App animasyon, ECS ve render arasındaki sırayı kurar.
- Önce skeleton/pose/clip import ve headless örnekleme, sonra GPU skinning/morph, ardından blend, retargeting, two-bone/FABRIK, look-at ve el/ayak yerleştirme. IK dünya sorguları köprü üzerinden alınır. Fizik öncesi hedef poz ve fizik sonrası render pozunu ayrı tamponla. Rig eşlemesi ve görsel düzeltme E0 Inspector ve ajan onay akışını kullanır.

**Çıkış** `character_lab` içinde merdivene oturan ayaklar ve iki elle nesne tutuşu. Pose/IK birim testi, import ve cooked veri eşitliği, skinned raster/RT gölge ve yansıma tutarlılığı.

### M21 / S1 — Pasif ragdoll'dan aktif ragdoll'a

- ADR-0025'teki `Astral::Character` sınırını temel al. Animation poz üretir, Physics P0 gövde/eklem ve motorları yürütür, Character Animation/Physics/World verisini birleştirir. Katman ve App entegrasyonunu ADR ile kesinleştir. `RagdollAsset` / `.ragdoll` kemik eşlemesi, kütle, eklem limiti, çarpışma filtreleri ve kas profili taşır.
- Önce pasif ragdoll ve animasyon↔fizik geçişleri. Ardından kinematik/güçlü/kısmi/aktif/pasif/kalkma durumları, tork sınırı ve PD/drive hedefleri, bölgesel harmanlama ve kas profilleri. Denge/destek poligonu, adım alma, darbe tepkisi, düşüşü yakalama ve kalkmayı ayrı davranış verisiyle ayarla. Yorgunluk ve benzeri profil etkilerini temel kontrol kararlı olduktan sonra ekle.
- Luau ve C++ gameplay yüzü mod seçimi, darbe ve toparlanma talebi gibi yüksek seviyeli eylemler sunar. Fizik LOD, aktif ragdoll sayısı, dünya sorgusu ve solver bütçesini ayrı izle. Ayarlamada simülasyon kararlılığı ile oyun tepkisi ayrı parametrelere sahip olsun.

**Çıkış** `character_lab` arena sahnesinde darbe, sendeleme, düşme ve kalkma. Sabit girdili replay, poz/denge toleransları, uyuyan ve aktif NPC dağılımı için CPU profili.

### M23 / V0 — Rigid Vehicle

- Jolt VehicleConstraint'i P0 sınırında değerlendir. Araç asset/prefab ve App ECS adaptörü. Teker ve süspansiyon, direksiyon, fren, motor/vites/diferansiyel, lastik tutuşu/yüzey eşlemesi, input actions, Inspector ve hasar olayları.
- Yüksek seviyeli sözleşme throttle, brake, steering, gear ve gözlemlenebilir araç durumu taşır. Simülasyon uygulaması Physics içinde kapsüllenir. V1 aynı girdiyi ve ortak durum görünümünü kullanır.

**Çıkış** `driving_lab` pisti, iki veya daha fazla zeminde viraj/fren testi, süspansiyon kararlılığı ve en az 10 araç CPU profili.

### M23 / V1 — Node-beam araç deformasyonu

- Rigid Vehicle'dan bağımsız uygulama. Düğüm kütlesi, beam sertliği/sönümü, plastik deformasyon, kopma eşiği ve yapısal gruplar. Backend kararı için adaptif alt adım, kararlılık, enerji sapması, yüksek hızlı çarpışma ve lastik teması prototipi kur.
- Jolt coupling'de temas sahipliği, kuvvet aktarım sırası, çarpışma temsilinin güncellenme maliyeti ve çift impulsu engelleyen sınır ADR'de açıklansın. Render mesh deformasyonu, fragment detach, çarpışma vekili ve LOD'u doğrula.
- N0, araç girdileri ve kritik hasar olayları ile seçili düğüm snapshot/düzeltmelerini ölçer. Tam düğüm durumu, bant genişliği ve uzaklık profili açısından karşılaştırma adayıdır.

**Çıkış** `crash_lab` kontrollü tek araç çarpışma tekrarı, deformasyon ve enerji metrikleri, ardından çoklu araç CPU/bellek ve ağ profili.

### M23 / S2 — Cloth, Soft Body ve Hair

- `ClothAsset`, `SoftBodyAsset`, `GroomAsset` ve instance component'leri için ortak kaynak, binding, yaşam döngüsü ve LOD sözleşmesi. Solver seçimini iş yüküne göre yap. Cloth için topoloji, pinning, çarpışma ve rüzgâr, isteğe bağlı tearing. Soft body için hacim/şekil koruma, skinning ve rigid coupling, Jolt ile özel solver karşılaştırması. Hair için guide/follower strands, kafa derisine bağlama, collision ve GPU compute/render.
- Gameplay'i etkileyen deformasyonun otoritesi CPU/sunucu profiline ait. Kozmetik GPU simülasyonu yerel kalabilir. M17 rüzgârı ve M18 akış/yüzdürme verisi alan sözleşmesinden alınır. Cloth/soft body ve hair aşamalı teslim edilir.

**Çıkış** `physics_lab` içinde rüzgârda pelerin ve ezilip toparlanan soft body, `character_lab` içinde hareketli saç. Çarpışma, LOD geçişi ve kararlılık testleri, CPU/GPU maliyet tablosu.

### M23 — Kırılma entegrasyonu

M19 yıkım olayını ortak fragment yaşam döngüsüne bağla. Önceden kırılmış mesh'ler, dinamik parçalar, sahip olunan handle'lar, uyutma ve streaming sırasında temizleme. SDF geometri güncellemesi ve V1 beam kopması kendi simülasyon verilerini yönetir. Çok parçalı sahnede spawn bütçesi, hücreye geri dönüş ve save/load doğrulanır.

## N0 — Multiplayer / Co-op

- İlk hedef iki oyunculu co-op. Transport arayüzü, handshake/protokol ve şema sürümü, sunucu otoritesi, ownership, spawn/despawn ve snapshot. Replication alanlarını reflection üzerinden açıkça seç. Runtime handle, kalıcı içerik kimliği ve bağlantı/oturum kimliği ayrı eşlemelerle çözülür.
- Sunucu sabit tick'te doğrulanmış sıra numaralı input işler. İstemci yerel prediction/reconciliation ve uzaktaki entity'ler için interpolation kullanır. Lag compensation kapsamı oyun türüne göre ADR'de belirlenir. Sunucu otoritesi ve mesaj/snapshot bütçeleri için sınır, zaman aşımı ve desync telemetrisi ekle.
- Cell interest ve oyuncu başına streaming ilgisi sunucu yükleme sahipliğinden ayrılır. Geç katılım, cross-cell refs, reconnect, oturum kapanışı ve kalıcı kayıt test edilir. Dedicated server penceresiz ve editör bağımlılığı olmadan çalışır. SDK'sız sunucu derlemesi gereksinimini platform hedefiyle değerlendir.
- S1/S2/V1 sonrası ayrı fizik profilleri ekle. Örneğin ragdoll oyun durumunu sunucuda, uzak poz ayrıntılarını kozmetik tut. Araç hasarı ve soft body'nin oyun açısından gerekli verisini düşük frekanslı snapshot ve olaylarla ölç. Sözleşme packet loss, jitter ve latency altında yakınsamayı ve bant genişliğini tanımlar.

**Çıkış** `examples/coop_lab` içinde dedicated server ve iki istemci, hücre geçişi, geç katılım, paket kaybı/gecikme simülasyonu, authority ve düzeltme raporu.

## Kabul sahneleri

Her örnek `templates/blank`/standart oyun kabuğu ve feature sınırlarını izler. Editör örneği bağımsız editör hedefini açar. İlk kapsam için mevcut sahneler genişletilebilir.

| Örnek | Kanıt |
|---|---|
| `examples/world_editor` | E0/E1 ve M15. Prefab onayı, undo/redo, Level Instance, arazi |
| `examples/physics_lab` | F1/P0/S2. Komut, sorgu, motorlu eklem, cloth ve soft body. Başlangıç `physics_playground` |
| `examples/character_lab` | A0/S1/S2. Rig, IK, aktif ragdoll, kalkma, saç |
| `examples/driving_lab` / `examples/crash_lab` | V0/V1/M23. Sürüş, çarpışma ve deformasyon |
| `examples/coop_lab` | N0. Sunucu otoritesi, replikasyon ve streaming |
| `examples/open_world_game` | M14–M22 ve V0 entegrasyonu. Terrain, yollar, sürüş, hava, su, HLOD. Başlangıç `open_world` |

## Uygulama sırası ve kararlar

| Sprint | Teslim |
|---|---|
| 1 | F0 kaynak envanteri, CI ve benchmark tabanı. Fizik, animasyon, scene/map ve editör için capability matrisi |
| 2 | F1 gameplay sözleşmesi ve fizik komut prototipi. E0 hierarchy/Inspector ve transaction prototipi |
| 3 | P0 motorlu eklem ve A0 skeleton/pose import dilimleri. E1 dünya/prefab ve Level Instance tasarımı |

Sprintler sıralama önerisidir. Her teslimden önce ilgili `AGENTS.md`, `CLAUDE.md`, mimari, ADR, kod ve testleri oku. Dal/SHA ve mevcut API'leri dosya referanslarıyla kaydet. Modül sınırı, API adı, backend, asset biçimi, editör sahipliği veya determinizm sözleşmesi değişiyorsa önce ADR önerisini ve kabul testlerini incelemeye aç. Uygulamayı küçük worktree/PR'lara böl. İlgili build/test, feature/script denetimi, katman ve boyut kontrolleri, golden ve benchmark sonuçlarıyla kapanış yap. Tamamlanan faz tarihçesini koru.

| Karar | Ne zaman ve neyle |
|---|---|
| ImGui docking, multi-viewport ve editör hedefi | E0 prototipi. Mevcut panel kodu, Linux/Windows ve paket bağımlılıkları |
| Level Instance kimliği, override, nesting ve cross-cell ownership | E1 ADR. Mevcut prefab path/diff modeli ve MapState üzerine iki instance round-trip |
| Jolt yükseltmesi, job pool ve Physics API adları | P0. Mevcut pin üzerinde mini prototip, API uyumluluğu, sanitizer ve performans |
| Character katmanı ve deformables solver sınırı | A0/S1/S2. ADR-0025'teki Character ayrımı, bağımlılık grafiği ve CPU/GPU yük profili |
| Node-beam/Jolt coupling | V1. Temas, kararlılık, drift, yüksek hız ve maliyet ölçümü |
| Co-op sonrası oyuncu sayısı ve rekabetçi özellikler | N0 protokol tasarımı. İki oyunculu referans, bant genişliği ve RTT/kayıp bütçesi |
| Hedef platformlar, minimum CPU/GPU, kalite ve performans bütçeleri | F0 referans ölçümleri; ilgili faz başlamadan ürün kararı |

### Önceki plandan izlenecek açık işler

- F0, dağıtım ve test kanıtlarıyla M13.5/M13.6 kapsamını kapatır. Eski Context sarmalayıcılarının kaldırılması F1'de tüketici etkisiyle ele alınır. Script paralelliği, ek toplu sütun sorguları, script bütçe uyarıları, native codegen ve C++'a geçiş ölçütü profiler verisiyle önceliklendirilir.
- G/Ç ve derleme izlemesinde sahne/prefab arka plan hazırlığı, shader pipeline disk önbelleği ve render komut kaydının paralelleştirilmesi ölçüme bağlı takip işleridir. Değişmeyen transform alt ağaçlarını atlama, gerçek sahne profiline göre değerlendirilir.
- C++ hot reload ve DLL eklentileri ileri aşamadır. Özellikle modüller arası component kimliği için ayrı ADR gerekir. [World sınırları](modules/world.md)

## Tamamlanan fazlar

## M0 — İskelet ve Core ✅

**Sonuç:** Modüler CMake sistemi (`astral_add_module`), `Astral::Core` modülü, 25 test,
ThreadSanitizer temiz, `find_package(Astral COMPONENTS Core)` ile kullanılabilir,
GCC/Clang/MSVC + TSan CI. Bağımsız inceleme bir kilitlenme hatası buldu; düzeltildi.
Ayrıntı: [modules/core.md](modules/core.md).

**Devreden borç:** [core.md → Bilinen sınırlar](modules/core.md#bilinen-sınırlar).

## M1 — Platform ve GPU ✅

**Sonuç:** Vulkan 1.3 RHI (handle, bindless, BDA, timeline, gecikmeli silme, swapchain), SDL3
pencere/input, kendi matematik kütüphanemiz, `hello_triangle`. GPU testleri lavapipe ile
headless, pencere+present testi Xvfb ile koşuyor; validation hatası yok. Public header kuralı
artık otomatik test. Ayrıntı: [gpu.md](modules/gpu.md), [platform.md](modules/platform.md).

**Bitiş kriterleri (karşılandı):** `examples/hello_triangle` pencerede üçgen çizer; GPU modülü
pencere olmadan (headless/offscreen) da çalışır ve testleri CI'da yazılımsal Vulkan
(lavapipe) ile koşar; public header'larda Vulkan/SDL tipi yok.

## M2 — Dünya ve App ✅

**Sonuç:** Kendi archetype ECS'imiz (cache dostu SoA, paralel sorgu, ertelenmiş komutlar, hiyerarşi),
instanced forward renderer, `App` + `Game` + `System`; `spinning_cubes` 576 küpü component/system
olarak yazılmış oyun koduyla çiziyor. M1 incelemesinin 1 kritik + 9 önemli, M2 incelemesinin 2 kritik + 3 önemli bulgusu testle düzeltildi;
tüm suite ASan/LSan, TSan ve `-Werror` (GCC/Clang) altında temiz.
Rehber: [writing-a-game.md](guides/writing-a-game.md).

**Bitiş kriterleri (karşılandı):** `examples/spinning_cubes`: oyun kodu yalnızca `Game` + component + system
yazar; ECS sorguları JobSystem ile paralel; system'ler headless test edilir.

## M3 — Render ✅

**Sonuç:** Render graph (otomatik bariyer, havuz, ayıklama) ve üzerinde hibrit deferred renderer:
gölge haritası, mesh G-buffer, SDF raymarch (CSG + yumuşak birleşim), PBR aydınlatma, SDF yumuşak
gölgeleri, nokta ışıklar, ACES. `examples/sdf_scene`. İncelemenin 6 önemli bulgusu testle düzeltildi. Brick bake, TAA, IBL,
cascaded gölgeler sonraki alt fazlara kaldı ([render.md](modules/render.md)).

**Bitiş kriterleri (karşılandı):** render graph pass ekleme/çıkarma mevcut pass'lere dokunmadan yapılır;
eski v2'deki deferred + SDF teknikleri bu mimariye temiz yeniden yazılmış olur.

## M4 — Fizik ✅

**Sonuç:** Jolt üzerine `Astral::Physics` (body'ler, şekiller, sensörler, CCD, raycast, temas olayları,
karakter kontrolcüsü); App'te `RigidBody`/`CharacterController`, sabit 60 Hz adım. `examples/physics_demo`.
İncelemenin 7 önemli bulgusu testle düzeltildi; fizik TSan altında temiz.
Ayrıntı: [physics.md](modules/physics.md).

**Bitiş kriterleri (karşılandı):** `physics_demo` (düşen kutular, raycast); fizik renderer olmadan test edilir.

## M3.5 — Fotogerçekçi render ✅

**Sonuç:** RHI'de ray tracing (BLAS/TLAS, adresle ray query); renderer'da ray-traced yol: RT gölgeli doğrudan
ışık + alan ışıkları, DDGI + yakın alan RTGI, GGX yansımalar, SVGF tarzı denoiser, TAA, bloom
([ADR-0011](decisions/0011-hybrid-ray-traced-lighting.md)). `examples/rt_showcase`. Raster yolu yedek olarak
korunuyor. Shader include bağımlılık hatası bulundu ve düzeltildi.
**Bağımsız inceleme bu fazda henüz yapılmadı** — sonraki adımlardan biri.

![RT showcase](images/m3.5-rt-showcase.png)

**Bitiş kriterleri (karşılandı):** RT destekli cihazda (lavapipe dahil) gölge, alan ışığı, ayna yansıması,
renk taşması ve SDF katılımı testlerle doğrulanır; validation temiz; RT yoksa raster yoluna düşer.

## M3.6 — Render API v2 ✅

**Sonuç:** Kaynak/kare ayrımı netleşti: mesh ve materyal paylaşılan handle'lar, kare verisi saf `RenderScene`.
Materyal tablosu raster ve RT yollarında ortak. G-buffer hız hedefi; App transform geçmişini kendisi tutar.
`set_settings` ile çalışma anında teknik değişimi. 27 render testi (materyal, ayar, hareket dahil).

## M5a — ECS tamamlama ✅

**Sonuç:** ECS yeniden yazılmadı, üstü tamamlandı: değişiklik takibi (satır başına tick, sistem başına referans),
filtreli sorgular, olaylar, reflection ve JSON ile dünya kaydet/yükle (entity referansları yeniden eşlenir).
Core'a bağımlılıksız `Json` ve `ASTRAL_REFLECT` ([ADR-0013](decisions/0013-ecs-change-detection-reflection.md)). ThreadSanitizer temiz. Ayrıntı: [world.md](modules/world.md).

**Bitiş kriterleri (karşılandı):** bir dünya JSON'a kaydedilip başka bir dünyaya yüklenir ve yeniden kaydedildiğinde
bayt bayt aynı belgeyi verir; App sistemleri `changed<T>()` ile yalnız kendi görmedikleri değişiklikleri görür.

## M5b — Asset'ler ✅

**Sonuç:** `Astral::Assets` ([assets.md](modules/assets.md), [ADR-0014](decisions/0014-assets.md)): kalıcı
AssetId, asenkron glTF/GLB yükleme, texture'lı PBR materyaller (raster + RT), handle koruyan hot-reload,
sahnede `ModelRef`. `examples/model_viewer`.

![model viewer](images/m5b-model-viewer.png)


## M5c — Sahne ve prefab ✅

**Sonuç:** `astral.scene` v1 biçimi (sahne ve prefab aynı biçimde). Kayıtlar kalıcı `id` taşıyor, tekrar
kaydetmede diff yalnızca değişen satırlarda çıkıyor. Prefab örnekleri yalnızca farklarını saklıyor
(`components`/`parts` yamaları, `removed`, `deleted`); farklar kayıt anında hesaplanıyor, yani koddan
yapılan değişiklik de override oluyor. İç içe prefab ve varyant, örnek içine referans (`"2/5"`),
hot-reload'da override'ları koruyan yeniden açılış var. Ek olarak isimle yazılan enum'lar,
`PrimitiveMesh`, App'te `load_scene`/`save_scene`/`spawn_prefab` ve offscreen render. RGB oda artık bir
`.scene` dosyası (`examples/scene_viewer`). Testler: debug 10/10, TSan temiz.
Ayrıntı: [ADR-0015](decisions/0015-scenes-and-prefabs.md), [modules/world.md](modules/world.md#sahneler-ve-prefablar).

**Devreden borç:** ikili sahne biçimi (M6 cook), açık dünya hücreleri/streaming (sonraki faz), sahne
başına render ayarları dosyada değil, bağımsız inceleme yapılmadı.

## M6 — Paketleme ✅

**Sonuç:** `astral-cook` aracı (`cook_directory`): kaynak klasörü aynı yollarla cook eder. glTF → `AMDL`
(texture'lar kullanıma göre BC7 sRGB / BC7 / BC5, hazır mip zinciri; `.bin` ve görüntüler modele katılır),
sahne/prefab → ikili JSON, geri kalanı kopya. İçerik hash'li artımlı önbellek var. `.apak`: dosya başına zstd,
okumada boyut ve hash doğrulaması. `AssetManager` pack'ten ya da gevşek (kaynak veya cook edilmiş)
klasörden içeriğe bakarak okur; BC'siz GPU'da RGBA8'e açar. GPU'ya BC5/BC7, renderer'a hazır mip zinciri
eklendi; normal map Z'si shader'da. `AppDesc::pack` (exe'nin yanında varsa kullanılır), `astral_add_game`
+ `<oyun>_package` (çalıştırılabilir klasör), kurulu pakette `Astral::astral_cook`. Paketli RGB oda gevşek
modla aynı görüntüyü veriyor (ortalama fark 0.006/255). Testler: debug 12/12, release `-Werror`, TSan temiz,
`tests/consumer` kurulu paketle oyunu cook edip çalıştırıyor.
Ayrıntı: [ADR-0016](decisions/0016-packaging.md), [modules/assets.md](modules/assets.md#cook-ve-paket-m6).

**Devreden borç:** memory-map ve kısmi okuma yok, prefab şablon cook'u yok (sahne yine JSON
değerlendirmesi), ASTC/mobil yok, bağımsız inceleme yapılmadı.


## M7 — İlk oyun: 3D Pong ✅

**Sonuç:** `examples/pong`: tünelde 3D Pong ("Curveball"). Oyuncu WASD/oklar/fare ile oynuyor, karşıda
yapay zekâ var, topun rakete çarptığı nokta yönünü büküyor, 5 sayıda maç bitiyor. Oyun kodu iki dosya
(`pong.hpp/.cpp`: 5 component, 5 sistem). Görünen her şey `court.scene` ve 5 prefab'da; bunlara oyunun
kendi component'leri (`pong::Paddle`, `pong::Court`...) dahil. Rakip raket bir prefab **varyantı**,
derinlik halkası override'lı bir prefab örneği. Testler headless ve deterministik: dört dakikalık AI-AI
maçı 2 saniyede, kurallar ayrı ayrı. `pong_package` ile paketleniyor. Görüntü:
[images/m7-pong.png](images/m7-pong.png).

**Pong'un motorda bulduğu eksikler** (sonraki fazların girdisi):
- Ses yok: çarpma ve sayı sesi çalınamıyor → M8.
- Metin/HUD yok: skor pencere başlığına ve sahnedeki ışıklara yazıldı → M8.
- `astral.hpp` `Window` ve sahne API'sini içermiyordu → bu fazda düzeltildi.
- Tekil component'e (kural nesnesi) erişim için yardımcı yok (`each` + ilk eleman) → küçük iş.
- Yakalama her kareyi çiziyor; mantığı hızlıca ileri sarıp yalnız son kareleri çizmek mümkün değil.
- Bir component'in materyalini değiştirmek yeni bir materyal oluşturuyor; önbellek hiç boşalmıyor
  (`PrimitiveMesh`).

## M10 — Shader dili ve materyaller ✅

**Sonuç (0.15.0):** Karar: [ADR-0022](decisions/0022-shader-and-material-language.md) (sapmalar orada). Rehber:
[shaders-and-materials.md](guides/shaders-and-materials.md).
- **Dil:** `.ashader` = Godot benzeri başlık + GLSL gövdesi (`Astral::Shader`: ayrıştırıcı, uniform ipuçları,
  glslang ile çalışma anında derleme, hatalar `.ashader` satırıyla).
- **Yüzeyler:** `surface` shader'ı G-buffer ve gölge aşamalarına gömülür (`vertex()` deformasyonu, `alpha_test`,
  `cull_*`, `TIME`); `blend_mix`/`blend_add` ileri geçişte arkadan öne sıralı çizilir.
- **Işın izleme:** özel `surface()` RT compute shader'larına çalışma anında derlenir; aynı materyal raster ve RT
  yansıma/GI'da aynı yüzey değerlerini verir. Hata durumunda hazır RT shader'larına dönülür.
- **`post` ve `sky`:** tam ekran shader'lar (post zinciri ping-pong, sky yalnız arka planda).
- **`.material`:** shader + değerler + ebeveyn zinciri (`resolve_material`); `ctx.load_material`,
  `PrimitiveMesh::material_file`, `ctx.sky`, `ctx.post`.
- **Döngü:** `.ashader`/`.material` hot reload (handle'lar sabit, hatada son çalışan sürüm kalır);
  F1 → Materials paneli uniform ipuçlarından üretilir; cook `.ashader`'ı derleyip doğrular.
- **Demo:** `examples/shader_showcase` (yaprak, iki cam küre — biri ebeveynli materyal, lav, hologram, sky,
  vinyet); görüntü testi `shader_showcase`.

![M10](images/m10-shader-showcase.png)

**Bilinen sınırlar:** [render.md](modules/render.md) ve [shader.md](modules/shader.md) sonlarında. Başlıcaları:
şeffaflar gölge/RT'ye girmez, kırılma yok, HDR öncesi post yok, `MaterialOverride` ve önceden derlenmiş SPIR-V
önbelleği ertelendi, `particle`/`volume` türleri M14'te.

## M9 — Dünya ve harita sistemi ✅

**Sonuç (0.14.0):** Karar: [ADR-0021](decisions/0021-world-and-level-system.md).
- **Biçimler:** `.world` (ayarlar, ızgaralar, veri katmanları) + `<harita>.entities/<id>.ent` (nesne başına
  dosya). Bozuk, kimliksiz, çift kimlikli dosyalar raporlanır.
- **Cook:** `.world` + entity klasörü → dizin + `<harita>.cells/<ızgara>_<x>_<z>.cell` (ikili); artımlı,
  eski hücreler silinir.
- **Streaming:** `CellStreamer` (histerezis, en yakın önce, kare bütçesi, iptal); App'te `StreamingSource`,
  JobSystem'de çözme, 1 ms ekleme bütçesi, `ctx.finish_world_loading()`.
- **Katmanlar ve hafıza:** `MapLayers` (gece gibi), `MapState` (yok edilen/değişen nesneler dünya
  koordinatında saklanır).
- **Kayan orijin:** 2 km eşik, 100 m adım; kök transform'lar, fizik (`PhysicsWorld::shift_origin`),
  `RenderHistory` birlikte kayar, `OriginShifted` olayı. 100 km uçuşta milimetre hassasiyeti (test).
- **Araçlar:** F1 → World paneli, hücre çizimi, profiler'da "world streaming".
- **Demo:** `examples/open_world`: 4×4 km, ~4700 nesne, ~980 hücre, iki ızgara (ana + uzaktan görünen
  kuleler), gece katmanı; iki görüntü testi.

**Bilinen sınırlar:** bölüm örnekleri (`.scene` yerleştirme) ve hücreler arası referanslar yok; hücre
ataması sınır kutusuna değil konuma göre; oyunun hücrede yeni oluşturduğu nesneler saklanmıyor; hücre
dosyası okuma ana iş parçacığında; gökyüzü arka planı ortam rengini izlemiyor (M14).

## M8 — Geliştirici altyapısı, metin/HUD, ses ✅

**Sonuç (0.13.0):** Karar kayıtları: [ADR-0019](decisions/0019-dev-tools-and-ui-overlay.md),
[ADR-0020](decisions/0020-audio.md).
- **Profiler:** global olmayan CPU kapsam zamanlayıcıları (`ctx.profiler`, `ASTRAL_PROFILE_SCOPE`); App'in
  her aşaması ölçülüyor. GPU: render graph'ta pass başına timestamp (`Renderer::gpu_timings()`).
- **Debug çizim:** `ctx.debug` (çizgi, kutu, küre, kapsül, ok, eksen, ızgara; derinlik testli ya da üstte,
  süreli). Fizik şekilleri ve ışıklar için hazır katmanlar (`ctx.debug_show`).
- **Debug UI (F1):** Dear ImGui 1.92, kendi RHI'mizle çizilir. Paneller: performans (FPS grafiği, CPU ve GPU
  tabloları), sahne listesi, inspector (reflect edilmiş her component'i düzenle/ekle/sil), render ayarları.
- **Metin/HUD:** SDF font atlası (Türkçe karakterler), `ctx.hud` (dikdörtgen, çerçeve, resim, metin; gölge,
  hizalama). ImGui ve HUD tek 2D katmanda (`UiLayer`).
- **Ses:** `Astral::Audio` (miniaudio): WAV/FLAC/MP3, kanallar, 3D ses, sessiz cihaz (testler). App'te
  `ctx.audio`, `ctx.sound(yol)`, `AudioSource`/`AudioListener` component'leri; sesler pack'ten de okunuyor.
- **Pong:** ekranda skor ve sonuç yazısı, Türkçe kontrol satırı; raket/duvar/servis sesleri (3D, hıza göre
  perde), sayı ve maç sonu sesleri, sahne dosyasında ortam uğultusu. Sesler kodla üretiliyor
  (`examples/pong/tools/make_sounds.py`), lisans derdi yok.

- **Görüntü testleri (M8.6, güncellendi: [ADR-0031](decisions/0031-hardware-independent-golden-tests.md)):** `tests/golden`: örnek oyun ekran dışında bir kare yakalar, `astral-golden`
  kayıtlı lavapipe referansıyla karşılaştırır (`golden`: lavapipe'ta kanal başına eşik + kötü piksel oranı; `golden-hw`: donanım GPU'sunda parlaklık SSIM + güçlü piksel sınırı), fark varsa kırmızı
  işaretli fark görüntüsü yazar. GPU yoksa veya cihaz türü eşleşmiyorsa test atlanır. Referans güncelleme (yalnız lavapipe): `ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$'`.

**Bilinen sınırlar:** referanslar lavapipe ile üretildi; farklı sürücü/GPU'da tolerans aşılırsa referansı o
ortam için güncellemek gerekir. Bağımsız kod incelemesi token bütçesi nedeniyle bu fazda yapılmadı.

## M11 — Oyun kodu ve scripting ✅

**Sonuç (0.16.0):** Kararlar: [ADR-0017](decisions/0017-gameplay-scripting.md), [ADR-0023](decisions/0023-modular-game-code.md),
uygulama sapmaları [ADR-0026](decisions/0026-script-host-and-features.md). Modül: [script.md](modules/script.md).
- **World:** ham (tip-silinmiş) component erişimi, reflection'dan alan tabloları, şemadan **dinamik component'ler**
  (sahne/kayıt/inspector yolunu kullanır), adla gönderilen/okunan olaylar (`EventRegistry`).
- **Script (`Astral::Script`, Luau 0.748):** sistemler (entity başına veya fazda bir kez), davranışlar,
  olay dinleyicileri, zamanlayıcılar, `astral.query/spawn/find`, sandbox, zaman bütçesi, `dosya:satır` hataları,
  hot reload (hatada eski sürüm kalır), reflection'dan canlı component vekilleri.
- **Oyun çatısı:** `FeatureSet`/`feature.json` (bağımlılık sırası, döngü ve manifestte olmayan kayıt hataları),
  `astral-features check` sınır denetimi (CTest), `.actions` input eylemleri, `ctx.write_script_api`
  (`astral.d.luau` + `API.md`), kök `AGENTS.md`.
- **Şablon ve örnekler:** `templates/blank` (boş oyun: özellik + Luau + test + paketleme + sınır denetimi), standart oyun kabuğu
  `run_game` ([ADR-0027](decisions/0027-blank-template-and-game-shell.md)); bütün örnekler bu düzende yeniden kuruldu, yeni görüntü testleri
  (blank, hello_triangle, spinning_cubes, sdf_scene).
- **Pong** üç özellik paketi oldu: `pong_rules` (kurallar Luau, veri C++), `pong_scorelights`, `pong_audio`;
  aynı beş test geçiyor.

**Bilinen eksikler (sonraki fazlara girdi):** gamepad, tween/durum makinesi/tetik hacmi/kayıt yardımcıları,
Luau bytecode cook'u, luau-analyze doğrulaması, iç içe reflection'ın script erişimi
([script.md → Bilinen sınırlar](modules/script.md#bilinen-sınırlar)).

## M12 — Fizik altyapısı ve alanlar ✅

**Sonuç (0.17.0):** Karar: [ADR-0029](decisions/0029-physics-fields-and-joints.md). Modül: [physics.md](modules/physics.md). Plan: `superpowers/plans/2026-11-22-m12-physics.md`.
- **Fizik çekirdeği:** 16 çarpışma katmanı + matris, `QueryFilter`; `shape_cast`, `overlap`; yüzey kimliği; convex hull, mesh, heightfield, compound; Fixed/Point/Hinge/Slider/Distance eklemleri
  (bağlı gövdeler çarpışmaz); sensör bayraklı temas olayları.
- **Alanlar:** `FieldWorld` (rüzgâr: küresel + hacimler, gust, türbülans; su: yüzey, dalga, akış, yoğunluk), saf işlevler; `fields.glsl` aynı sayıları GPU'da verir (parite testi).
- **App:** `WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil`, `Joint` component'leri; `Contact`/`TriggerEntered`/`TriggerExited` olayları; isteğe bağlı görsel interpolasyon; debug çizimi.
- **Script:** `astral.physics.*` ve `astral.fields.*` (`ScriptPhysics` arayüzü), olaylar `astral.on` ile; üretilen API ve tip tanımı güncel.
- **Demo:** `examples/physics_playground` (arazi + göl, yüzen/batan gövdeler, rüzgâr, menteşeli köprü, zıplatma pedi, kanat), görüntü testi.
- **Ertelenen:** Jolt'u `JobSystem`'e bağlama, SixDOF/motorlu eklem, alan GLSL'ini tüketen shader'lar (M14/M16/M17).

## M13 — Gelişmiş PBR ve Path Tracer Referansı ✅

Karar: [ADR-0033](decisions/0033-advanced-pbr-and-path-tracer.md).
- Turquin (2019) enerji koruyan çoklu saçılmalı GGX (`brdf.glsl`) ve ortak `bsdf.glsl` katman kütüphanesi: clearcoat (IOR 1.5), Charlie NDF sheen, Heitz anizotropik GGX, dielektrik Fresnel + Snell kırılma + Beer-Lambert hacimsel soğurma (`Transmission`), subsurface scattering (`Subsurface`), ıslaklık (`WETNESS`), RNM detay normal + Parallax Occlusion Mapping (POM).
- `.ashader` `shading_model <standard|clearcoat|sheen|anisotropic|transmission|subsurface>;` başlığı ve ilgili tüm built-in değişkenler (`CLEARCOAT`, `SHEEN_COLOR`, `ANISOTROPY`, `TRANSMISSION`, `IOR`, `SUBSURFACE`, `WETNESS` vb.) raster, hibrit RT ve path tracer varyantlarında tekdüze derlenir.
- Gecikmeli kutu-izdüşümlü decal sistemi (`Decal` component + `DecalPass` + `decal.comp`, path tracer birincil vuruşlarında da uygulanır).
- Birikimli çok sekmeli yansız Path Tracer referans modu (`RenderSettings::path_tracer`, `PathTracerPass` + `path_tracer.comp`, RGBA32Float ping-pong biriktirme, kamera/ayar değişiminde otomatik sıfırlama).
- *Depoda kayıtlı sonuç:* `examples/material_showcase` (7 M13 PBR materyali + 2 decal, `P` tuşuyla canlı Hibrit RT / Path Tracer geçişi) + 1280×720 RTX 3060 kıyaslaması (`m13_pbr_rt` 2.98 ms / 335 FPS, 64-SPP Path Tracer 0.83 ms/spp, RMSE 2.74, ortalama parlaklık farkı %0.29).

## M13.5 — Kod sağlığı ve modülerlik ✅

**Sonuç (0.18.0):** Kararlar: [ADR-0030](decisions/0030-refactor-rules-and-size-budget.md), [ADR-0031](decisions/0031-hardware-independent-golden-tests.md).
- **R0 (Güvenlik ağı):** Windows Debug CI, `.gitattributes` LF normalizasyonu ve CRLF testleri, doctest 2.5.3, `golden` (lavapipe) / `golden-hw` (donanım GPU'su SSIM + güçlü piksel sınırı) ayrımı, `tools/check_sizes.py` (`code_size_check`) ve `tools/check_layers.py` (`layer_check`).
- **R1 (`Astral::Render`):** `renderer.cpp` store'lara (`stores/`), kare yuvalarına (`frame/`), tek sorumluluklu pass'lere (`passes/`) ve raster/RT yollarına (`paths/`) bölündü.
- **R2 (`Astral::App`):** `app.cpp` iç servislere (`FrameLoop`, `RenderBridge`, `PhysicsBridge`, `AudioBridge`, `ScriptBridge`, `WorldBridge`, `DevTools`) ve gruplu `Context` servis arayüzlerine (`scenes()`, `maps()`, `materials()`, `audio_service()`, `script_service()`, `physics_service()`) ayrıldı.
- **R3 & R4 (Bütçe tamamlama ve derleme süresi):** `debug_ui` (`DebugPanelRegistry` + `panels/`), `cook.cpp`, `cooked_model.cpp`, `gltf.cpp`, `asset_manager.cpp`, `physics.cpp` (`physics_queries_joints.cpp`), `bindings.cpp` (`bindings_services.cpp`), `api.cpp`, `scene.cpp` (`prefab.cpp`), `serialize.hpp`, `device.cpp` (`device_init.cpp`), `pipelines.cpp` ve `examples/physics_playground` (`playground.scene`) bütçe sınırları içine çekildi; `ASTRAL_SIZE_BUDGET_STRICT=ON` varsayılan olarak açıldı (yalnızca `Context` M14 kaynak uyumluluğu istisnası kaldı). İleri bildirim başlıkları (`world_fwd.hpp`, `render_fwd.hpp`, `assets_fwd.hpp`), `ASTRAL_PCH` ve `ASTRAL_UNITY_BUILD` eklendi.

## M13.6 — Ölçeklenebilir çalışma zamanı ✅

**Sonuç (0.19.0):** Karar: [ADR-0032](decisions/0032-scalable-runtime-and-parallel-scheduler.md).
- **P0 (Ölçek benchmark'ları):** `bench/` altında başsız `astral-bench` yürütülebilir hedefi (`bench_ecs`, `bench_transforms`, `bench_script`, `bench_streaming`, `bench_shader_compile`) ve `astral_benchmarks_smoke` CTest testi eklendi.
- **P1 (Erişim bildiren sistemler ve deterministik paralel zamanlayıcı):** `SystemAccess` (`reads<...>()`, `writes<...>()`, `query<...>()`, `mark_exclusive()`) ve `System::configure(SystemAccess&)` eklendi. `App::run_phase` çakışmasız ardışık sistemleri `JobSystem` üzerinde paralel dalgalar (waves) halinde çalıştırır; `World::ScopedTick` iş parçacığı-yerel (`thread_local`) tick ve değişiklik taban çizgisi (`change_baseline`) sağlayarak paralel ve seri koşunun `changed<T>()` dahil birebir aynı sonucu üretmesini garanti eder (`AppDesc::parallel_systems`, `ctx.set_parallel_systems(bool)`).
- **P2 (Paralel transform yayılımı):** `update_transforms(World&, JobSystem*)` kökleri `parallel_each` ile, çocuk hiyerarşisini ise düz dizi tabanlı derinlik seviyelerine ayırıp seviye başına `jobs->parallel_for` ile paralel yayar (MSVC Debug 100k entity derinlik=4: seri 577 ms → paralel 211 ms, önceki tabana göre ~7,2× hızlanma).
- **P3 (Ana iş parçacığından çıkan hücre G/Ç):** `MapRuntime::begin_load` içinde `.cell` dosya okuması (`assets->read`) ve JSON/hücre ayrıştırması tamamen `JobSystem` iş parçacığına taşındı.
- **P4 (Script verimi ve profiler):** Ara tablo ayırmadan eşleşen entity'leri dolaşan `astral.each({"A", "B"}, fn)` bağlaması (`astral.d.luau` ve `API.md` üretimi dahil) ve `ScriptHost::run` içinde modül başına `ASTRAL_PROFILE_SCOPE` (`<modül>.luau`) eklendi.
