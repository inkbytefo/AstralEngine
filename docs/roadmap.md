# Yol Haritası

Durum: ✅ bitti · 🚧 sürüyor · 🔜 sıradaki · ⬜ planlı

## Neredeyiz

| Faz | Kapsam | Durum |
|---|---|---|
| M0–M2 | Core, Platform, Vulkan RHI, kendi ECS'imiz, App | ✅ |
| M3, M3.5, M3.6 | Render graph; ray-traced aydınlatma (RT gölge, DDGI, RT yansıma, denoiser, TAA, bloom); render API v2 | ✅ |
| M4 | Fizik (Jolt): body'ler, raycast, temaslar, karakter | ✅ |
| M5a–c | ECS tamamlama, asset'ler (glTF, hot-reload), sahne ve prefab | ✅ |
| M6 | Paketleme: cook, BC7/BC5, `.apak`, `astral_add_game` | ✅ |
| M7 | İlk oyun: 3D Pong | ✅ |
| M8 | Geliştirici araçları, metin/HUD, ses, görüntü testleri | ✅ |
| M9 | Dünya ve harita sistemi: `.world` + `.ent`, hücre streaming, katmanlar, kayan orijin | ✅ |
| M10 | Shader dili (`.ashader`) ve materyaller (`.material`): yaprak, cam, lav, hologram, sky, post | ✅ |

## Plan v4 (2026-11-21)

Hedef: **Unreal 5 seviyesinde bir dünya sistemi** (açık dünyada da hızlı), **yapay zekâyla rahat yazılan,
spagettiye dönmeyen** oyun kodu, **gdshader benzeri özel shader/materyal** dili ve **hacim tabanlı VFX**
(CS2 dumanı gibi); bunların üstüne fizik, arazi, su ve hava. Su ve hava tek başına özellik değil; ortak
mekanizmaların birleşimi: materyal/shader dili, şeffaf geçiş, fizikte alanlar ve kuvvetler, hacimsel
render ve GPU partikülleri. Bu yüzden önce temeller (M8–M12), sonra özellikler (M13–M22).

v3'ten fark: kullanıcı öncelikleriyle **dünya/harita sistemi** (yeni M9) ve **modüler oyun kodu**
(ADR-0023, M11) öne alındı, shader işi bir dile (ADR-0022) dönüştü, M14 hacimsel VFX'i (ADR-0024) içeriyor.
Sonraki fazların numarası birer kaydı.

