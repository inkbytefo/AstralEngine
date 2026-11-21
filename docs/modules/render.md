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
TLAS build ─► gbuffer ─► sdf ─► ddgi.trace ─► ddgi.update (×2) ─► rt.lighting ─► denoise.temporal
          ─► denoise.atrous (×N) ─► composite ─► taa ─► bloom.down (×6) ─► bloom.up (×5) ─► tonemap
```

| Pass | Ne yapar |
|---|---|
| TLAS | Her kare instance'lardan yeniden kurulur; BLAS mesh oluşturulurken bir kez |
| `gbuffer` / `sdf` | Raster ile aynı; projeksiyon TAA için her kare Halton(2,3) ile alt-piksel kaydırılır |
| `ddgi.trace` | Her prob kare başına `rays_per_probe` ışın atar (Fibonacci küresi, her kare rastgele döner); vuruşlar doğrudan ışık + **önceki karenin probları** ile ışıklanır → sekmeler zamanla birikir |
| `ddgi.update` | Işınlar oktahedral atlaslara (irradiance 8×8, mesafe momentleri 16×16, kenarlıklı) histerezisle karışır |
| `rt.lighting` | Piksel başına: güneş (açısal yarıçaplı koni), nokta ışıklar (ampul yarıçapı), dikdörtgen alan ışıkları — her biri 1 örnek + gölge ışını; **yakın alan GI** (1 kosinüs ışını: `near_gi_radius` içinde vurursa o yüzeyin gerçek ışığı, yoksa DDGI); **yansıma** (GGX görünür-normal örneklemesi, pürüzsüzde tam ayna) |
| `denoise.temporal` | Diffuse ve speküler sinyaller albedodan ayrılmış hâlde, dünya konumuyla reprojekte edilip geometri kontrolüyle birikir; parlaklık momentleri tutulur |
| `denoise.atrous` | Kenar duyarlı dalgacık filtresi (normal × derinlik × varyansa göre parlaklık); ayna yüzeyler filtrelenmez |
| `composite` | `albedo × diffuse + DFG × speküler + emisyon`; gökyüzü; alan ışıkları kameraya görünür |
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
- Yansımaların reprojeksiyonu yüzey hareketine göre (sanal derinlik yok): hızlı kamera hareketinde ayna
  yansımaları kısa süre bulanıklaşır.
- Alan ışıklarının speküler parlaması doğrudan değil yansıma ışınıyla gelir; ikincil vuruşlarda alan ışığı
  speküleri yok (ortam speküleri DDGI'dan yaklaşık).
- Işık başına piksel başına 1 gölge ışını: çok sayıda ışıkta pahalı (ReSTIR DI sonraki adım).
- Şeffaflık/kırılma, saç, hacimsel sis/bulut (atmosfer fazı) yok. Prob taşıma (relocation) yok: duvar içinde
  kalan problar Chebyshev ve arka yüz testiyle bastırılır.
- Bağımsız inceleme bu fazda yapılmadı.

### Raster yol

- Tek gölge haritası tüm sahneyi (ışık uzayında sıkıca) kaplar: çok büyük sahnelerde çözünürlük düşer → cascaded shadow maps.
- Kamera bir SDF şeklinin içindeyse o şekil şeffaf görünür.
- SDF'ler her pikselde tüm şekil listesini değerlendirir: onlarca şekle kadar uygun; daha fazlası için
  eski v2'deki seyrek brick bake + mesafe alanı önbelleği taşınacak.
- Doku/materyal haritası, IBL (yalnız gökyüzü rengi yansıması), TAA, bloom, şeffaflık, frustum culling yok.
- Nokta ışıklar gölge atmaz; tüm ışıklar her pikselde döngüyle hesaplanır (clustered/tiled yok).
