# Astral::Render

Katman 2. GPU üzerinde çizim. **ECS'i bilmez**: `RenderScene` (mesh'ler, SDF şekilleri, ışıklar,
güneş, ortam) çizer; App bunu component'lerden doldurur.

## API sözleşmesi ([ADR-0012](../decisions/0012-render-api-contract.md))

```cpp
astral::Renderer renderer(device, swapchain->format(), settings);
MeshHandle cube = renderer.create_mesh(astral::make_cube());                     // kaynak: bir kez
MaterialHandle red = renderer.create_material({.base_color = {1, 0, 0, 1}, .roughness = 0.4f});
// her kare: çıkar (extract) → çiz. Renderer sahneyi saklamaz; span'ler render() bitene kadar geçerli.
MeshInstance inst{.mesh = cube, .material = red, .transform = world, .previous_transform = world_last_frame};
renderer.render(cmd, image, camera, {.meshes = std::span(&inst, 1), .lights = lights, .sun = sun});
renderer.set_settings(s);   // çalışma anında; teknik/GI değişirse geçmiş sıfırlanır
```

| Kavram | Kural |
|---|---|
| Kaynaklar | `MeshHandle`, `MaterialHandle`: Renderer'ın sahip olduğu handle'lar. Silinmiş handle güvenli: mesh atlanır, materyal varsayılana düşer |
| Kare verisi | `RenderScene` saf veri (span'ler). ECS'i bilmez; App veya kendi motorun doldurur |
| Hareket | `previous_transform` verilirse nesne başına hareket vektörü (TAA/denoiser hayalet bırakmaz); verilmezse hareketsiz sayılır |
| Ayarlar | `RenderSettings` hem kurucuda hem `set_settings` ile; `settings()` ile okunur |
| Işıklar | `PointLight`, `RectLight`, `DirectionalLight`, `Environment` saf veri; App'te component olarak da kullanılır |
| Texture | `create_texture(ImageData)` (mip zinciri otomatik, sRGB'de lineer uzayda filtrelenir; ya da `ImageData::levels` + `format` ile cook edilmiş hazır zincir: RGBA8, BC7, BC5 — `supports(ImageFormat)`), `update_texture` handle'ı korur. Normal map'in Z'si shader'da X/Y'den kurulur (iki kanallı BC5 için). `MaterialDesc`: `base_color_texture`, `metallic_roughness_texture` (G pürüzlülük, B metal), `normal_texture` (+`normal_scale`, tangent gerekmez), `emissive_texture` |
| Hot-reload | `update_mesh`, `update_texture`, `update_material`: aynı handle, yeni veri |

İki yol var: cihaz ray tracing destekliyorsa ve `RenderSettings::ray_tracing` açıksa (varsayılan)
**ray-traced yol** ([ADR-0011](../decisions/0011-hybrid-ray-traced-lighting.md)), değilse **raster yol**.
`renderer.ray_tracing()` hangisinin kullanıldığını söyler.

## Ray-traced yol

```
TLAS build ─► gbuffer (+material_ext) ─► sdf ─► decals (varsa) ─► ddgi.trace ─► ddgi.update (×2)
          ─► restir.temporal ─► restir.spatial (3+ ışık) ─► rt.lighting ─► denoise.temporal
          ─► denoise.atrous (×N) ─► composite ─► sky ─► taa ─► bloom ─► tonemap
```
`RenderSettings::path_tracer = true` iken hibrit geçişler yerine `path_tracer` (`RGBA32Float` ping-pong birikimli çok sekmeli NEE+MIS yansız yol izleyici) → `tonemap` → `post` → `ui` çalışır.

| Pass | Ne yapar |
|---|---|
| TLAS | Her kare instance'lardan yeniden kurulur; BLAS mesh oluşturulurken bir kez |
| `gbuffer` / `sdf` | 5 renk hedefi (`albedo`, `normal`, `emissive` [a=kodlanmış `ShadingModel`], `velocity`, `material_ext`); POM, RNM detay normal ve ıslaklık burada işlenir |
| `decals` | `RenderScene::decals` boş değilse G-buffer (`albedo`, `normal`, `emissive`, `material_ext`) üzerine kutu-izdüşümlü boya/pürüzlülük/ıslaklık uygular (boşsa sıfır maliyet) |
| `path_tracer` | `RenderSettings::path_tracer` açıldığında tüm M13 BSDF'lerini, decal'ları, SDF'leri ve özel `.ashader`'ları yansız çoklu sekme + Russian roulette ile biriktirir (kamera hareketinde sıfırlanır) |
| `ddgi.trace` | Her prob kare başına `rays_per_probe` ışın atar (Fibonacci küresi, her kare rastgele döner); vuruşlar doğrudan ışık + **önceki karenin probları** ile ışıklanır → sekmeler zamanla birikir |
| `ddgi.update` | Işınlar oktahedral atlaslara (irradiance 8×8, mesafe momentleri 16×16, kenarlıklı) histerezisle karışır |
| `restir.temporal` / `restir.spatial` | 3+ yerel ışıkta (`RenderSettings::restir_di`) uzay-zamansal ReSTIR DI (Bitterli 2020): 24 aday + başlangıç görünürlük ışını, zamansal yeniden kullanım ($M \le 20$) ve yansız görünürlük-farkındalı MIS uzamsal yeniden kullanım |
| `rt.lighting` | Piksel başına: güneş (açısal yarıçaplı koni), yerel ışıklar (ReSTIR rezervuarı ya da ≤2 ışıkta doğrudan), **yakın alan GI** (1 kosinüs ışını: `near_gi_radius` içinde vurursa o yüzeyin gerçek ışığı, yoksa DDGI); **yansıma** (GGX VNDF + RectLight MIS, `specular.a`'da sanal yansıma derinliği) |
| `denoise.temporal` | Diffuse yüzey hareketiyle, speküler ise pürüzlülüğe göre harmanlanmış **sanal yansıma derinliği** ve 3×3 varyans kırpmasıyla bağımsız reprojekte edilip birikir; parlaklık momentleri tutulur |
| `denoise.atrous` | Kenar duyarlı dalgacık filtresi (normal × düzlem mesafesi × varyansa göre parlaklık); ayna yüzeyler filtrelenmez |
| `composite` | `albedo × diffuse + DFG × speküler + emisyon` (`render_mode unshaded` doğrudan `ALBEDO + EMISSION`); gökyüzü; alan ışıkları kameraya görünür |
| `sky` | İsteğe bağlı özel `shader_type sky` geçişi (hem mesh derinliği hem SDF derinliği == 1.0 olan piksellerde) |
| `taa` | Catmull-Rom geçmiş, YCoCg varyans kırpma, en yakın derinlikle reprojeksiyon |
| `bloom` | CoD:AW / RDR2 yöntemi: 13 dokunuşlu aşağı örnekleme (ilk seviyede Karis ortalaması), tent filtreyle yukarı |

SDF'ler TLAS'ta değildir: her RT ışını üçgen vuruşuna kadar SDF sahnesini de raymarch eder; böylece SDF'ler
gölge atar, yansır ve GI'a katılır.

```cpp
RenderSettings s;                       // varsayılanlar iyi bir başlangıç
s.gi = {.probes_x = 8, .probes_y = 7, .probes_z = 10, .rays_per_probe = 128,
        .volume_min = {-3, 0, -3.5f}, .volume_max = {3, 5, 5.5f}};   // eşitse: mesh'lere otomatik sığar
s.debug_view = DebugView::DiffuseLight; // Albedo, Normals, DiffuseLight, Specular
RectLight panel{.position = {0, 4.99f, 0}, .right = {0.85f, 0, 0}, .up = {0, 0, 0.85f}, .intensity = 18};
renderer.render(cmd, image, camera, {.meshes = items, .rect_lights = std::span(&panel, 1)});
renderer.reset_history();               // kamera kesmelerinde
```

Birimler: güneş/nokta ışık yoğunlukları raster yoldaki gibi (π ile çarpılmış); `RectLight` ve emisyon
**ışıma** (radiance) olarak; gökyüzü/zemin renkleri ışıma olarak ıska ışınlarına girer (`ambient` yalnız raster
yoldadır). Varsayılan gökyüzü bulutlu bir gün kadar parlaktır; iç mekânlarda koyulaştırın.

## Raster yol — kare akışı (render graph)

```
shadow ─┐
gbuffer ─┼─► sdf (compute) ─► lighting (compute, HDR) ─► tonemap ─► hedef
         └───────────────────────┘
```

| Pass | Ne yapar |
|---|---|
| `shadow` | Güneşten bakan ortografik derinlik haritası (2048², tüm sahneyi kapsar) |
| `gbuffer` | Mesh'ler: albedo+roughness, normal+metallic, emissive, derinlik. Aynı mesh = tek instanced çağrı |
| `sdf` | Her piksel için SDF sahnesini mesh derinliğine kadar raymarch eder; kendi G-buffer'ını yazar. SDF yoksa pass hiç eklenmez |
| `lighting` | Mesh/SDF'yi derinliğe göre birleştirir; GGX PBR; güneş (PCF gölge × SDF yumuşak gölge), nokta ışıklar, gök/yer ambient, gökyüzü yansıması |
| `tonemap` | ACES, pozlama; sRGB olmayan hedeflerde gamma |

## SDF şekilleri

Şekiller **liste sırasıyla** birleşir: `Union`, `Subtract`, `Intersect`, `SmoothUnion` (eriyerek
birleşme, renkler de karışır), `SmoothSubtract`. Primitifler: küre, kutu (yuvarlatılmış), torus,
kapsül, silindir. App'te `Sdf{shape, order}` component'i; `order` küçük olan önce.

## RenderGraph (ileri seviye)

Pass'ler texture'ları nasıl kullandığını bildirir (`read`, `write_storage`, `color`, `depth`, `copy_*`);
graph bariyerleri, render pass açma/kapamayı, geçici texture havuzunu ve **sonucu kullanılmayan
pass'leri ayıklamayı** kendisi yapar. Yeni bir teknik = yeni pass; mevcut pass'lere dokunulmaz.
Bir pass bir texture'ı tek bir şekilde kullanır (okuma+yazma = iki pass veya ping-pong texture).
`RenderGraph` tek başına kullanılabilir; Renderer'ın kendi karesine dışarıdan pass ekleme kancası
henüz yok.

```cpp
graph.add_pass("blur", [&](PassBuilder& b) { b.read(hdr); b.write_storage(blurred); },
               [&](PassContext& ctx) { /* dispatch */ });
```

## Bilinen sınırlar


- `build_mips` box filtre (normal map mip'lerini cook adımı ayrıca normalize eder). Daha keskin filtreler
  (Kaiser) ileride.

### Ray-traced yol
- SDF'lerin hareket vektörü yalnız kameradan türetilir (SDF'ler kareler arası durağan varsayılır).
- İkincil vuruşlarda ortam speküleri DDGI'dan yaklaşık olarak gelir.
- Şeffaflık/kırılma, saç, hacimsel sis/bulut (atmosfer fazı) yok. Prob taşıma (relocation) yok: duvar içinde
  kalan problar Chebyshev ve arka yüz testiyle bastırılır.

### Raster yol

- Tek gölge haritası tüm sahneyi (ışık uzayında sıkıca) kaplar: çok büyük sahnelerde çözünürlük düşer → cascaded shadow maps.
- Kamera bir SDF şeklinin içindeyse o şekil şeffaf görünür.
- SDF'ler her pikselde tüm şekil listesini değerlendirir: onlarca şekle kadar uygun; daha fazlası için
  eski v2'deki seyrek brick bake + mesafe alanı önbelleği taşınacak.
- Doku/materyal haritası, IBL (yalnız gökyüzü rengi yansıması), TAA, bloom, şeffaflık, frustum culling yok.
- Nokta ışıklar gölge atmaz; tüm ışıklar her pikselde döngüyle hesaplanır (clustered/tiled yok).

## Debug çizim, 2D katman, metin, GPU zamanları (M8)

- `RenderScene::debug_lines` (`DebugLine{from, to, color, on_top}`): tonemap'ten sonra derinlik testli ya da
  üstte çizilir. `DebugDraw` bunları üreten yardımcı (şekiller, süreli çizgiler, `advance(dt)`).
- `RenderScene::ui` (`UiLayer`: `UiVertex` + index + `UiCommand{texture, clip, sdf_softness}`): tek `ui`
  pass'i; ImGui, HUD ve ileride sprite'lar aynı biçimi üretir. Katmanlar sırayla çizilir.
- `Font::create(ttf, FontDesc)`: stb_truetype ile SDF atlası; `measure`, kerning, `line_height`.
  `UiCanvas`: `rect`, `frame`, `image`, `text`; `pack_color`.
- `Renderer::gpu_timings()` / `gpu_frame_ms()`: render graph pass'lerinin GPU süreleri (timestamp yoksa boş).
- `ImageData::format` + `levels` (cook edilmiş BC görüntüler ve hazır mip zinciri), `supports(ImageFormat)`.

## Özel shader'lar ve materyaller (M10)

Karar: [ADR-0022](../decisions/0022-shader-and-material-language.md). Dil: [shader.md](shader.md); kullanım:
[rehber](../guides/shaders-and-materials.md).

| API | Ne |
|---|---|
| `compile_surface_shader(kaynak, ad)` | `.ashader` (surface) → G-buffer, gölge (alfa testliyse gölge fragment'i de) ya da şeffaf (`blend_*`) aşamaları. Üretilen kod kullanıcı `vertex()`/`surface()` fonksiyonlarını şablona sarar; hatalar `.ashader` satırıyla |
| `compile_screen_shader(kaynak, ad)` | `shader_type post` / `sky` → tam ekran fragment |
| `Renderer::create_shader(build)` / `destroy(shader)` | Pipeline'lar; materyaller `MaterialDesc::shader` ile bağlanır |
| `MaterialDesc::shader`, `params`, `textures` | Shader'ın uniform bloğu (`layout_uniforms` düzeni, float olarak) ve doku slotları; ayarlanmamış doku beyaz (normal haritası: düz) okunur |
| `RenderScene::time`, `sky`, `post` | `TIME`; arka plan materyali; tonemap sonrası efekt zinciri |
| `resolve_material`, `evaluate_uniforms` (`material_file.hpp`) | `.material` ebeveyn zinciri ve değerlerin parametre bloğuna çevrilmesi (saf veri) |
| `Renderer::shader_error()` | Özel yüzeyler ışın izleme shader'larına giremediyse nedeni |

**Çizim sırası:** opak mesh'ler shader'a göre gruplanır (raster G-buffer ve RT G-buffer geçişleri aynı
mantık) → aydınlatma → **sky** (yalnız derinliği 1 olan piksellerde, HDR'a) → tonemap → **şeffaf** geçiş
(`blend_mix`/`blend_add`; arkadan öne sıralı, derinlik testli, ileri aydınlatma: güneş + nokta ışıklar + ortam;
tonemap shader içinde) → **post** zinciri (ping-pong) → debug çizgileri → UI.

**Işın izleme tutarlılığı:** RT compute shader'ları (`rt_lighting`, `ddgi_trace`), her özel `surface()`
fonksiyonu eklenmiş hâliyle çalışma anında yeniden derlenir (`compile_rt_variants`); özel shader yoksa
önceden derlenmiş olanlar kullanılır. Parametre bloğunun adresi ve `TIME` `Frame::custom` içinde taşınır.

**Bilinen sınırlar (M10):** şeffaf yüzeyler gölge düşürmez, gölge almaz, RT yansıma/GI'da yoktur ve kırılma
(ekran görüntüsünü bükme) yok; sky yalnız arka planı değiştirir (aydınlatma ve yansımalar `Environment`
renklerini kullanır); post için HDR öncesi nokta yok; RT varyantında `vertex()`, türevler ve normal haritası
yok; deforme mesh'ler için BLAS güncellemesi yok (RT'de deformasyon görünmez).

## Dosya düzeni

`modules/render/src/` altındaki iç düzen:

- `gpu_layouts.hpp`: `shaders/include/astral/scene.glsl` ile eşleşen skaler GPU veri yapıları, kaynak kayıtları ve push constant blokları (`static_assert` boyut/ofset denetimli).
- `stores/mesh_store.{hpp,cpp}`: mesh handle havuzu, vertex/index tamponları ve BLAS yaşam döngüsü.
- `stores/texture_store.{hpp,cpp}`: doku handle havuzu, `build_mips`, yükleme ve yedek 1×1 dokular.
- `stores/material_store.{hpp,cpp}`: materyal havuzu, varsayılan materyal, GPU materyal tablosu ve `.ashader` parametre bloğu paketleme.
- `stores/shader_store.{hpp,cpp}`: yüzey/ekran shader pipeline havuzu ve özel `.ashader` yüzey parçalarıyla yeniden derlenen RT pipeline'ları (`rt_lighting`, `ddgi_trace`).
- `frame/frame_slot.hpp`: kare halka tamponları (`GrowBuffer`, `FrameSlot`, `FrameRing`).
- `frame/instances.{hpp,cpp}`: kare başına örnek hazırlama (`prepare_instances`), şeffaf sıralama ve TLAS kurulumu.
- `passes/frame_inputs.hpp`, `passes/render_passes.hpp`: kare girdi yapısı (`FrameInputs`, `GBufferTextures`) ve tüm pass'lerin sahibi (`RenderPasses`).
- `passes/*.{hpp,cpp}`: kendi pipeline'ına ve geçmiş/ping-pong dokularına sahip bağımsız pass türleri (`shadow`, `gbuffer`, `sdf`, `decal`, `lighting`, `ddgi`, `rt_lighting`, `path_tracer`, `denoise`, `composite`, `sky`, `taa`, `bloom`, `tonemap`, `transparent`, `post`, `debug_lines`, `ui`).
- `surface_shader.cpp`, `surface_shader_rt.cpp`: `.ashader` G-buffer/gölge/forward derlemesi ve RT/Path Tracer/ekran varyantı birleştirmesi (M13 `shading_model` + built-in destekli).
- `paths/raster_path.{hpp,cpp}`, `paths/ray_traced_path.{hpp,cpp}`: raster ve ışın izlemeli kare bileşimleri (yalnız kare verisini hazırlayıp pass'leri sırayla grafa ekler).
- `renderer.cpp`: ince `Renderer` cephesi (store'lar, pass'ler, ayarlar ve yol seçimi).