| Faz | Kapsam | Bağımlı | Durum |
|---|---|---|---|
| M8 | Geliştirici altyapısı: debug UI, debug çizim, profiler, metin/HUD, ses; görüntü testleri | — | ✅ |
| M9 | **Dünya ve harita sistemi:** `.world`, nesne başına dosya, hücreler + asenkron streaming, veri katmanları, bölüm örnekleri, kayan orijin ([ADR-0021](decisions/0021-world-and-level-system.md)) | M8 | ✅ |
| M10 | **Shader dili ve materyaller:** `.ashader` (gdshader benzeri), `.material` + örnekler, çalışma anında derleme + hot-reload, raster/RT/gölge/forward varyantları, alfa testi, şeffaf geçiş ([ADR-0022](decisions/0022-shader-and-material-language.md)) | M8 | ✅ |
| M11 | **Oyun kodu ve scripting:** özellik paketleri + SOLID kuralları + sınır denetimi ([ADR-0023](decisions/0023-modular-game-code.md)), Luau ([ADR-0017](decisions/0017-gameplay-scripting.md), [ADR-0026](decisions/0026-script-host-and-features.md)), input eylemleri, `AGENTS.md`, üretilen API belgesi | M9 | ✅ |
| M12 | Fizik altyapısı: katmanlar, mesh/heightfield, eklemler, sorgular; **alan sistemi** (rüzgâr, su hacmi) ve kuvvetler | M8 | 🔜 |
| M13 | Gelişmiş PBR: clearcoat, sheen, anizotropi, transmission, subsurface, decal, path tracer referansı | M10 | ⬜ |
| M14 | **Atmosfer ve VFX:** fiziksel gökyüzü, froxel sis, **hacimsel efektler (CS2 dumanı)**, GPU partikülleri, decal/iz, `.vfx` ([ADR-0024](decisions/0024-volumetric-vfx.md)) | M10, M12 | ⬜ |
| M15 | Arazi I: Terrain3D benzeri heightfield (clipmap, bölgeler, 32 katman, sculpt, delik, fizik, yerleştirme), dünya hücreleriyle stream ([ADR-0018](decisions/0018-terrain-and-sdf-world.md)) | M9, M10, M12 | ⬜ |
| M16 | Su I: okyanus ve göller (FFT dalga, kırılma, köpük, su altı, kıyılar) + dalgayı izleyen kaldırma kuvveti | M14, M15 | ⬜ |
| M17 | Hava: hacimsel bulutlar, hava durumu (yağmur, ıslaklık), ileri aerodinamik | M13, M14 | ⬜ |
| M18 | Su II: etkileşimli su (GPU sığ su simülasyonu, iz, sıçrama, nehir akışı) | M16 | ⬜ |
| M19 | SDF dünya ve yıkım: seyrek SDF hacimleri, çalışma anında CSG, yıkılabilir nesneler ([ADR-0018](decisions/0018-terrain-and-sdf-world.md)) | M12, M15 | ⬜ |
| M20 | Animasyon: iskelet, GPU skinning, blend, root motion, IK (ayak yerleştirme, bakış, uzanma), skinned RT | M10 | ⬜ |
| M21 | **Karakter fiziği:** ragdoll, kaslar, aktif ragdoll, Euphoria tarzı davranışlar (denge, darbe, düşüş, kalkma) ([ADR-0025](decisions/0025-physics-based-character-animation.md)) | M12, M20 | ⬜ |
| M22 | Dünya ölçeği II: HLOD, büyük ölçekli bitki örtüsü, GPU-driven culling, meshlet LOD | M9, M15, M19 | ⬜ |
| M23 | Fizik III: araçlar, kumaş/soft body (rüzgâr alanıyla), kopan parçalar, önceden kırılmış mesh'ler | M12, M19, M21 | ⬜ |
| — | Kılıç arenası (yer tutucu): animasyon ve aktif ragdoll olgunlaşınca | M20, M21 | ⬜ |

### İlkeler

1. **Önce mekanizma, sonra özellik.** Su render'ı "şeffaf geçiş + `.ashader` + alan" üstüne yazılır,
   kendi özel yolunu açmaz. Duman "hacimsel efekt + `volume` shader" üstüne.
2. **Tek veri, iki tüketici.** Rüzgâr ve su alanları bir kez tanımlanır; fizik (CPU) ve partikül, bitki,
   kumaş, bulut (GPU) aynı tanımı örnekler. Dumanın oyun ızgarası görüşü de görüntüyü de sürer.
3. **Veri ECS'te, davranış sistemlerde, içerik dosyalarda.** Yeni her component reflect edilir: sahne
   dosyası, editör paneli ve script bağlaması kendiliğinden gelir.
4. **SOLID her zaman** ([architecture.md](architecture.md), [ADR-0023](decisions/0023-modular-game-code.md)):
   yeni davranış yeni sistem/olayla eklenir, mevcut kod değiştirilmez; global durum yok; servisler arayüzle.
5. **Yapay zekâ dostu.** Küçük kalıplı dosyalar, reflection'dan üretilen API belgesi ve tip tanımları,
   headless testler, otomatik sınır denetimi. Ajan tahmin etmez, okur ve test eder.
