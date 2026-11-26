# Render (`Astral::Render`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core, Astral::Gpu, Astral::Shader (`modules/render/CMakeLists.txt:60-63`); derleme zamanında stb_truetype ve Dear ImGui'nin Roboto fontu indirilir (`modules/render/CMakeLists.txt:4-15`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Render modülü, ECS'i bilmeyen bir kare çizicidir: çağıran taraf her kare `RenderScene` (mesh örnekleri, SDF şekilleri, ışıklar, güneş,
ortam, decal, duman, partikül, debug çizgisi, 2D katman) adlı düz veriyi verir, `Renderer::render()` bunu bir `gpu::CommandList`'e
kaydeder (`modules/render/include/astral/render/renderer.hpp:245-265`, `:425`). İki ana yol vardır: cihaz donanımsal ray query destekliyorsa
hibrit ışın izlemeli yol (RT gölge, DDGI, yansıma, denoiser, TAA, bloom), değilse raster yol; ayrıca RT donanımı gerektiren bir path tracer
referans modu vardır. Modül sahne yönetimi, culling hiyerarşisi, animasyon/skinning, LOD, ses ya da asset yükleme yapmaz; dosyadan materyal
okuma `Astral::App` içindeki `MaterialLibrary`'dedir (`modules/app/src/material_library.cpp`), bu modül yalnız `.material` çözümleme ve
`.ashader` derleme yardımcılarını verir.
İlgili modüller: [gpu.md](gpu.md) (RHI), [shader.md](shader.md) (`.ashader` dili, SPIR-V önbelleği), [app.md](app.md) (sahneden `RenderScene` çıkarma, `MaterialLibrary`).

## Public API

Hepsi `modules/render/include/astral/render/` altındadır. Renderer'ın tüm yüzeyi tek sınıftadır (`Renderer`); diğer tipler düz veridir.

### Renderer cephesi

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Renderer(device, target_format, settings)` | Store'ları, pass'leri ve RenderGraph'ı kurar. `ray_tracing` ayarı açık ve cihaz destekliyorsa RT pipeline'larını hazırlar; başaramazsa raster yola düşer | `modules/render/include/astral/render/renderer.hpp:365`, `modules/render/src/renderer.cpp:81-86` |
| `render(cmd, target, camera, scene)` | Kareyi `cmd`'ye kaydeder, `target` ColorTarget durumunda biter. Halka tamponlar GPU ile kendiliğinden senkronlanır | `modules/render/include/astral/render/renderer.hpp:425`, `modules/render/src/renderer.cpp:217-233` |
| `render_ui(cmd, target, ui, clear)` | Hedefi temizleyip yalnız 2D katmanları çizer (3B sahne başka bir render target'a çizildiyse) | `modules/render/include/astral/render/renderer.hpp:429`, `modules/render/src/renderer.cpp:235-246` |
| `create_mesh` / `update_mesh` / `destroy` | Mesh yükler (bloklayıcı). `update_mesh` aynı handle'ın arkasındaki geometriyi değiştirir. İndeks vertex sayısını aşıyorsa reddeder | `modules/render/include/astral/render/renderer.hpp:371-374`, `modules/render/src/stores/mesh_store.cpp:18-22` |
| `create_texture` / `update_texture` / `destroy` | RGBA8 görüntü (mip zinciri otomatik) ya da hazır BC7/BC5 zinciri yükler | `modules/render/include/astral/render/renderer.hpp:377,407-408` |
| `create_render_target` / `resize_render_target` / `native_target` | Offscreen renkli hedef; UI katmanında örneklenebilir | `modules/render/include/astral/render/renderer.hpp:379-383` |
| `create_material` / `update_material` / `destroy` / `default_material` | Materyal handle'ı; geçersiz handle varsayılan materyale düşer | `modules/render/include/astral/render/renderer.hpp:410-416` |
| `create_shader(SurfaceShaderBuild \| ScreenShaderBuild)` / `destroy` | Derlenmiş `.ashader` için pipeline'lar; materyal `MaterialDesc::shader` ile bağlanır | `modules/render/include/astral/render/renderer.hpp:387-390` |
| `set_shader_compile_services` / `set_job_system` / `set_busy_callback` | SPIR-V önbelleği + iş sistemi, mip işçileri, uzun yüklemede pencereyi canlı tutma kancası (hepsi ödünç alınır) | `modules/render/include/astral/render/renderer.hpp:394-400` |
| `shader_error()` | Özel yüzeylerin RT shader'larına girememe nedeni ("" = sorun yok) | `modules/render/include/astral/render/renderer.hpp:403` |
| `supports(ImageFormat)` | BC için `gpu::Feature::TextureCompressionBC` gerekir | `modules/render/include/astral/render/renderer.hpp:405`, `modules/render/src/stores/texture_store.cpp:135-137` |
| `set_settings` / `settings` | Çalışma anında ayar değişimi; ilgili geçmiş sıfırlanır | `modules/render/include/astral/render/renderer.hpp:420-421`, `modules/render/src/renderer.cpp:183-205` |
| `stats` / `gpu_timings` / `gpu_frame_ms` / `ray_tracing` / `reset_history` | İstatistik, pass başına GPU süresi (birkaç kare gecikmeli), aktif yol, kamera kesmesinde geçmiş sıfırlama | `modules/render/include/astral/render/renderer.hpp:432-439` |
| `build_mips(image, jobs)` | Kutu filtreli RGBA8 mip zinciri; renk görüntüleri doğrusal uzayda ortalanır, iş sistemiyle aynı baytları üretir | `modules/render/include/astral/render/renderer.hpp:44`, `modules/render/src/stores/texture_store.cpp:97-124` |

### Sahne verisi ve ayarlar

| Tip | Ne yapar | Kaynak |
|---|---|---|
| `RenderScene` | Kare verisi: `meshes`, `sdf`, `lights`, `rect_lights`, `decals`, `sun`, `environment`, `debug_lines`, `ui`, `time`, `sky`, `post`, `world_origin`, `smoke`, `particles`. Span'ler `render()` dönene kadar geçerli kalmalı | `modules/render/include/astral/render/renderer.hpp:245-265` |
| `MeshInstance` | Mesh + materyal + dönüşüm; `previous_transform` verilirse nesne başına hareket vektörü | `modules/render/include/astral/render/renderer.hpp:143-149` |
| `MaterialDesc` | PBR faktörleri + dokular + M13 katmanları + `TextureMapping` + özel shader parametre bloğu | `modules/render/include/astral/render/renderer.hpp:98-127` |
| `ClearcoatDesc`, `SheenDesc`, `AnisotropyDesc`, `TransmissionDesc`, `SubsurfaceDesc`, `DetailDesc` | M13 gelişmiş yüzey katmanları | `modules/render/include/astral/render/renderer.hpp:47-83` |
| `TextureProjection`, `TextureMapping` | `MeshUV`, `PlanarXY/XZ/YZ`, `Triplanar` | `modules/render/include/astral/render/renderer.hpp:85-95` |
| `Decal` | Kutu izdüşümlü deferred decal (boya, ıslaklık, normal) | `modules/render/include/astral/render/renderer.hpp:130-140` |
| `SdfShape`, `SdfPrimitive`, `SdfOperation` | Liste sırasıyla birleşen SDF şekilleri (küre, kutu, torus, kapsül, silindir; birleşim/çıkarma/kesişim/yumuşak varyantlar) | `modules/render/include/astral/render/renderer.hpp:151-168` |
| `PointLight`, `RectLight`, `DirectionalLight` | Işık tipleri. `RectLight` yalnız ışın izlemeli yolda; **SpotLight yok** | `modules/render/include/astral/render/renderer.hpp:170-203` |
| `Environment` | Gök/zemin rengi, ortam gücü, pozlama, aerial perspective, froxel sis | `modules/render/include/astral/render/renderer.hpp:205-214` |
| `SmokeVolumeView`, `ParticleBurst` | Yerel duman hacmi, GPU partikül patlaması | `modules/render/include/astral/render/renderer.hpp:217-242` |
| `GiSettings` | DDGI ızgarası (probe sayısı, ışın sayısı, histerezis, hacim) | `modules/render/include/astral/render/renderer.hpp:268-274` |
| `RenderSettings` | Tüm kalite/teknik ayarları (varsayılanlar aşağıda tablo) | `modules/render/include/astral/render/renderer.hpp:280-318` |
| `RenderStats`, `PathTracerReset` | Kare istatistiği; path tracer birikiminin neden yeniden başladığı | `modules/render/include/astral/render/renderer.hpp:322-352` |
| `DebugView` | `None, Albedo, Normals, DiffuseLight, Specular, Motion` (RT yolu için) | `modules/render/include/astral/render/renderer.hpp:278` |

### Shader, materyal dosyası, RenderGraph, yardımcılar

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `compile_surface_shader(source, name, services)` | `.ashader` yüzeyini G-buffer, gölge, (alfa testliyse) gölge fragment'i ya da forward (`blend_*`) aşamalarına çevirir; iş parçacığı güvenli | `modules/render/include/astral/render/surface_shader.hpp:68` |
| `compile_screen_shader` | `post`, `sky`, `particle`, `volume` shader türleri | `modules/render/include/astral/render/surface_shader.hpp:63` |
| `compose_rt_surfaces`, `compile_rt_variants` | Özel `surface()` fonksiyonlarını RT compute shader'larına gömer (5 shader) | `modules/render/include/astral/render/surface_shader.hpp:81,93` |
| `resolve_material`, `evaluate_uniforms` | `.material` ebeveyn zinciri (en çok 16 derinlik, döngü hatadır) ve değerlerin parametre bloğuna çevrilmesi; saf veri, GPU/dosya sistemi yok | `modules/render/include/astral/render/material_file.hpp:40,56`, `modules/render/src/material_file.cpp:9,29,73` |
| `RenderGraph`, `PassBuilder`, `PassContext`, `RGTexture` | Her kare yeniden kurulan kare grafiği; bariyer, render pass açma, geçici texture havuzu, kullanılmayan pass'leri ayıklama | `modules/render/include/astral/render/render_graph.hpp:71`, `:37`, `:60`, `:17` |
| `DebugDraw` | Kare içi debug çizgi toplayıcı (kutu, küre, kapsül, ok...) | `modules/render/include/astral/render/debug_draw.hpp:31` |
| `UiLayer`, `UiCommand`, `UiCanvas`, `Font` | Piksel uzayında 2D geometri; SDF font atlası (Latin-1 + Latin Extended-A + ₺) | `modules/render/include/astral/render/ui.hpp:43,31`, `modules/render/include/astral/render/text.hpp:98,52` |
| `MeshData`, `make_cube/plane/sphere`, `ImageData` | Mesh ve görüntü veri tipleri, ilkel mesh üreticileri | `modules/render/include/astral/render/mesh.hpp:18,24-28`, `modules/render/include/astral/render/image.hpp:13` |

## Nasıl çalışır

### Genel yapı, sahiplik, iş parçacığı

- `Renderer` bir pimpl'dir. `Renderer::Impl` şunlara sahiptir: `RenderGraph`, `TextureStore`, `ShaderStore`, `MeshStore`, `MaterialStore`,
  kare örnek listesi (`FrameInstances`), kare halkası (`FrameRing`), tüm pass'ler (`RenderPasses`), istatistik ve `TemporalState`
  (`modules/render/src/renderer.cpp:28-48`). Dosya düzeni: `src/stores/` (kaynak havuzları), `src/frame/` (kare tamponları ve örnek
  hazırlama), `src/passes/` (her biri kendi pipeline ve geçmiş texture'larına sahip pass'ler), `src/paths/` (raster ve ışın izlemeli kare
  bileşimi), `src/gpu_layouts.hpp` (`shaders/include/astral/scene.glsl` ile eşleşen, `static_assert` denetimli GPU yapıları).
- **Çıkar → çiz sözleşmesi:** Renderer sahneyi saklamaz; çağıran her kare `RenderScene` doldurur
  (`modules/render/include/astral/render/renderer.hpp:358-360` açıklaması, ADR-0012). Mesh/materyal/texture/shader kaynakları handle'dır
  (`Handle<Tag>`, nesil sayaçlı havuz); silinmiş handle güvenlidir: mesh atlanır, materyal varsayılana düşer
  (`modules/render/src/frame/instances.cpp:36`, `modules/render/src/stores/material_store.cpp:49-51`).
- **İş parçacığı:** Renderer'ın kare kaydı tek iş parçacığındadır. `Impl` düz üyelerden oluşur, kilit/atomik yoktur
  (`modules/render/src/renderer.cpp:28-48`); bunun belgelenmiş bir sözleşmesi yoktur, yani "tek iş parçacığından çağır" bir çıkarımdır
  (doğrulanmadı). İş sistemi yalnız ödünç verildiğinde kullanılır: mip zincirleri (`build_mips`) ve shader derlemeleri. `compile_surface_shader`
  iş parçacığı güvenlidir (`modules/render/include/astral/render/surface_shader.hpp:66`).
- **Kare halkası:** 3 slot (`ring_size`, `modules/render/src/frame/frame_slot.hpp:13`); `FrameRing::acquire` GPU henüz o slotu okuyorsa
  CPU'da bekler (`modules/render/src/frame/frame_slot.hpp:60-66`). Slot başına büyüyen, host-görünür `GrowBuffer`'lar: instances, lights,
  shapes, frame, rects, decals, materials, params, debug_lines, ui, fog_volumes, fog_density (`modules/render/src/frame/frame_slot.hpp:43-45`).
  Büyüme başarısız olursa eski tampon ve kapasite korunur, sonraki kare yeniden dener (`modules/render/src/frame/frame_slot.hpp:21-34`,
  test: `modules/render/tests/msr_regression_test.cpp` "REN03").
- **Her kare yeniden yüklenenler:** `GpuFrame` (720 bayt, `modules/render/src/gpu_layouts.hpp:123-149`), tüm örnekler, ışıklar, SDF şekilleri,
  decal'lar, tüm materyal tablosu ve `.ashader` parametre bloğu. Materyal tablosu her karede yeniden çevrilir ve yüklenir
  (`modules/render/src/stores/material_store.cpp:103-134`, `modules/render/src/frame/instances.cpp:31`); kirli takibi yoktur.
- **Örnek hazırlama** (`modules/render/src/frame/instances.cpp:28-82`): yok edilmiş mesh'ler atlanır; `blend_*` yüzeyler ayrılır; opak
  örnekler önce shader'a, sonra mesh indeksine göre sıralanır (aynı mesh+shader koşusu tek instanced çizim); şeffaflar nesne orijininin
  kameraya uzaklığına göre arkadan öne sıralanır (parça başına değil).
- **TLAS** (yalnız RT yolu): örnek listesi son başarılı GPU gönderimindekiyle aynıysa yeniden kullanılır, değilse yeniden kurulur
  (`modules/render/src/frame/instances.cpp:132-160`). Karşılaştırma her karede CPU'da yapılır. Kurulamazsa `render_ray_traced_path` kareyi
  sessizce bırakır (`modules/render/src/paths/ray_traced_path.cpp:309`; hata raporunun nerede verildiği doğrulanmadı).

### Render yolları: raster, hibrit ışın izlemeli, path tracer

| | Raster yol | Hibrit ışın izlemeli yol | Path tracer |
|---|---|---|---|
| Seçim | `m.rt == false` | `m.rt && !settings.path_tracer` | `m.rt && settings.path_tracer` |
| Koşul | her cihaz | `RenderSettings::ray_tracing` ve `gpu::Feature::RayTracing` ve RT pipeline'ları derlendi | aynı; RT yoksa `path_tracer` ayarı sessizce yok sayılır ve raster çizilir |
| Kare kaydı | `modules/render/src/paths/raster_path.cpp:64-127` | `modules/render/src/paths/ray_traced_path.cpp:229-262` | `modules/render/src/paths/ray_traced_path.cpp:265-294` |
| Direkt ışık | güneş (3×3 PCF gölge haritası) + SDF yumuşak gölge, nokta ışıklar (gölgesiz) | RT gölgeli güneş konisi, nokta + rect ışık (ReSTIR ya da RIS) | NEE + MIS, tüm ışıklar |
| Dolaylı ışık | gökyüzü/zemin gradyanı (ya da `sky_ibl`) | DDGI + 1 ışınlı yakın alan GI | çok sekmeli yol izleme (`path_tracer_max_bounces`) |
| Yansıma | yalnız ortam (gökyüzü) | tek ışın/piksel GGX VNDF + rect MIS | yansız (clamp'li, bkz. zayıf yanlar) |
| AA / zamansal | yok (MSAA/FXAA/TAA yok) | jitter'lı TAA + SVGF benzeri denoiser | birikim (jitter'sız projeksiyon, alt piksel örnekleme) |
| Bloom | yok | var | var |
| Rect ışık | desteklenmez (`rects = 0`) | var | var |

Seçim `Renderer` kurulurken ve `set_settings` içinde yapılır; RT açıldığında raster'da yaratılmış mesh'lerin BLAS'ları sonradan kurulur
(`modules/render/src/renderer.cpp:85`, `modules/render/src/renderer.cpp:188-194`, `modules/render/src/stores/mesh_store.cpp:79-91`). Raster yolda RT pipeline'ı hiç
derlenmez; `Renderer::render` yolu `m.rt` ile seçer (`modules/render/src/renderer.cpp:224-232`).

### RenderGraph

- Kullanım: `begin_frame → import/create → add_pass(isim, setup, execute) → execute(cmd)`. `setup` pass'in texture kullanımını bildirir
  (`read`, `write_storage`, `color`, `depth`, `depth_read`, `copy_src/dst`, `side_effect`); pass'ler `transition()` ya da `begin_rendering()`
  çağırmaz (`modules/render/include/astral/render/render_graph.hpp:37-58`).
- `execute` önce geriye doğru ayıklar: bir pass, bir import'a ya da yaşayan bir sonraki pass'in girdisine yazıyorsa ya da `side_effect`'liyse
  yaşar (`modules/render/src/render_graph.cpp:163-178`). Sonra yaşayan pass'lerin kullanım birleşiminden texture usage'ı çıkarır ve geçici
  texture'ları havuzdan alır; havuz anahtarı tam boyut+format+usage eşleşmesidir ve kullanılmayan girdi 3 kare sonra bırakılır
  (`modules/render/src/render_graph.cpp:81,145-161,235-250`). Başarısız ayırma havuzlanmaz, sonraki kare yeniden dener.
- Her pass için her erişimde `cmd.transition` kaydedilir, renk/derinlik erişimleri `begin_rendering`'e dönüştürülür
  (`modules/render/src/render_graph.cpp:210-233`). Bir pass bir texture'ı yalnız tek biçimde kullanabilir; ikinci kullanım hata raporlanır ve yok
  sayılır (`modules/render/src/render_graph.cpp:133-141`).
- **Sınırlar:** pass sırası eklenme sırasıdır (yeniden sıralama yok); geçici texture'lar kare içinde bellek paylaşmaz (aliasing yok: `taken`
  bayrağı kare boyunca durur); tek komut listesi (async compute yok); kayıt tek iş parçacığında. Buffer device address ile erişilen tamponlar
  (ReSTIR rezervuarları, DDGI ışın tamponu, partikül durumu) grafikte izlenmez: `side_effect()` ve elle `cmd.memory_barrier()` kullanılır
  (`modules/render/src/passes/rt_lighting_pass.cpp:49,66`, `modules/render/src/passes/ddgi_pass.cpp:167`,
  `modules/render/src/passes/particle_pass.cpp:81,104`).
- **GPU profili:** timestamp varsa pass başına süre ölçülür; 4 slotluk sorgu halkası, en çok 128 pass, sonuç beklenmeden birkaç kare sonra okunur
  ve yalnız submit edilmiş listeler "pending" sayılır (`modules/render/src/render_graph.cpp:93-124,192-208,374-381`).

### Pass listesi

Raster yol (`modules/render/src/paths/raster_path.cpp:96-120`):

```
[sky IBL bake] → shadow → gbuffer → sdf → decals → lighting → sky → aerial → fog (integrate+composite)
              → [sdf.depth_merge] → transparent → tonemap → particles → post → debug.lines → ui
