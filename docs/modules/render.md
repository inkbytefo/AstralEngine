# Astral::Render

Katman 2. GPU üzerinde çizim. **ECS'i bilmez**: `RenderScene` (mesh'ler, SDF şekilleri, ışıklar,
güneş, ortam) çizer; App bunu component'lerden doldurur.

```cpp
astral::Renderer renderer(device, swapchain->format());
MeshHandle cube = renderer.create_mesh(astral::make_cube());
renderer.render(cmd, image, camera, {.meshes = items, .sdf = shapes, .lights = lights, .sun = sun});
```

## Kare akışı (render graph)

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

```cpp
graph.add_pass("blur", [&](PassBuilder& b) { b.read(hdr); b.write_storage(blurred); },
               [&](PassContext& ctx) { /* dispatch */ });
```

## Bilinen sınırlar

- Tek gölge haritası tüm sahneyi kaplar (büyük sahnelerde bulanık/pikselli) → cascaded shadow maps.
- SDF'ler her pikselde tüm şekil listesini değerlendirir: onlarca şekle kadar uygun; daha fazlası için
  eski v2'deki seyrek brick bake + mesafe alanı önbelleği taşınacak.
- Doku/materyal haritası, IBL (yalnız gökyüzü rengi yansıması), TAA, bloom, şeffaflık, frustum culling yok.
- Nokta ışıklar gölge atmaz; tüm ışıklar her pikselde döngüyle hesaplanır (clustered/tiled yok).