6. **Her şey çalışırken değişir.** Shader, materyal, script, sahne, prefab, dünya hücresi hot-reload.
7. **Ölçmeden optimize yok, görmeden kabul yok.** Her faz profiler verisi ve görüntü testiyle (offscreen,
   CI'da lavapipe) biter; her fazın bir demo sahnesi olur.
8. **Doğruluk referansı.** M13'ten itibaren gerçek zamanlı sonuçlar birikimli path tracer referansıyla
   karşılaştırılır.

### Faz ayrıntıları

**M8 — Geliştirici altyapısı ve oyun temelleri.** ✅ Ayrıntı aşağıda (Tamamlanan fazlar → M8).

**M9 — Dünya ve harita sistemi.** ✅ Ayrıntı aşağıda (Tamamlanan fazlar → M9). Karar: [ADR-0021](decisions/0021-world-and-level-system.md).
- `.world` biçimi (dünya ayarları, ızgaralar, veri katmanları); `<harita>.entities/` klasöründe nesne başına
  dosya; tek `.scene` küçük haritalar için geçerli kalır.
- Cook: entity'leri sınır kutusuna göre hücrelere dağıtır, hücre başına ikili blok.
- Çalışma anı: `StreamingSource`, asenkron hücre yükleme (JobSystem), kare bütçeli birleştirme, histerezisli
  boşaltma, hücre farkı olarak saklanan oyun durumu, hücreler arası kalıcı referanslar.
- Veri katmanları (açma/kapama), bölüm örnekleri (`.scene` → dünyaya yerleştirme), `ctx.open_world`.
- Kayan orijin: dosyada `double`, çalışırken orijine göre `float`; fizik ve render geçmişi birlikte kayar.
- Araçlar: debug UI'da hücre görünümü, yükleme yarıçapları (debug çizim), profiler'da streaming istatistiği.
- *Çıkış:* 4×4 km test dünyası (binlerce prosedürel nesne), kamera uçarken takılmasız streaming; aynı
  dünyada gece katmanı aç/kapa; iki nesne dosyası ayrı ayrı düzenleniyor.

**M10 — Shader dili ve materyaller.** ✅ Ayrıntı aşağıda (Tamamlanan fazlar → M10). Karar: [ADR-0022](decisions/0022-shader-and-material-language.md).
- glslang ile çalışma anında derleme; `.ashader` başlık ayrıştırıcısı; built-in'ler; hata satır eşlemesi.
- `shader_type surface` → G-buffer, gölge, RT vuruş (uber-shader'a eklenen dal), forward/şeffaf varyantları;
  `vertex()` deformasyonu ve deforme mesh'ler için BLAS güncelleme politikası; alfa testi her yolda.
- `.material` (+ ebeveyn zinciri), uniform ipuçlarından otomatik materyal paneli, `MaterialOverride`;
  glTF materyalleri `standard.ashader` örneği olarak.
- Şeffaf/forward geçiş: sıralı şeffaflar, RT yansıma, ekran uzayı kırılma. `post` ve `sky` türleri.
- Cook'ta önceden derleme, diskte pipeline önbelleği.
- *Çıkış:* rüzgârda sallanan yaprak, cam, akan lav ve hologram materyalleri dosyadan; shader düzenlenince
  oyun durmadan değişiyor; raster ve RT görüntüleri aynı materyali gösteriyor.

**M11 — Oyun kodu ve scripting ✅** (sonuç: aşağıda "Tamamlanan fazlar"). Kararlar: [ADR-0023](decisions/0023-modular-game-code.md),
[ADR-0017](decisions/0017-gameplay-scripting.md).
- Özellik paketleri (`feature.json`, `install`), bağımlılık sırası, sınır denetimi (build/lint).
- `Astral::Script` (Luau): reflection'dan bağlama, sistem script'leri ve entity davranışları, script'te
  component, olaylar, hot-reload, sandbox; `astral.d.luau` ve `API.md` üretimi.
- Oyun çatısı: input eylemleri (tuş eşleme, gamepad), zamanlayıcılar/tween, tetik hacimleri, durum
  makinesi yardımcıları, kayıt sistemi.
- `AGENTS.md`: yapay zekâ ajanları için özellik iskeleti, kurallar, komutlar.
- *Çıkış:* Pong özellik paketlerine bölünüyor, kuralları Luau'da ve aynı testleri geçiyor.

**M12 — Fizik altyapısı ve alanlar.**
- Çarpışma katmanları; mesh, convex hull, heightfield ve compound şekiller; eklemler; shape cast ve overlap.
- Fizik materyalleri (yüzey tipi → ses ve VFX), görsel interpolasyon, Jolt'u JobSystem'e bağlama.
- **Alan sistemi:** rüzgâr (genel + yerel hacimler + türbülans + esinti), su hacimleri (yükseklik, akış,
  yoğunluk); CPU'da ve GPU'da aynı fonksiyon. Kuvvetler: kaldırma, su içi ve hava sürüklemesi.
- *Çıkış:* yüzen kutular, rüzgârla sürüklenen nesneler, eklemli köprü, heightfield zemin.

**M13 — Gelişmiş PBR.** Clearcoat, sheen, anizotropi, transmission, subsurface, ıslaklık; enerji koruyan
çoklu saçılma; detay haritaları, parallax, decal'lar; path tracer referans modu. Hepsi `.ashader`'da
`shading_model` seçenekleri olarak. *Çıkış:* materyal test sahnesi, gerçek zamanlı ve referans yan yana.

**M14 — Atmosfer ve VFX.** Karar: [ADR-0024](decisions/0024-volumetric-vfx.md).
- Fiziksel gökyüzü, günün saati, aerial perspective, gökyüzünden IBL ve DDGI beslemesi.
- Froxel hacimsel sis ve ışık huzmeleri; tek "katılımcı ortam" birleştiricisi.
- **Hacimsel efektler:** deterministik oyun ızgarası (flood fill, fizik sorgusuyla duvar bilgisi, mermi/
  patlama boşaltma, görüş sorgusu) + GPU görüntü hacmi (seyrek tuğla atlası, gürültü, rüzgâr, raymarch,
  DDGI ışığı, zamansal yeniden kullanım); `shader_type volume` (duman, ateş, gaz).
- GPU partikülleri (`shader_type particle`), decal/izler, `.vfx` bileşim dosyası.
- *Çıkış:* koridoru dolduran, kapıdan taşan, mermiyle delinen ve el bombasıyla açılan duman bombası
  (CS2 gibi); günün saati döngüsü; rüzgârda savrulan toz.

**M15 — Arazi I: heightfield (Terrain3D benzeri).** Karar: [ADR-0018](decisions/0018-terrain-and-sdf-world.md).
Bölgeli veri (`.aterrain`, dünya hücreleriyle stream), GPU clipmap, RT için kaba BLAS, 32 katmanlı
materyal (`.ashader` arazi türü), heightfield fizik + delikler, motor içi fırçalar, bitki/kaya yerleştirme.
*Çıkış:* motor içinde boyanıp şekillendirilmiş 2×2 km arazi; üstünde fizik ve karakter.

**M16 — Su I: okyanus ve göller.** FFT okyanus, göl yüzeyleri, kırılma, RT yansıma, emilim, köpük, su altı;
kıyı köpüğü arazi yüksekliğinden; kaldırma kuvveti dalgayı CPU'da aynı fonksiyondan örnekler.
*Çıkış:* kıyıya vuran dalgalı denizde sallanan tekne ve yüzen kasalar.

**M17 — Hava: bulutlar ve hava durumu.** Hacimsel bulutlar, bulut gölgeleri; bulutluluk/yağış/rüzgâr/
ıslaklık zamanla değişir ve sis, yağmur partikülleri, ıslak yüzeyler, su birikintilerini sürer; balistik
ve kanat kaldırma kuvveti. *Çıkış:* açık havadan fırtınaya geçiş; ıslanan arazi.

**M18 — Su II: etkileşimli su.** GPU sığ su simülasyonu: izler, dalgalar, sıçramalar, nehirler; akış
fizikte kuvvet. *Çıkış:* suya düşen nesne dalga yapıyor; nehirde sürüklenen kütükler.

**M19 — SDF dünya ve yıkım.** Seyrek tuğla SDF hacimleri (mağara, kemer), GPU'da mesh'e çevirme, çalışma
anında CSG (krater, tünel), yıkılabilir nesneler. *Çıkış:* patlama kraterleri, kazılan tünel, delinen duvar.

**M20 — Animasyon.** Yeni `Astral::Animation` modülü: iskelet, poz, GPU skinning, glTF animasyonları, blend
ağaçları, root motion, morph target, IK (iki kemik, FABRIK zinciri, bakış, ayak yerleştirme, el uzanma),
skinned BLAS. *Çıkış:* animasyonlu karakter merdivende ayakları basamağa oturarak yürür; RT gölge ve
yansımada doğru.

**M21 — Karakter fiziği ([ADR-0025](decisions/0025-physics-based-character-animation.md)).** `.ragdoll` (Jolt
ragdoll + swing-twist eklemler), kaslar (eklem başına güç/sönüm, kas profilleri, yorgunluk/acı), durumlar
(kinematik → güçlü → kısmi → aktif → pasif → kalkma), Euphoria tarzı davranışlar (denge, sendeleme, darbe
tepkisi, düşüşü yakalama, başını koruma, kalkma), fizik LOD'u ve ragdoll bütçesi. Yeni `Astral::Character`
modülü. *Çıkış:* kılıç arenası yer tutucusu: vurulan karakter sendeler, düşerken elleriyle yakalanır,
kalkar; aynı girdiyle aynı sonuç.

**M22 — Dünya ölçeği II.** HLOD (hücre vekil mesh'leri), büyük ölçekli bitki örtüsü, GPU-driven culling,
meshlet LOD. *Çıkış:* birkaç km² açık dünya, uzakta da dolu ufuk, sabit kare süresi.

**M23 — Fizik III.** Araçlar, kumaş ve soft body (rüzgâr alanıyla), yıkım parçaları dinamik body olarak,
önceden kırılmış mesh'ler.

### Neden bu sıra

- **M8 önce:** paneller, profiler ve görüntü testleri olmadan sonraki hiçbir şey ayarlanamaz ve doğrulanamaz.
- **Dünya sistemi (M9) içerikten önce:** harita biçimi sonradan değişirse bütün içerik taşınır. Arazi,
  SDF hacimleri ve streaming aynı hücre modeline oturmalı. Ayrıca "harita nedir" sorusu oyun kodunu da
  belirliyor (dünya ayarları, oyun kuralları entity'si).
- **Shader dili (M10) görsel her şeyden önce:** PBR, su, arazi, VFX hepsi `.ashader` türleri.
- **Oyun kodu düzeni (M11) erken:** oyun kodu büyümeden kurallar ve araçlar yerinde olmalı; sonraki
  demolar Luau ve özellik paketleriyle hızlı kurulur.
- **Fizik alanları (M12) VFX'ten önce:** duman duvarları fizik sorgusuyla bulur, rüzgârla taşınır.
- **Arazi sudan önce; su ve hava iki dalgada; SDF ve yıkım araziden sonra; animasyon ve ragdoll sonra**
  (v3'teki gerekçeler geçerli).

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

- **Görüntü testleri (M8.6):** `tests/golden`: örnek oyun ekran dışında bir kare yakalar, `astral-golden`
  kayıtlı referansla karşılaştırır (kanal başına eşik + izin verilen kötü piksel oranı), fark varsa kırmızı
  işaretli fark görüntüsü yazar. Üç sahne: scene_viewer, rt_showcase, Pong (320×180, lavapipe). GPU yoksa
  test atlanır. Referans güncelleme: `ASTRAL_UPDATE_GOLDEN=1 ctest -L golden`.

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