```

Hibrit ışın izlemeli yol (`modules/render/src/paths/ray_traced_path.cpp:229-262`):

```
[sky IBL bake] → gbuffer → sdf → decals → ddgi.trace → ddgi.update.irradiance → ddgi.update.distance
 → [restir.temporal → restir.spatial] → rt.lighting → denoise.temporal → denoise.atrous ×N → composite → sky → aerial → taa
 → [shadow (yalnız sis/duman varsa)] → froxel passes → froxel composite → [sdf.depth_merge] → transparent → bloom → tonemap
 → particles → post → debug.lines → ui
```

Path tracer (`modules/render/src/paths/ray_traced_path.cpp:265-294`):

```
[sky IBL bake] → [gbuffer → sdf  (yalnız derinlik gerekiyorsa)] → path_tracer → sky → aerial → [sdf.depth_merge] → transparent
 → bloom → tonemap → particles → post → debug.lines → ui
```
Derinlik, özel sky materyali, aerial perspective, şeffaf çizim, debug çizgisi ya da partikül varsa üretilir
(`modules/render/src/paths/ray_traced_path.cpp:271-278`); aksi halde G-buffer hiç kaydedilmez.

| Pass | Ne yapar | Kaynak |
|---|---|---|
| `shadow` | Güneşten ortografik derinlik haritası (D32F, `shadow_map_size`). Tüm mesh sınırlarına (ışık uzayında) tek harita sığdırılır; iki yüz çizilir; `shadows_disabled` shader'lar atlanır. RT yolunda yalnız sis/duman açıkken eklenir | `modules/render/src/passes/shadow_pass.cpp:15-50,95-144`, `modules/render/src/paths/ray_traced_path.cpp:220-227` |
| `gbuffer` | 5 renk hedefi + D32F: albedo RGBA8 (a=pürüzlülük), normal RGBA16F (a=metalik), emissive RGBA16F (a=şading modeli+1), velocity RG16F, material_ext RGBA16F. Mesh+shader koşularında instanced çizim; CPU'da yalnız standart materyalli opak gruplar için görüş hacmi elemesi (grupta tek örnek görünürse grubun tamamı çizilir). POM (12 adım), detay normal/albedo, ıslaklık, doku projeksiyonu burada | `modules/render/src/passes/gbuffer_pass.cpp:10-41,49-50,70-113`, `modules/render/shaders/gbuffer.frag:49` |
| `sdf` | Her piksel için SDF şekil listesini raymarch eder, kendi G-buffer'ını yazar (albedo RGBA8, normal RGBA16F, derinlik R32F, velocity RG16F); şekil yoksa pass eklenmez. Velocity yalnız kamera hareketini taşır | `modules/render/src/passes/sdf_pass.cpp:24-54` |
| `sdf.depth_merge` | Yalnız şeffaf çizim veya debug çizgi varsa SDF derinliğini mesh derinliğine yazar | `modules/render/src/passes/sdf_pass.cpp:56-73` |
| `decals` | `RenderScene::decals` doluysa G-buffer'ın dört hedefinin tam çözünürlüklü kopyasını yazar (albedo, normal, emissive, material_ext); boşsa maliyet yok | `modules/render/src/passes/decal_pass.cpp:13-49` |
| `lighting` (raster) | G-buffer'dan HDR: güneş (3×3 PCF × SDF yumuşak gölge), nokta ışıklar, gökyüzü ortamı, tüm M13 modelleri. 16'dan fazla nokta ışıkta 8×8 karo başına ışık listesi (kapasite 128, taşarsa tam liste) | `modules/render/shaders/lighting.comp:17-35,78-111,113-205` |
| `ddgi.trace` | Her prob kare başına `rays_per_probe` ışın atar (Fibonacci küresi, her kare döner); vuruşlar doğrudan ışık + önceki karenin probları ile aydınlatılır | `modules/render/src/passes/ddgi_pass.cpp:139-154`, `modules/render/shaders/ddgi_trace.comp:1-45` |
| `ddgi.update.irradiance` / `.distance` | Işınlar oktahedral atlaslara (irradiance 8×8, mesafe momentleri 16×16, 1 texel sınır) harmanlanır; adaptif histerezis; gömülü prob tespiti (arka yüz vuruşları > ışınların 1/4'ü) | `modules/render/src/passes/ddgi_pass.cpp:156-173`, `modules/render/shaders/ddgi_update.comp:25-28,99` |
| `restir.temporal` / `restir.spatial` | Nokta+rect ışık toplamı > 2 ve `restir_di` açıksa: 24 aday + görünürlük ışını + zamansal yeniden kullanım (M ≤ 20); sonra 3 komşulu (yarıçap 24 px) uzamsal yeniden kullanım (M ≤ 60) | `modules/render/src/passes/rt_lighting_pass.cpp:37-72`, `modules/render/shaders/include/astral/restir.glsl:11-13`, `modules/render/shaders/restir_di_spatial.comp:15-16` |
| `rt.lighting` | Piksel başına: güneş (açısal yarıçaplı koni), yerel ışıklar (ReSTIR rezervuarı ya da doğrudan/RIS), yakın alan GI (1 kosinüs ışını, `near_gi_radius` içinde vurursa o yüzeyin gerçek ışığı, yoksa DDGI), yansıma (GGX VNDF, clearcoat/anizotropi/şeffaflık dalları, rect MIS) | `modules/render/src/passes/rt_lighting_pass.cpp:74-130`, `modules/render/shaders/rt_lighting.comp:67-307` |
| `denoise.temporal` | Diffuse ve speküler ayrı reprojekte edilir (speküler için sanal yansıma derinliği, `specular.a`), 3×3 varyans kırpması, parlaklık momentleri | `modules/render/src/passes/denoise_pass.cpp:65-115`, `modules/render/shaders/denoise_temporal.comp` |
| `denoise.atrous` | 5×5 kenar duyarlı dalgacık (B3 spline × normal × düzlem mesafesi × varyansa göre parlaklık); adım `1<<i`; roughness < ~0.08 olan speküler filtrelenmez. Varsayılan 3 iterasyon, en çok 5 | `modules/render/src/passes/denoise_pass.cpp:117-162`, `modules/render/shaders/denoise_atrous.comp:53-80` |
| `composite` | `albedo × diffuse + env_brdf × speküler + emisyon`; gökyüzü; rect ışıklar kameraya görünür; `debug_view` çıktıları | `modules/render/shaders/composite.comp:12-66` |
| `sky` | İsteğe bağlı özel `shader_type sky`, yalnız derinliği 1 olan (mesh ve SDF) piksellerde | `modules/render/src/passes/sky_pass.cpp:78-99` |
| `aerial perspective` | `aerial_extinction > 0` ise opak yüzeylere uzaklığa göre atmosfer rengi | `modules/render/src/passes/aerial_pass.cpp:9-12,20-48` |
| `taa` | Halton(2,3) jitter (16 örnek), en yakın derinlikle reprojeksiyon, Catmull-Rom geçmiş, YCoCg varyans kırpma (γ=1.25), Karis sıkıştırma; şeffaflar geçmişe girmez | `modules/render/src/passes/taa_pass.cpp:43-76`, `modules/render/shaders/taa.comp:48-100`, `modules/render/src/paths/ray_traced_path.cpp:145-152` |
| froxel (`froxel integrate`, `froxel composite`, `volume material atlas`, `volume sunlight atlas`, `fog probe radiance`) | Küresel sis + yerel duman: 16 dilimli froxel ızgarası (ekran ≤ 240×135 hücre, dilimler t² dağılımlı), Henyey-Greenstein g=0.45, güneş görünürlüğü raster gölge haritasından 1 örnekle, isteğe bağlı DDGI ortam ışığı | `modules/render/src/passes/fog_pass.cpp:213-266`, `modules/render/shaders/fog_integrate.comp:23-66` |
| `transparent` | `blend_mix`/`blend_add` yüzeyler HDR'a, arkadan öne, derinlik testli, ileri aydınlatma (güneş + tüm nokta ışıklar + gökyüzü ortamı, gölgesiz), aerial ve sis uygulanır | `modules/render/src/passes/transparent_pass.cpp:5-36`, `modules/render/src/surface_shader.cpp:192-206` |
| `bloom` | 13 dokunuşlu aşağı örnekleme (ilk seviyede Karis), en çok 6 seviye, eşit ağırlıklı yukarı örnekleme; `bloom_strength <= 0` ise atlanır | `modules/render/src/passes/bloom_pass.cpp:32-108`, `modules/render/src/passes/bloom_pass.hpp:26` |
| `tonemap` | `mix(hdr, bloom, bloom_strength)` → pozlama (`Environment::exposure`) → ACES → UNORM hedefte sRGB kodlama. Otomatik pozlama yok | `modules/render/src/passes/tonemap_pass.cpp:20-43`, `modules/render/shaders/tonemap.frag:8-20` |
| `particles` | GPU partikül simülasyonu (ekran uzayı derinlik çarpışması) + billboard çizimi; **tonemap sonrası, LDR hedefe** | `modules/render/src/passes/particle_pass.cpp:65-159`, `modules/render/src/paths/ray_traced_path.cpp:258-259` |
| `post` | `shader_type post` zinciri (ping-pong); girdi yalnız LDR ekran görüntüsü | `modules/render/src/passes/post_pass.cpp:13-42` |
| `debug.lines` | Derinlik testli (önce) ve `on_top` (sonra) çizgiler | `modules/render/src/passes/debug_lines_pass.cpp:30-70` |
| `ui` | `UiLayer`'lar sırayla, kırpma dikdörtgeni ve SDF yumuşaklığıyla | `modules/render/src/passes/ui_pass.cpp:23-50,87-107` |
| `path_tracer` | Tek dispatch: 1 örnek/piksel/kare, RGBA32F ping-pong birikim; ışık NEE + MIS, Russian roulette (3. sekmeden sonra), BSDF örnekleme (clearcoat/spec/diffuse lob seçimi), kırılma + Beer-Lambert hacim takibi, SDF'ler, özel `.ashader`'lar | `modules/render/src/passes/path_tracer_pass.cpp:30-58`, `modules/render/shaders/path_tracer.comp:174-380` |

### Malzemeler

- **İki tür:** shader'ı geçersiz olan `MaterialDesc` standart PBR'dir; `MaterialDesc::shader` geçerliyse yüzeyi özel `.ashader` çizer ve
  `params`/`textures` o shader'ın parametre bloğunu doldurur (`modules/render/include/astral/render/renderer.hpp:117-126`).
- **GPU tablosu:** `MaterialDesc` → `GpuMaterial` (240 bayt) dönüşümü `MaterialStore::to_gpu`'dadır; tablo handle slotuyla indekslenir, ölü slotlar
  varsayılan materyalin değerlerini tutar (`modules/render/src/stores/material_store.cpp:69-101,105-108`,
  `modules/render/src/gpu_layouts.hpp:79`). Geçersiz parametre bloğu (>16384 kelime ya da doku ofseti bu sınırı aşıyor) `create/update`'te reddedilir
  (`modules/render/src/stores/material_store.cpp:19-35`).
- **`.material` dosyası** (saf veri, `Astral::Render` yalnız çözümler): `{"format": "astral.material", "version": 1, "type": "pbr" | "shader": yol,
  "parent": yol, "values": {...}}`. Ebeveyn zinciri en çok 16 derinlik, döngü hatadır; çocuğun değeri kazanır; `type: "pbr"` ile `shader`
  birlikte verilemez (`modules/render/src/material_file.cpp:9,29-71`). Dosyadan yükleme, hot-reload (0.25 sn aralıkla dosya zamanı) ve texture
  yükleme `Astral::App::MaterialLibrary`'dedir (`modules/app/src/material_library.cpp:213-261`, `:263-266`).
  - `type: "pbr"`: `values` anahtarları `MaterialDesc`'in reflection alanları (`base_color, roughness, metallic, emissive, normal_scale,
    shading_model, clearcoat, sheen, anisotropy, transmission, subsurface, detail, wetness, mapping`) ile 7 doku slotudur
    (`base_color_texture, metallic_roughness_texture, normal_texture, emissive_texture, detail_albedo/normal/height_texture`). Bilinmeyen ad hatadır
    (`modules/app/src/material_library.cpp:180-211`, `modules/render/include/astral/render/renderer.hpp:465-466`).
  - `shader: yol`: `evaluate_uniforms` değerleri shader'ın `layout_uniforms` düzenine çevirir; tipi uymayan değer ve bilinmeyen ad uyarı olur, varsayılan
    kullanılır (`modules/render/src/material_file.cpp:73-125`).
- **Şading modelleri** (`ShadingModel`: `Standard, Clearcoat, Sheen, Anisotropic, Transmission, Subsurface`,
  `modules/shader/include/astral/shader/ashader.hpp:17`). Model G-buffer'da `emissive.a = model + 1` olarak kodlanır (`Unshaded` ayrı bir değerdir,
  `modules/render/shaders/gbuffer.frag:88-89,113`, `modules/render/shaders/include/astral/bsdf.glsl:13-19`). `Standard` iken `clearcoat.strength > 0`
  modeli kendiliğinden `Clearcoat` yapar (`modules/render/shaders/gbuffer.frag:89`, `modules/render/src/surface_shader.cpp:229`).

| Model | Parametreler | Raster yol | Hibrit RT | Path tracer |
|---|---|---|---|---|
| Standard | `base_color, roughness, metallic, emissive`, dokular | GGX + çok-saçılımlı telafi | aynı | aynı |
| Clearcoat | `clearcoat.strength/roughness` (IOR 1.5) | üst katman BRDF + ortam | stokastik lob seçimi (clearcoat/taban) | lob seçimi |
| Sheen | `sheen.color/roughness` (Charlie NDF) | doğrudan ışık + kenar parlaması | kenar parlaması × dolaylı ışık (yaklaşık) | difüz lobuna eklenen yaklaşık kenar terimi |
| Anisotropic | `anisotropy.strength` [-1,1], `rotation` | anizotropik GGX | anizotropik VNDF örnekleme | anizotropik VNDF |
| Transmission | `strength, ior, thickness, attenuation_color/distance` | yalnız gökyüzü kırılması + Fresnel (sahne kırılmaz) | ayna yansıması + kırılma ışını + çıkış arayüzü ışını (en çok 3 ışın), Beer-Lambert | tam kırılma + hacim içi Beer-Lambert takibi |
| Subsurface | `strength, color, radius` | doğrudan ışıkta saçılım yaklaşımı + arka yarıküre ortam | arka yarıküre DDGI geçirgenliği (yaklaşık) | olasılıksal içe geçiş |

Kaynaklar: `modules/render/shaders/lighting.comp:37-76,183-201` (raster), `modules/render/shaders/rt_lighting.comp:135-297` (hibrit RT),
`modules/render/shaders/path_tracer.comp:225-319` (path tracer).

- **Ek katmanlar:** `wetness` (gözenekli yüzeyi karartır, pürüzlülüğü düşürür, su clearcoat'ı ekler); `DetailDesc` (detay albedo + normal, `parallax_scale > 0`
  ise 12 adımlı POM); detay dokuları ve POM yalnız G-buffer'a (birincil görünür yüzeye) uygulanır (`modules/render/shaders/gbuffer.frag:41-75`); ıslaklık RT/PT vuruşlarında da uygulanır
  (`modules/render/shaders/include/astral/rt.glsl:237`). `Decal` kutu
  izdüşümlü boya/ıslaklık/normal uygular ve yalnız G-buffer'ı değiştirir (`modules/render/src/passes/decal_pass.cpp:13-49`).
- **Özel yüzeylerde** aynı alanlar `surface()` içinde yazılan yerleşiklerdir (`CLEARCOAT, SHEEN_COLOR, ANISOTROPY, TRANSMISSION, IOR, SUBSURFACE,
  WETNESS...`, `modules/render/include/astral/render/surface_shader.hpp:2-10`); model `shading_model` başlığından gelir.

### Özel `.ashader` entegrasyonu

Dil tanımı ve ayrıştırıcı `Astral::Shader` modülündedir ([shader.md](shader.md)); bu modül her `shader_type` için pipeline üretir ve renderer'a bağlar. Karar: ADR-0022
([../decisions/0022-shader-and-material-language.md](../decisions/0022-shader-and-material-language.md)).

| `shader_type` | Ne üretilir | Yerleşikler | Kaynak |
|---|---|---|---|
| `surface` | G-buffer vertex+fragment, gölge vertex (+`alpha_test` ise gölge fragment), RT parçası (`rt_chunk`) | `vertex()`: `VERTEX, NORMAL, UV, WORLD_POSITION, MODEL_MATRIX, TIME`; `surface()`: `UV, NORMAL, WORLD_POSITION, VIEW_DIR, SCREEN_UV, ALBEDO, ROUGHNESS, METALLIC, EMISSION, ALPHA, ALPHA_SCISSOR, NORMAL_MAP, AO` ve M13 alanları | `modules/render/src/surface_shader.cpp:359-424`, `modules/render/include/astral/render/surface_shader.hpp:2-10` |
| `surface` + `render_mode blend_mix/blend_add` | Yalnız forward vertex+fragment; G-buffer, gölge ve RT parçası yok. Şeffaf geçişte çizilir | aynı + ileri aydınlatma (`astral_lit`) | `modules/render/src/surface_shader.cpp:378-394`, `modules/render/src/surface_shader.cpp:168-190` |
| `post` | Tam ekran fragment; `RenderScene::post` zincirinde sırayla tonemap'ten sonra | `COLOR` (LDR, ekran rengi), `SCREEN_UV`, `SCREEN_TEXTURE`, `TIME` | `modules/render/src/surface_shader_rt.cpp:129-181,257-290` |
| `sky` | Arka plan fragment'i (+`sky_capture` varyantı) | `EYEDIR, SUN_DIR, SUN_COLOR, TIME`, çıktı `COLOR` (HDR) | `modules/render/src/surface_shader_rt.cpp:129-181` |
| `particle` | Fragment (`particle()`), isteğe bağlı vertex (`motion()`); varsayılan toplamalı karışım | `UV, WORLD_POSITION, COLOR, AGE, LIFETIME, LIFE_T, ALPHA` / `WORLD_POSITION, ORIGIN, WIND, AGE, LIFETIME, LIFE_T, RANDOM, SIZE` | `modules/render/src/surface_shader_rt.cpp:69-116` |
| `volume` | Duman/sis atlasını dolduran compute (`volume()`) | `UVW, WORLD_POSITION, TINT, EMISSION, DENSITY, TIME, AGE` | `modules/render/src/surface_shader_rt.cpp:22-67` |
| `ui` | Ayrıştırılır, **derlenmez**: `compile_screen_shader` hata verir, `MaterialLibrary` "not supported yet" der | - | `modules/render/src/surface_shader_rt.cpp:262`, `modules/app/src/material_library.cpp:158` |

- **Parametre bloğu:** uniform'lar `#define` ile `astral_pf(offset)` / `astral_pu(offset)` okumalarına çevrilir; `int`/`bool` kelimenin kendisinden okunur
  (float turu yok, ADR-0042), `sampler2D` bindless indeksi taşır ve ayarlanmamış doku beyaz (normal haritasında düz normal) okunur
  (`modules/render/src/surface_shader.cpp:30-57`, `modules/render/src/stores/material_store.cpp:121-127`).
  ([../decisions/0042-typed-shader-parameter-words.md](../decisions/0042-typed-shader-parameter-words.md))
- **Ray tracing tutarlılığı:** özel `surface()` her opak shader için bir RT parçasına çevrilir; `compile_rt_variants` bunları `rt_lighting`, `ddgi_trace`,
  `restir_temporal`, `restir_spatial`, `path_tracer` olmak üzere beş compute shader'a gömer, beşi eşzamanlı derlenir
  (`modules/render/src/surface_shader_rt.cpp:183-255`). `ShaderStore` bir özel shader eklendiğinde/silindiğinde "kirli" bayrağı kurar ve **sonraki RT karesinin
  başında, render iş parçacığında, eşzamanlı** olarak beş shader'ı yeniden derler; başarısızsa ön derlenmiş (özel yüzeysiz) pipeline'lar kullanılır ve neden
  `Renderer::shader_error()`'da kalır, bu durumda özel materyaller RT yansıma/GI'da taban değerleriyle görünür
  (`modules/render/src/stores/shader_store.cpp:99,155,182-212`, `modules/render/src/paths/ray_traced_path.cpp:301`).
- **RT'deki farklar:** RT varyantında türevler 0 okunur (`dFdx/dFdy`), `texture()` sabit LOD 2'den örneklenir, varying'ler 0'dır (`vertex()` çalışmaz), `NORMAL_MAP`
  ve `ALPHA` yok sayılır (`modules/render/src/surface_shader.cpp:303-309`, `modules/render/include/astral/render/surface_shader.hpp:77-80`).
  `alpha_test` RT'de uygulanır: en az bir alfa testli özel shader varsa **tüm** RT ışın sorguları "opak" bayrağını kaybeder (`ASTRAL_RT_ALPHA_TEST`)
  (`modules/render/src/surface_shader_rt.cpp:215-216,238`, `modules/render/shaders/include/astral/rt.glsl:299-312`).
- **Çizim sırası ve gölge:** opak özel yüzeyler shader'a göre gruplanır (raster ve RT G-buffer aynı mantık); `blend_*` yüzeyler TLAS'ta olmadığı için RT'de
  görünmez, gölge düşürmez ve almaz (`modules/render/src/frame/instances.cpp:35-39`, `modules/render/src/passes/transparent_pass.cpp:5-36`).
- **Kabul edilip kullanılmayan `render_mode`'lar:** `rt_simple` (surface), `after_tonemap` (post), particle/volume için `unshaded`, `ui` modları ayrıştırıcıda
  geçerlidir ama renderer'da hiçbir yerde okunmaz (`modules/shader/src/ashader.cpp:372-382`; render tarafında `has_render_mode` yalnız `unshaded`(surface),
  `alpha_test`, `cull_*`, `shadows_disabled`, `blend_*` için çağrılır: `modules/render/src/surface_shader.cpp:269-380`, `modules/render/src/surface_shader_rt.cpp:79`).

### Aydınlatma

| Bileşen | Nasıl | Kaynak |
|---|---|---|
| Güneş | `DirectionalLight`. Raster: tek ortografik gölge haritası + 3×3 PCF (`shadows=false` yalnız bu haritayı kapatır; RT/PT güneşi bayrağı yok sayıp her zaman gölge ışını atar: `modules/render/src/passes/shadow_pass.cpp:90`). RT: piksel başına açısal yarıçaplı konide 1 gölge ışını (`angular_radius` varsayılan 0.0047 rad); yumuşak gölge denoiser'a bırakılır | `modules/render/include/astral/render/renderer.hpp:197-203`, `modules/render/shaders/lighting.comp:17-35`, `modules/render/shaders/include/astral/rt.glsl:563-577` |
| Nokta ışık | `PointLight`; yarıçapta `(1-(d/r)^4)^2/(d²+1)` sönümü. Raster'da gölgesiz; RT'de `source_radius` ile yumuşak gölge. Doğrudan döngü eşiği: birincil vuruşta ≤4, ikincilde ≤2 ışık; üstü RIS (veya ReSTIR) | `modules/render/include/astral/render/renderer.hpp:170-176`, `modules/render/shaders/include/astral/rt.glsl:500-551,585-607` |
| Dikdörtgen alan ışığı | `RectLight` (yalnız RT yolu ve path tracer); iki yüzlü seçeneği; kameraya ve yansımalara görünür, fiziksel yumuşak gölge. Doğrudan döngü eşiği: birincil ≤2, ikincil ≤1; üstü RIS (veya ReSTIR) | `modules/render/include/astral/render/renderer.hpp:178-188`, `modules/render/shaders/include/astral/rt.glsl:612-631`, `modules/render/shaders/composite.comp:54-61` |
| DDGI | Tek (cascade'siz), düzgün probe ızgarası; varsayılan 8×8×8 probe, 128 ışın/probe (16..512'ye kırpılır), histerezis 0.97 (büyük ışık değişiminde adaptif düşer). Hacim `gi.volume_min == volume_max` ise her karede mesh ve SDF sınırlarına otomatik sığdırılır; ızgara konumu/aralığı hacim çaprazının %1'inden fazla kayarsa geçmiş sıfırlanır. İki atlas çifti (RGBA16F irradiance 8×8+kenar, RG16F mesafe momentleri 16×16+kenar) | `modules/render/include/astral/render/renderer.hpp:268-274`, `modules/render/src/passes/ddgi_pass.cpp:72-124`, `modules/render/shaders/ddgi_update.comp:25-28` |
| Yakın alan GI | Piksel başına 1 kosinüs ışını, `near_gi_radius` (0.75 m) içinde vurursa o yüzeyin gerçek ışığı; yoksa DDGI | `modules/render/shaders/rt_lighting.comp:114-133` |
| ReSTIR DI | Yerel ışık sayısı (nokta+rect) > 2 ise, bkz. pass tablosu. Rezervuar tamponları 3 × 16 B × piksel sayısı | `modules/render/src/paths/ray_traced_path.cpp:171`, `modules/render/src/passes/rt_lighting_pass.cpp:13-35` |
| RT yansıma | Piksel başına tek ışın: GGX VNDF (clearcoat, anizotropi, rect MIS dalları). `roughness < 0.04` ayna sayılır. Pürüzlülük kesimi aşağıda | `modules/render/shaders/rt_lighting.comp:144-297`, `modules/render/shaders/include/astral/brdf.glsl:13` |
| İkincil vuruş ışığı | `shade_secondary`: doğrudan ışık + DDGI dolaylı + DDGI'dan bulanık "ayna" ortam terimi + emisyon | `modules/render/shaders/include/astral/rt.glsl:640-654` |
| Denoiser | SVGF benzeri: zamansal birikim (`max_history` 32) + à-trous | pass tablosu |
| SDF | Mesh'lerle derinlik-birleşir; RT'de TLAS'ta değildir, her RT ışını (gölge dahil) SDF listesini ayrıca raymarch eder (trace ≤128, occluded ≤96 adım, her adım tüm şekilleri tarar) | `modules/render/shaders/include/astral/rt.glsl:299-350`, `modules/render/shaders/include/astral/sdf.glsl:46` |
| Gökyüzü | Varsayılan: `Environment::sky/ground` gradyanı (RT ıska ışınlarında da). `sky_ibl` açıkken etkin `sky` materyali 64×32 equirect'e çekilir ve 6 seviyeye filtrelenir; lighting, DDGI ve RT ıska ışınları bunu kullanır | `modules/render/shaders/include/astral/brdf.glsl:138-152`, `modules/render/src/passes/sky_pass.cpp:15-52` |
| Sis, duman, aerial | Pass tablosundaki froxel hattı; duman: ≤16 hacim, kenar ≤25 hücre, 8³ tuğlalar, ≤512 yerleşik tuğla (taşan hacim `volume_brick_fallbacks`'a sayılır); `aerial_extinction/aerial_start_m` | `modules/render/src/passes/fog_pass.cpp:16-17,79-103,227-229`, `modules/render/include/astral/render/renderer.hpp:205-225` |
| Partikül | ≤8192 toplam, ≤1024/patlama, ≤64 kalıcı simülasyon durumu; ekran uzayı derinlik çarpışması; `lighting` > 0 ise DDGI ile modüle | `modules/render/src/passes/particle_pass.cpp:34,39,44,65-106` |

### Doku projeksiyonu

`TextureMapping::projection` (`MeshUV`, `PlanarXY`, `PlanarXZ`, `PlanarYZ`, `Triplanar`), `tile_size_m` (varsayılan 1 m), `uv_scale/offset/rotation`,
`blend_sharpness` (4; shader'a 1..32 kırpılır), `world_space` (true). Dünya uzayı projeksiyonunda kayan orijin kayması `RenderScene::world_origin`'den
`fmod(origin, tile)/tile` fazı olarak eklenir; böylece metre cinsinden desen orijin kaymasında yerinde kalır (`modules/render/src/stores/material_store.cpp:91-99`,
test: `modules/render/tests/renderer_test.cpp:590`). Tek `projection.glsl` raster G-buffer (`gbuffer.frag`) ve RT/path tracer (`rt.glsl`) tarafından
ortak kullanılır (`modules/render/shaders/include/astral/projection.glsl:7-77`, `modules/render/shaders/gbuffer.frag:20-21`,
`modules/render/shaders/include/astral/rt.glsl:11-20`). Yönelim kuralı: dikey yüzlerde dünya yukarısı (+Y) doku V'sidir, U ekranda sağa akar (ayna yok);
Y yüzlerinde U=Z, V=X (`modules/render/shaders/include/astral/projection.glsl:40-58`). `PlanarXY/XZ/YZ` sırasıyla Z, Y, X eksenli yüze eşlenir
(`modules/render/shaders/include/astral/projection.glsl:30-32`). Yönelim düzeltmesi 31e2c48 ile yapıldı (görev tanımı konuyu ADR-0051 ile anıyor; bu ağaçta o dosya yok, bkz. "Kararlar").

RT/path tracer ikincil vuruşlarında yalnız projeksiyonlu normal haritası uygulanır; `MeshUV` normal haritası, detay dokuları ve POM yalnız birincil
G-buffer'dadır (bkz. zayıf yanlar) (`modules/render/shaders/include/astral/rt.glsl:232-234`).

### Store'lar

| Store | Sorumluluk | Not | Kaynak |
|---|---|---|---|
| `MeshStore` | Vertex (storage) ve indeks tamponları, yerel AABB, RT açıkken BLAS | İndeks ≥ vertex sayısı ya da boş veri reddedilir; `update_mesh` yeni tamponu kurup eskisini bırakır; mesh'ler statiktir (BLAS güncelleme/yeniden sığdırma yok) | `modules/render/src/stores/mesh_store.cpp:16-62,79-91` |
| `TextureStore` | RGBA8 (sRGB/UNORM) ya da BC7/BC5 hazır zincir; 1×1 beyaz ve düz normal stand-in'ler; render target'lar | Mip: `build_mips` (CPU). BC kodlama yok (yalnız cook edilmiş veri alınır). `update_texture` yeni texture'ı kurup eskisini erteler | `modules/render/src/stores/texture_store.cpp:126-195` |
| `MaterialStore` | `HandlePool<MaterialDesc>`, varsayılan materyal (silinemez), GPU tablosu + parametre bloğu paketleme | Her karede tüm tablo yeniden kurulur | `modules/render/src/stores/material_store.cpp:11-134` |
| `ShaderStore` | Yüzey/post/sky/particle/volume pipeline havuzu; özel RT pipeline'ları | `find_by_key` kayıtlı tüm shader'lar üzerinde doğrusal tarama yapar (G-buffer ve gölge çizimlerinde shader anahtarı değiştikçe çağrılır) | `modules/render/src/stores/shader_store.cpp:49-212`, `modules/render/src/stores/shader_store.cpp:159-166` |

### Geçmiş / zamansal durum

- `TemporalState`: kare indeksi, `history_valid`, önceki view-projection, önceki kamera konumu, önceki ışık sayıları, `prev_restir_active`, path tracer imzaları,
  SDF imzası, sky anahtarı, önceki sahne zamanı (`modules/render/src/paths/ray_traced_path.hpp:21-35`). Ek olarak her pass kendi geçmiş texture'larını tutar:
  DDGI atlasları (2'li), denoiser diffuse/specular/moments (2'li), geometri geçmişi, ReSTIR rezervuarları (2 + 1 geçici), TAA (2'li), path tracer birikimi (2'li RGBA32F).
  Çift tamponlar `frame_index & 1` ile dönüşümlüdür (`modules/render/src/passes/rt_lighting_pass.cpp:78-79`).
- **Geçmiş yalnız GPU gönderimi başarılı olunca ilerler:** kare kaydında yeni `TemporalState` hesaplanır, `cmd.on_submitted` ile uygulanır; düşen/başarısız kare
  önceki geçmişi, DDGI geçerliliğini ve path tracer örnek sayısını olduğu gibi bırakır (`modules/render/src/paths/ray_traced_path.cpp:348-365`).
- **Sıfırlayanlar:** kamera kesmesi için `reset_history()`; RT'nin açılıp kapanması; `gi.probes_*`, `gi.rays_per_probe`, `denoise_iterations`, `restir_di`, `path_tracer`,
  `path_tracer_max_bounces`, `sky_ibl` değişimi (`modules/render/src/renderer.cpp:188-201`); hedef boyut değişimi (`modules/render/src/paths/ray_traced_path.cpp:127-139`);
  SDF şekil verisi değişimi hibrit RT'de zamansal geçmişi geçersiz kılar, çünkü SDF hızı yalnız kamera hareketinden türetilir
  (`modules/render/src/paths/ray_traced_path.cpp:118-125`, test: `modules/render/tests/renderer_test.cpp:333`); ReSTIR geçmişi yalnız ışık sayıları aynıysa geçerlidir
  (`modules/render/src/paths/ray_traced_path.cpp:171-176`). `debug_view`, `max_history`, `taa_blend`, `near_gi_radius` ve yansıma kesimi değişimleri geçmişi sıfırlamaz.
- Raster yolda zamansal durum yoktur (`frame_index = 0`, `prev_view_projection` birim matris).

### Path tracer sıfırlama telemetrisi ve donma

`RenderStats::path_tracer_reset` (`PathTracerReset`), o karede birikimin neden yeniden başladığını söyler (`None` = başlamadı). Sebepler: `Resize, Geometry, Lights, Environment,
Time, Camera, Resources, Settings, Manual, History` (`modules/render/include/astral/render/renderer.hpp:322-334`).

- Üç ayrı 64-bit FNV-1a imzası tutulur: `geometry` (örnek sayıları, her örneğin model matrisi, materyal indeksi, vertex/indeks adresi; SDF şekilleri; decal'lar), `lights` (nokta, rect, güneş
  yönü/rengi/yoğunluğu/açısal yarıçapı), `environment` (gökyüzü, zemin, ortam değeri) ve ayrıca sky anahtarı. **Nicemlenmez**: referans modda her fark farklı bir sahnedir
  (`modules/render/src/paths/ray_traced_path.cpp:40-44,54-89`). `exposure`, sis ve aerial imzaya girmez (tonemap ve atmosfer birikimden sonradır).
- Sıra: önce geçmiş yoksa `History`, sonra sahne nedenleri (`Geometry` → `Lights` → `Environment` → `Time`), en son kamera; böylece beklenmedik bir sürüklenme kamera
  hareketinin arkasında saklanmaz (`modules/render/src/paths/ray_traced_path.cpp:91-116`). `Time`, yalnız `TIME` kullanan bir özel yüzey varsa ve sahne zamanı değişmişse
  tetiklenir (`modules/render/src/paths/ray_traced_path.cpp:46-52`). Bir karede birden çok sıfırlama olursa ilk neden korunur
  (`modules/render/src/passes/path_tracer_pass.hpp:24-30`). Kaynak güncellemeleri (`update_mesh/texture/material`, `destroy` texture/shader/materyal) `Resources` (mesh silme ayrıca sıfırlamaz; örnek listesi değişince `Geometry` imzası yakalar), `set_settings` `Settings`,
  `reset_history()` `Manual`, boyut `Resize` olarak sıfırlar (`modules/render/src/renderer.cpp:108-172,183-215`).
- **`path_tracer_freeze_scene`** (varsayılan false): `Geometry..Time` nedenlerini bastırır; birikim sahne değişimleri boyunca devam eder ve sonuç bu durumların ortalamasıdır.
  Kamera, boyut, kaynak düzenlemesi ve ayar değişimleri yine sıfırlar. Bastırma `reason >= Geometry && reason <= Time` karşılaştırmasıyla yapılır; yani enum sırasına
  bağlıdır (`modules/render/src/paths/ray_traced_path.cpp:103-105`, `modules/render/include/astral/render/renderer.hpp:322-334`).
  Testler: `modules/render/tests/m13_pbr_test.cpp:583,634,661`. Örnek sayısı gönderimde ilerler; eski nesil bir örnek sayılmaz (`modules/render/src/passes/path_tracer_pass.hpp:36-41`).
- Rapor yalnız path tracer açıkken doludur; dışında `None` (`modules/render/src/paths/ray_traced_path.cpp:374-375`).

### Yansıma pürüzlülük kesimi

`max_reflection_roughness` (varsayılan 1) ve `reflection_roughness_fade` (0.15) pürüzlü yüzeylerin yansıma ışınını atlamasını sağlar: kesimden daha pürüzlü piksel, yansımasını DDGI
probelarından (oklüzyonu bilen) "baskın yön" (ayna yönü ile normal arası) için alır; geçiş aralığında seçim stokastiktir ve denoiser çözer
(`modules/render/shaders/rt_lighting.comp:224-237`). Varsayılan 1'de blok hiç çalışmaz, rastgele sayı bile tüketilmez, yani görüntü eskisiyle aynıdır. Kesimin dışında kalanlar:
clearcoat lobu seçilen pikseller, anizotropik yüzeyler ve şeffaflık dalı (`modules/render/shaders/rt_lighting.comp:154,229`). Değerler `normalized()` ile
`[0,1]` ve `≥0` aralığına çekilir (`modules/render/src/renderer.cpp:73-78`). Path tracer bu ayardan etkilenmez.

### Shader derleme servisleri, busy callback ve `build_mips`

- **Servisler:** `ShaderCompileServices{cache, jobs}` iki ödünç işaretçidir (global örnek yok) (`modules/shader/include/astral/shader/compile.hpp:33-36`).
  `Astral::App` bir `SpirvCache` (içeriği SHA-256 ile adreslenen disk önbelleği, bkz. `modules/shader/include/astral/shader/spirv_cache.hpp:1-9`) ve kendi `JobSystem`'ini kurup
  `Renderer::set_shader_compile_services` ile verir; önbellek klasörü `ASTRAL_SHADER_CACHE` ortam değişkeniyle (`off`/`0` kapatır, klasör yolu verir) seçilir ve açılışta 512 MiB
  bütçeye budanır (`modules/app/src/app.cpp:211-219`, `modules/app/src/shader_cache_location.hpp:18-19`). Servis yoksa derleme seri ve önbelleksizdir. Motorun kendi shader'ları
  derleme zamanında SPIR-V'ye çevrilip gömülür (`modules/render/CMakeLists.txt:89-104`); çalışma anında yalnız `.ashader`'lar ve özel yüzeyli RT varyantları derlenir.
  Surface shader aşamaları `compile_glsl_batch` ile eşzamanlı derlenir; sonuç seri/önbelleksiz derlemeyle aynıdır (test: `modules/render/tests/surface_shader_test.cpp`,
  "compiled with jobs and a cache equals the serial uncached build").
- **Busy callback:** renderer, oluşturduğu/güncellediği her texture, mesh, materyal (yalnız `create_material`) ve shader'dan sonra çağırana haber verir; App bunu en çok 50 ms'de bir
  `Window::keep_responsive()` (`SDL_PumpEvents`) çağırmak için kullanır, böylece saniyeler süren yükleme pencereyi "yanıt vermiyor" yapmaz
  (`modules/render/src/renderer.cpp:98-107,120-124,146-170`, `modules/app/src/app.cpp:223-233`, `modules/platform/src/window.cpp:138`). `update_material` bildirim yapmaz
  (`modules/render/src/renderer.cpp:164-168`) ve başlık yorumu bunu "update" diye de sayar (`modules/render/include/astral/render/renderer.hpp:398-399`).
- **`build_mips`:** 2:1 tam indirgeme (iki kenar da çift) için 2×2 ortalama, diğer boyutlarda kapsanan kaynak alanı örtüşme ağırlıklı ortalama (tek boyutlarda son satır/sütun kaybolmaz);
  renk görüntüleri sRGB→doğrusal tablo ile doğrusal uzayda ortalanır ve yeniden kodlanır. 128×128 texel ve üstü seviyeler iş sistemiyle satır bazında paralel hesaplanır;
  sonuç seri hesapla bayt bayt aynıdır (`modules/render/src/stores/texture_store.cpp:44-124`,
  testler: `modules/render/tests/mips_reference_test.cpp`, `modules/render/tests/stores_test.cpp`).
- Not: görev tanımı bu üç konuyu ADR-0047/0048 ile ilişkilendiriyor; bu ağaçta bu numaralı dosyalar yoktur (bkz. "Kararlar").

### Varsayılan `RenderSettings`

| Alan | Varsayılan | Not |
|---|---|---|
| `shadow_map_size` | 2048 | Yalnız raster yol ve RT yolunda sis/duman; `> 0` değişince gölge haritası yeniden yaratılır (`modules/render/include/astral/render/renderer.hpp:281`, `modules/render/src/renderer.cpp:202-204`) |
| `ray_tracing` | true | Cihaz desteklemezse raster (`modules/render/include/astral/render/renderer.hpp:284`) |
| `gi` | 8×8×8 probe, 128 ışın, histerezis 0.97, hacim otomatik | Probe sayısının üst sınırı yok; ışın 16..512 (`modules/render/include/astral/render/renderer.hpp:268-274`) |
| `denoise_iterations` | 3 | En çok 5 (`max_denoise_iterations`), fazlası `normalized()` ile kırpılır (`modules/render/include/astral/render/renderer.hpp:287-288`) |
| `max_history` | 32 | Zamansal birikim uzunluğu, en az 1 (`modules/render/include/astral/render/renderer.hpp:289`) |
| `taa_blend` | 0.1 | Kullanımda [0.01, 1]'e kırpılır (`modules/render/include/astral/render/renderer.hpp:290`, `modules/render/src/passes/taa_pass.cpp:57`) |
| `bloom_strength` | 0.04 | ≤0: bloom pass'i atlanır (`modules/render/include/astral/render/renderer.hpp:291`) |
| `max_ray_distance` | 1000 | RT ışın tmax (`modules/render/include/astral/render/renderer.hpp:292`) |
| `near_gi_radius` | 0.75 | 0: yalnız probe (`modules/render/include/astral/render/renderer.hpp:295`) |
| `max_reflection_roughness` | 1.0 | 1 = her şeyi izle; [0,1]'e kırpılır (`modules/render/include/astral/render/renderer.hpp:300`) |
| `reflection_roughness_fade` | 0.15 | ≥0 (`modules/render/include/astral/render/renderer.hpp:303`) |
| `restir_di` | true | Yerel ışık sayısı > 2 iken geçerli; eşik sabit kodlu (`modules/render/include/astral/render/renderer.hpp:305`) |
| `path_tracer` | false | RT donanımı gerekir (`modules/render/include/astral/render/renderer.hpp:307`) |
| `path_tracer_max_bounces` | 6 | Kullanımda [1, 16]'ya kırpılır (`modules/render/include/astral/render/renderer.hpp:308`, `modules/render/src/paths/ray_traced_path.cpp:201`) |
| `path_tracer_freeze_scene` | false | (`modules/render/include/astral/render/renderer.hpp:313`) |
| `debug_view` | `None` | `Albedo, Normals, DiffuseLight, Specular, Motion`; açıkken sis, aerial ve partikül kapanır (`modules/render/include/astral/render/renderer.hpp:314`, `modules/render/src/paths/ray_traced_path.cpp:161,221`) |
| `fog_self_shadows` | true | Yerel ortam güneşi kendi hacmi içinde azaltır (`modules/render/include/astral/render/renderer.hpp:315`) |
| `fog_ddgi` | true | Hibrit yolda sise DDGI ortam ışığı (`modules/render/include/astral/render/renderer.hpp:316`) |
| `sky_ibl` | false | Etkin `sky` materyalini HDR ortam ışığı yapar (`modules/render/include/astral/render/renderer.hpp:317`) |

Sahne tarafı varsayılanları: güneş `intensity 3`, `direction (-0.4,-1,-0.3)`, `shadows true`; `Environment` `sky (0.45,0.6,0.85)`, `ground (0.18,0.16,0.14)`, `ambient 0.35`, `exposure 1`,
aerial ve sis kapalı (`modules/render/include/astral/render/renderer.hpp:197-214`). `MaterialDesc` varsayılanı: beyaz, `roughness 0.6`, `metallic 0`
(`modules/render/include/astral/render/renderer.hpp:98-101`).

### Debug çizim, 2D katman, metin

`DebugDraw` düz veri toplayıcıdır (GPU gerektirmez); çizgiler tonemap'ten sonra derinlik testli ya da `on_top` çizilir. `UiLayer` piksel uzayında indeksli üçgenler + komut başına
doku, kırpma ve SDF yumuşaklığıdır; ImGui, HUD ve sprite aynı biçimi üretir. `Font` stb_truetype ile SDF atlası kurar (varsayılan Roboto Medium gömülü; ASCII, Latin-1, Latin Extended-A
yani Türkçe harfler, ₺) (`modules/render/include/astral/render/text.hpp:28-50`, `modules/render/src/passes/debug_lines_pass.cpp:30-70`, `modules/render/src/passes/ui_pass.cpp:23-50`).

## Güçlü yanlar

- **Temiz sınır:** ECS'i bilmeyen, düz veriyle çalışan tek `Renderer` sınıfı; aynı sözleşme `App` ve doğrudan kullanan araçlar için geçerli
  (`modules/render/include/astral/render/renderer.hpp:358-360`). Kaynaklar handle'dır, geçersiz handle çökertmez (mesh atlanır, materyal varsayılana düşer).
- **Üç yol, tek BSDF/G-buffer kütüphanesi:** raster, hibrit RT ve path tracer aynı M13 BSDF'lerini, aynı `projection.glsl`'i ve aynı özel `surface()` parçalarını kullanır; "raster, RT ve PT"
  çapraz testleri vardır (`modules/render/tests/m13_pbr_test.cpp:188-305,461`).
- **Tam hibrit zincir tek modülde:** RT gölge, DDGI, ReSTIR DI, yakın alan GI, RT yansıma, SVGF benzeri denoiser, TAA, bloom, froxel sis, GPU partikül ve GPU duman atlası; RT donanımı yoksa
  raster'a düşer (`modules/render/src/renderer.cpp:85`).
- **Kaçırılan kare dayanıklılığı:** zamansal geçmiş yalnız GPU gönderimi başarılı olunca ilerler; başarısız boyutlandırma/ayırma "boyut 0" ile işaretlenip sonraki karede yeniden denenir
  (`modules/render/src/paths/ray_traced_path.cpp:348-365`, `modules/render/src/passes/rt_lighting_pass.cpp:29-34`).
- **Gözlemlenebilirlik:** pass başına GPU süresi, `RenderStats` ve path tracer sıfırlama nedeni (test edilmiş) kamerayı oynatmadan teşhisi mümkün kılar
  (`modules/render/include/astral/render/renderer.hpp:336-352`, `modules/render/tests/m13_pbr_test.cpp:583-690`).
- **Özel shader tek kaynak:** bir `.ashader` hem G-buffer/gölge/forward hem RT parçası olarak derlenir; hatalar `.ashader` satırına eşlenir; RT parçası derlenemezse sahne bozulmaz, neden
  `shader_error()`'da kalır (test: `modules/render/tests/renderer_test.cpp:946`).
- **Hızlı başlangıç altyapısı:** SPIR-V önbelleği, eşzamanlı derleme, paralel `build_mips` ve busy callback; ölçülen etki "Performans" bölümündedir.
- **Girdi doğrulaması:** mesh indeksi, materyal parametre ofseti, denoiser iterasyonu, `ParamTexture` ofseti sınırlıdır ve regresyon testleri vardır (`modules/render/tests/msr_regression_test.cpp`).
- **Test genişliği:** 142 `TEST_CASE`; matematik sözleşmeleri (white-furnace, VNDF, oktahedral kodlama, offset_ray, sRGB, bloom DC kazancı) ve piksel okuyan uçtan uca testler.

## Zayıf yanlar ve bilinen sınırlar

### A. Doğrulanmış performans boşlukları

| # | Boşluk | Ayrıntı ve kaynak |
|---|---|---|
| A1 | **Render scale / yarı çözünürlük RT yok.** | Tüm RT hedefleri (geometri geçmişi, rezervuarlar, denoiser, TAA, path tracer birikimi) hedef texture boyutunda yaratılır; ölçek ayarı `RenderSettings`'te yok, upscaler (DLSS/FSR/TAAU) yok (`modules/render/src/paths/ray_traced_path.cpp:127-139`, `modules/render/src/passes/rt_lighting_pass.cpp:13-35`, `modules/render/include/astral/render/renderer.hpp:280-318`). Maliyet ayarı yalnız ışın/pürüzlülük/iterasyon düzeyindedir. |
| A2 | **À-trous tam çözünürlük, paylaşımlı bellek yok.** | Her iterasyon her piksel için 5×5 = 25 dokunuş yapar (her dokunuşta geometri + diffuse + speküler fetch ve `camera_ray` yeniden kurma); `shared` bellek kullanılmaz, kernel adımı `1<<i` olduğundan dokunuşlar iterasyonla uzaklaşır (önbellek etkisi ölçülmedi, çıkarımdır) (`modules/render/shaders/denoise_atrous.comp:18,53-80`, adım: `modules/render/src/passes/denoise_pass.cpp:131`). Harici ölçüm: 2 geçiş 2.6 ms, 3 geçiş 4.4 ms (bkz. Performans). |
| A3 | **ReSTIR maliyeti ve sabit kodlu ">2 ışık" eşiği.** | Eşik üç yerde ayrı ayrı sabit yazılıdır (`modules/render/src/paths/ray_traced_path.cpp:171,356`, `modules/render/src/passes/rt_lighting_pass.cpp:86-87`); aday sayısı (24), zamansal/uzamsal M sınırları (20/60), komşu sayısı (3) ve yarıçap (24 px) shader sabitidir, ayardan değişmez (`modules/render/shaders/include/astral/restir.glsl:11-13`, `modules/render/shaders/restir_di_spatial.comp:15-16`). Rezervuar tamponları 3 × 16 B × piksel (1080p'de ≈100 MB, hesap). Harici ölçüm: rt_ultra'da temporal+spatial ≈ 10.9 ms (yeniden ölçülmedi). Eşiğin hemen üzerindeki 3 ışıkta bu hattın RIS'e göre kazandırıp kazandırmadığı ölçülmedi. |
| A4 | **İkincil ışın başına O(N) RectLight döngüsü.** | `trace_env_or_surface` her yansıma/kırılma ışınında tüm rect ışıkları analitik olarak keser (`modules/render/shaders/rt_lighting.comp:35-41`); path tracer her sekmede aynısını yapar (`modules/render/shaders/path_tracer.comp:181-186`); RIS ağırlıkları ve MIS seçim olasılığı da O(N)'dir (`modules/render/shaders/include/astral/rt.glsl:462-472,618-631`), kamera ışını için `composite` da (`modules/render/shaders/composite.comp:54-61`). Rect ışık sayısı sınırlanmamış, hızlandırma yapısı yok. |
| A5 | **Transmission dallanması (en çok 3 ışın).** | Şeffaflık modelli piksel yansıma ışını + içe kırılma ışını + çıkış arayüzü ışını izler (strength ≥ 0.95 ise yakın alan GI ışını atlanır) (`modules/render/shaders/rt_lighting.comp:118,154-207`); aynı warp'ta farklı yollar ıraksar. Ölçülmedi. |
| A6 | **Sis/duman açıkken raster gölge haritası.** | RT yolunda bile sis veya duman varsa raster `ShadowPass` çalışır ve froxel güneş görünürlüğü bu 1 örnekli sert haritadan okunur; RT gölgeleriyle tutarsızdır ve ek geçiş/çizim maliyeti getirir (`modules/render/src/paths/ray_traced_path.cpp:220-227,246-249`, `modules/render/shaders/fog_integrate.comp:13-21`). |
| A7 | **SpotLight yok.** | Işık tipleri `PointLight`, `RectLight`, `DirectionalLight`'tır (`modules/render/include/astral/render/renderer.hpp:170-203`); render ve shader kodunda spot koni terimi bulunmaz (grep). |
| A8 | **DoF yok; post shader'lar tonemap sonrası ve derinliksiz.** | `post()` yalnız LDR `SCREEN_TEXTURE`, `SCREEN_UV`, `TIME` görür; derinlik, normal, hız ya da HDR erişimi yok (`modules/render/src/passes/post_pass.cpp:13-42`, `modules/render/src/surface_shader_rt.cpp:168-174`). DoF, SSAO, hareket bulanıklığı, HDR öncesi efekt yazılamaz; `render_mode after_tonemap` parse edilir ama okunmaz. |
| A9 | **`App::read_frame` yalnız offscreen.** | Pencereli modda swapchain okunmaz; `offscreen_readback_` yoksa false döner (`modules/app/src/services/render_bridge.cpp:235-239`, `modules/app/include/astral/app/app.hpp:286-287`). Render modülünün kendisi readback sunmaz. |

### B. Doğruluk ve özellik sınırları

- **Path tracer "referans" olarak eksik ve yanlı:** radyans 64'e kırpılır (`modules/render/shaders/path_tracer.comp:371`; hibrit RT çıktısı da diffuse 64 / speküler 256'ya kırpılır: `modules/render/shaders/rt_lighting.comp:300-301`), yani
  yorumlardaki "unbiased" (`modules/render/include/astral/render/renderer.hpp:306`) kırpma nedeniyle yanlıdır. `MeshUV` normal haritası, detay albedo/normal ve POM ikincil/PT vuruşlarında uygulanmaz
  (`modules/render/shaders/include/astral/rt.glsl:232-234`); decal'lar yalnız ilk vuruşta (`modules/render/shaders/path_tracer.comp:211`). Nokta ışık sayısı > 4 ise tek bir ışık düzgün örneklenir, rect ışık
  seçimi 1/N'dir (`modules/render/shaders/path_tracer.comp:68-75,99-103`): çok ışıkta gürültü artar. Testler yalnız PT ile hibrit RT'nin ortalama parlaklığını karşılaştırır
  (`modules/render/tests/m13_pbr_test.cpp:405`).
- **Şeffaf yüzeyler (`blend_mix/blend_add`) ışın izleme dünyasında yoktur:** TLAS'a girmez, RT yansıma/GI/gölge ve path tracer'da görünmez, gölge düşürmez/almaz; TAA geçmişine girmez; sıralama nesne
  orijinine göredir, kırılma (ekranı bükme) yok (`modules/render/src/frame/instances.cpp:35-39,58-63`, `modules/render/src/passes/taa_pass.cpp:51-54`). Cam için `Transmission` modeli kullanılır (opak G-buffer + RT kırılma).
- **Partiküller tonemap sonrası LDR hedefe çizilir:** bloom, pozlama ve TAA'dan etkilenmez; toplamalı karışım LDR'de kırpılır; özel `particle()` shader'ı LDR renk üretir
  (`modules/render/src/paths/ray_traced_path.cpp:258-259`, `modules/render/src/paths/raster_path.cpp:116-117`, `modules/render/src/passes/particle_pass.cpp:12-16`).
- **Raster yol sade:** AA (MSAA/FXAA/TAA) yok, bloom yok, rect ışık yok, nokta ışık gölgesi yok, ortam yalnız gradyan ya da `sky_ibl` (`modules/render/src/paths/raster_path.cpp:96-120`,
  `modules/render/shaders/lighting.comp:113-205`).
- **Tek gölge haritası, kademe yok:** tüm mesh sınırlarına sıkı oturan tek ortografik harita; büyük sahnede çözünürlük düşer (`modules/render/src/passes/shadow_pass.cpp:15-50`).
- **SDF:** her piksel ve her RT ışını tüm şekil listesini tarar (O(N)); RT'de trace ≤128, occluded ≤96 raymarch adımı ve her adım tüm şekilleri değerlendirir; hız vektörü yalnız kameradan türer, SDF verisi değişince
  hibrit RT'nin tüm zamansal geçmişi geçersiz olur (hareketli SDF ile her karede denoiser/TAA yeniden başlar) (`modules/render/shaders/include/astral/rt.glsl:299-350`,
  `modules/render/src/paths/ray_traced_path.cpp:118-125`). Kamera bir SDF şeklinin içindeyse o şekil şeffaf görünür (test: `modules/render/tests/renderer_test.cpp:268`, "a camera inside an SDF shape").
- **DDGI:** probe sayısı için üst sınır yok (yalnız alt sınır 2; `rays_per_probe` 16..512); ışın tamponu `nx*ny*nz*rays*16` bayttır (ör. 64³ probe × 512 ışın = 2 GiB, hesap) (`modules/render/src/passes/ddgi_pass.cpp:73-76,116-117`).
  Hacim otomatik sığdırıldığında (`volume_min == volume_max`) sınır kutusu %1'den fazla değişirse geçmiş sıfırlanır; sınır kutusunu değiştiren sahnelerde (en dıştaki nesne hareket eder, nesne eklenir/çıkar) GI yeniden başlar
  (`modules/render/src/passes/ddgi_pass.cpp:78-96`; çalışma anı etkisi ölçülmedi). Tek düzgün ızgara (cascade yok), prob taşıma (relocation) yok, büyük hacimde aralık `extent/n` olarak seyrelir.
- **Mesh'ler statik:** deforme/skinned mesh için BLAS güncelleme yok; `update_mesh` tamponları ve BLAS'ı baştan kurar (`modules/render/src/stores/mesh_store.cpp:58-68`). LOD, GPU güdümlü culling, occlusion culling,
  meshlet, örnek başına materyal override (glTF'ye özgü `ModelMaterialOverride` dışında) yok. CPU görüş hacmi elemesi yalnız G-buffer'da, grup granülünde (gruptaki tek görünür örnek tüm grubu çizdirir) ve yalnız
  standart-materyalli gruplarda; gölge ve TLAS girdilerine uygulanmaz (`modules/render/src/passes/gbuffer_pass.cpp:10-41`).
- **Doku:** yalnız CPU kutu filtreli mip; BC sıkıştırma yok (cook edilmiş veri alınır), texture streaming yok, yükleme bloklayıcı ve mip başına ayrı `write_texture`
  (`modules/render/src/stores/texture_store.cpp:97-124,139-165`). `max_ray_distance`, `shadow_map_size`, `gi.probes_*` doğrulanmaz; `normalized()` yalnız üç alanı kırpar (`modules/render/src/renderer.cpp:73-78`).
- **Tonemap/kamera:** yalnız ACES ve sabit pozlama; otomatik pozlama, renk derecelendirme/LUT, vignette, film grain yok (`modules/render/shaders/tonemap.frag:8-20`). Froxel fazı sabit g=0.45 ve zamansal filtre yok
  (`modules/render/shaders/fog_integrate.comp:23-40`). Froxel ızgarası 240×135 hücreyle kısıtlıdır: 1080p üstünde hücre ekran pikselini kapsar (`modules/render/src/passes/fog_pass.cpp:227-229`).
- **Hata ve ayar hijyeni:** TLAS kurulamazsa RT karesi sessizce atlanır (`modules/render/src/paths/ray_traced_path.cpp:309`; hata raporu doğrulanmadı); `debug_view` değişimi zamansal geçmişi sıfırlamaz
  (`modules/render/src/renderer.cpp:195-201`); `update_material` busy bildirimi vermez (başlık yorumu "updates" der) (`modules/render/src/renderer.cpp:164-168`,
  `modules/render/include/astral/render/renderer.hpp:398-399`).

### C. Tasarım borcu ve bütçe baskısı

- **RenderGraph:** geçici texture aliasing'i, async compute, pass yeniden sıralama ve BDA tamponları için otomatik bağımlılık izleme yok; elle `memory_barrier()` ve `side_effect()` gerekir
  (`modules/render/src/render_graph.cpp:81,145-161`, `modules/render/src/passes/rt_lighting_pass.cpp:49,66`). Kare kaydı tek iş parçacığında; Renderer'a dışarıdan pass ekleme kancası yok.
- **Her karede tam yükleme:** tüm materyal tablosu, parametre bloğu ve örnek listesi her kare yeniden kurulup yüklenir; TLAS örnek listesi karşılaştırması CPU'dadır; `ShaderStore::find_by_key` O(shader sayısı)
  (`modules/render/src/stores/material_store.cpp:103-134`, `modules/render/src/frame/instances.cpp:31,81,136`, `modules/render/src/stores/shader_store.cpp:159-166`).
- **RT shader yeniden derlemesi render iş parçacığında, eşzamanlı:** özel yüzeyli shader eklenip silindiğinde sonraki RT karesi beş compute shader'ı derler (`modules/render/src/stores/shader_store.cpp:182-212`,
  `modules/render/src/paths/ray_traced_path.cpp:301`); iş sistemi ve önbellek bunu hafifletir, soğuk önbellekte kare takılır (ölçülmedi). En az bir alfa testli özel shader tüm RT ışın sorgularını "opak değil" yapar
  (`modules/render/shaders/include/astral/rt.glsl:299-312`).
- **Kabul edilip yok sayılan `render_mode`'lar ve `shader_type ui`:** `rt_simple`, `after_tonemap`, particle/volume `unshaded` ve ui modları ayrıştırıcıda geçerlidir, renderer okumaz (`modules/shader/src/ashader.cpp:372-382`).
- **Sıralamaya bağlı koşul:** `path_tracer_freeze_scene` bastırması `PathTracerReset` enum sırasına dayanır (`modules/render/src/paths/ray_traced_path.cpp:103-105`).
- **Boyut bütçesi:** `render_ray_traced_path` 79/80 satır, `GpuFrame` 24/25 üye (`python tools/check_sizes.py` çıktısı; `modules/render/src/paths/ray_traced_path.cpp:299`, `modules/render/src/gpu_layouts.hpp:123`).
  Yeni bir kare sabiti/pass eklemek bütçeyi aşar; `rt.glsl` 661 satırdır (`modules/render/shaders/include/astral/rt.glsl:661`).
- **Platform:** RT için donanımsal ray query gerekir (`gpu::Feature::RayTracing`); yazılım yedeği yoktur. VRAM bütçe API'si yok (gpu modülünde), geçici texture havuzu üst sınırsızdır
  (`modules/render/src/renderer.cpp:85`, `modules/render/src/render_graph.cpp:235-250`).

## Testler

`astral_render_tests` (CMake TIMEOUT 420 sn) 13 test dosyası (+ `test_main.cpp`) ve 142 `TEST_CASE` içerir (`modules/render/CMakeLists.txt:106-126`). Tüm testler `ASTRAL_SHADER_CACHE`'i build klasörüne yönlendirir (kök `CMakeLists.txt` test özelliği; denetim: `cmake/CheckTestEnvironment.cmake:2`).

| Dosya | Kapsam |
|---|---|
| `modules/render/tests/renderer_test.cpp` (63) | Raster ve RT uçtan uca: ilkel mesh'ler, derinlik, instancing, karo ışıkları, SDF (CSG, iç içe, derinlik), güneş/nokta/rect ışık, RT gölge, ayna, GI renk akışı, materyal paylaşımı, doku/normal harita, hareket vektörü, `update_mesh`, sıkıştırılmış mip, GPU zamanları, debug çizgi, UI, font, `.ashader` (raster + RT + hata geri dönüşü), `blend_*`, sky/post shader, aerial, froxel, duman, sky IBL, partikül, doku projeksiyonu (yönelim, normal) |
| `modules/render/tests/m13_pbr_test.cpp` (18) | Çok-saçılımlı GGX ve Charlie/anizotropi/Beer-Lambert matematik sözleşmeleri; clearcoat, sheen, anizotropi, transmission, subsurface, ıslaklık+decal, detay+POM modelleri raster/RT/PT arasında; path tracer birikimi, sıfırlama, telemetri, freeze; yansıma pürüzlülük kesimi |
| `modules/render/tests/render_audit_test.cpp` (17) | Matematik sözleşmeleri (VNDF, oktahedral, offset_ray, normal matrisi, sRGB/bloom) ve regresyonlar (DDGI sıfırlama/FP16 taşması, bloom DC, alfa testli RT gölge, forward PBR birimleri, rect MIS, unshaded, ReSTIR-RIS eşdeğerliği, sanal yansıma yeniden izdüşümü) |
| `modules/render/tests/stores_test.cpp` (6), `passes_test.cpp` (4), `render_graph_test.cpp` (5) | Store yaşam döngüleri, `prepare_instances` gruplama/sıralama/TLAS, RT shader yeniden kurma/hata/kurtarma; gölge/DDGI/zamansal geçmiş/bloom/post pass'leri; graph sırası, ayıklama, havuz, erişim denetimi |
| `modules/render/tests/msr_regression_test.cpp` (9) | REN03/05/06/07/08 (GrowBuffer, ofset, tek boyutlu mip, iterasyon sınırı, indeks) ve ADR-0042 int uniform'ları |
| `modules/render/tests/surface_shader_test.cpp` (8), `material_file_test.cpp` (5), `material_crlf_test.cpp` (1), `particle_builtins_test.cpp` (2) | Surface shader aşamaları, satır eşlemesi, iş sistemi+önbellekle eşitlik, RT varyantları; `.material` ebeveyn zinciri ve uniform değerleri; CRLF; partikül yerleşikleri |
| `modules/render/tests/mips_reference_test.cpp` (3), `renderer_busy_test.cpp` (1) | `build_mips` referans filtreyle eşitlik ve iş sistemiyle aynı bayt; busy callback sayımı |

**Kapsanmayanlar / zayıf noktalar:**
- Çoğu test `if (!f.device) return;` ile GPU yoksa assertion çalıştırmadan geçer; RT testleri RT donanımı yoksa atlanır (ör. `modules/render/tests/renderer_test.cpp:325`). Lavapipe golden testleri bu PC'de atlanır (lead doğrulaması).
- `"the roughness cutoff saves ray-traced lighting time on matte scenes"` süreyi yalnız loglar, tasarruf iddiasını doğrulamaz (`CHECK(full >= -1.0f)`) (`modules/render/tests/m13_pbr_test.cpp:734-754`).
- Doku projeksiyonu yönelim düzeltmesi (31e2c48) yalnız raster yolda test edilir (`Fixture` ray_tracing=false); RT/PT için yalnız Triplanar'ın kırmızı/mavi yarısı kontrol edilir
  (`modules/render/tests/renderer_test.cpp:1591-1660`, `:607-624`). Aynı `projection.glsl` kullanıldığı için yönelim paylaşılır ama RT'de ayrıca doğrulanmamıştır.
- PT-hibrit eşdeğerliği yalnız ortalama parlaklıktır (`modules/render/tests/m13_pbr_test.cpp:405`); RT/PT'de MeshUV normal haritası/detay eksikliği (B bölümü) test edilmez. ReSTIR yalnız RIS parlaklığıyla karşılaştırılır; çok rect ışık,
  > 2 eşiği çevresi, DDGI üst sınırları, RenderGraph ayırma hatası enjeksiyonu (REN02), TAA hayaletlenmesi ve denoiser kalitesi için ölçüt yoktur. Performans regresyon testi yoktur.
- **Kararsız:** paralel yükte `astral_render_tests` içindeki DDGI regresyon testinde (`modules/render/tests/render_audit_test.cpp:301`) SIGSEGV gözlendi; seri çalışmada geçer; kök neden araştırılmadı (lead gözlemi).
- Hardware golden testleri (`golden_hw_*`, 7 örnek) bu PC'de (RTX 3060) tonlama farkıyla başarısız; neden araştırılmadı (lead gözlemi). tsan/asan Windows'ta çalıştırılmadı.

## Performans

Yalnız ölçülmüş sayılar; ortam: RTX 3060, MSVC 19.51, Ninja, `examples/the_architect` (kaynak: lead ölçümleri, 2026-11-26). Hesaplananlar ayrıca belirtilmiştir.

| Ölçüm | Sonuç | Not |
|---|---|---|
| Başlangıç (`--capture --frames 4`), SPIR-V önbelleği öncesi | Debug 19.1 s, Release 4.18 s | |
| Başlangıç, önbellekli | Debug soğuk 11.2 s / sıcak 3.95 s; Release soğuk 2.53 s / sıcak 1.76 s | Kalan maliyet: cihaz+pencere 0.3-0.6 s, DevTools/ImGui 0.2 s (Release) / 0.65 s (Debug), sıralı texture çözme/yükleme |
| `rt.lighting`, 1080p, rt_high, Release | 10.43 ms (varsayılan); `max_reflection_roughness 0.35` ile 7.54 ms | Fark ≈ %27.7 (hesap). Test yalnız loglar, assert etmez |
| ReSTIR DI temporal+spatial, rt_ultra | ≈ 10.9 ms | Harici rapor, yeniden ölçülmedi |
| À-trous denoiser | 2.6 ms (2 geçiş) / 4.4 ms (3 geçiş) | Harici rapor, yeniden ölçülmedi |

Test içinde loglanan ancak sabit sayı olarak kaydedilmeyen ölçümler: `build_mips` (1024×1024 sRGB, referans/seri/4 işçi) ve `rt.lighting` süresi (`modules/render/tests/mips_reference_test.cpp:104-119`,
`modules/render/tests/m13_pbr_test.cpp:734-754`);  `build_mips` testi yalnız tablo-güdümlü filtrenin eski algoritmadan yavaş olmadığını doğrular (`CHECK(serial < reference)`), `rt.lighting` testi hiçbir şey doğrulamaz. Kare süresi, VRAM kullanımı, raster yol maliyeti, path tracer örnek hızı,
DDGI/denoiser/TAA tekil süreleri, farklı çözünürlükler ve çoklu ışık ölçeklenmesi: **Ölçülmedi**.

## Kararlar

- [ADR-0008](../decisions/0008-bindless-bda-glsl.md): bindless texture, buffer device address, GLSL
- [ADR-0009](../decisions/0009-render-graph-hybrid-deferred.md): render graph üzerinde hibrit deferred renderer
- [ADR-0011](../decisions/0011-hybrid-ray-traced-lighting.md): donanımsal ray query ile hibrit ışık
- [ADR-0012](../decisions/0012-render-api-contract.md): render API sözleşmesi (çıkar → çiz)
- [ADR-0018](../decisions/0018-terrain-and-sdf-world.md): SDF dünyası
- [ADR-0022](../decisions/0022-shader-and-material-language.md): `.ashader` ve `.material`
- [ADR-0024](../decisions/0024-volumetric-vfx.md): hacimsel VFX, GPU partikül, `volume`/`particle` shader türleri
- [ADR-0030](../decisions/0030-refactor-rules-and-size-budget.md): refaktör kuralları ve boyut bütçesi
- [ADR-0031](../decisions/0031-hardware-independent-golden-tests.md): donanımdan bağımsız görüntü testleri
- [ADR-0033](../decisions/0033-advanced-pbr-and-path-tracer.md): gelişmiş PBR, decal, path tracer (M13)
- [ADR-0042](../decisions/0042-typed-shader-parameter-words.md): shader parametrelerinde tipli word ABI

Eksik kayıtlar: görev tanımı SPIR-V önbelleği/derleme servisleri ve busy callback (ADR-0047/0048), path tracer sıfırlama telemetrisi ve donma (ADR-0049), yansıma pürüzlülük kesimi (ADR-0050) ve doku
projeksiyonu yönelimi (ADR-0051) için ADR numaraları verir; bu ağaçta `docs/decisions/` en çok 0044'e kadardır ve bu numaralı dosyalar yoktur. Davranışları yukarıda koddan belgelendi; ADR dosyaları
eklenirse buraya bağlanmalıdır. Tarihsel notlar: `docs/archive/renderer/` ve `docs/archive/audits/rendering-deep-audit-2026-11-25.md` (REN01-REN10'un çoğu kodda kapalı görünüyor, tek tek doğrulanmadı; RR01, RR02, RR05 benzeri açık sorunlar
yukarıdaki zayıf yanlara yeniden yazıldı).
