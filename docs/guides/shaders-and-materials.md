# Shader ve materyal iş akışı

> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi. Kaynaklar: `modules/render/src/material_file.cpp`,
> `modules/app/src/material_library.cpp`, `modules/app/src/services/render_bridge.cpp`, `modules/render/shaders/include/astral/projection.glsl`.
> Bu oturumda örnek çalıştırılmadı, görsel sonuç doğrulanmadı.
> Dilin kendisi (`shader_type`, `render_mode`, `shading_model`, yerleşik değişkenler, particle `AGE`/`LIFETIME`/`LIFE_T`) için
> [ashader-language.md](ashader-language.md).

Bu rehber şunları anlatır: `.material` dosyası, iki materyal türü, doku projeksiyonu, materyalin sahneye/oyuna nasıl bağlandığı,
hot reload, araçlar ve sınırlar.

## 1. İki dosya, iki materyal türü

| Dosya | İçerik |
|---|---|
| `.ashader` | Yüzeyin, gökyüzünün, son işlemin ya da efektin GLSL tarifi ([ashader-language.md](ashader-language.md)) |
| `.material` | JSON: hangi shader (ya da standart PBR), hangi değerler, isteğe bağlı ebeveyn |

`.material` iki türden biridir (`modules/render/src/material_file.cpp:52-61`):

| Tür | Yazılış | Değerler nereye gider |
|---|---|---|
| **Özel shader** | `"shader": "shaders/x.ashader"` | `.ashader` içindeki `uniform`'lara (`evaluate_uniforms`) |
| **Standart PBR** | `"type": "pbr"` | `MaterialDesc` alanlarına (`build_pbr`) |

`type` ve `shader` birlikte yazılırsa hata alırsın: `expected type pbr or a shader path` (`modules/render/src/material_file.cpp:54-57`).
Belgenin başı her zaman `"format": "astral.material", "version": 1` olmalı. Değilse hata `not an astral.material v1 document` olur
(`modules/render/src/material_file.cpp:48-50`).

### 1.1 Özel shader materyali

```json
{ "format": "astral.material", "version": 1,
  "shader": "shaders/lava.ashader",
  "values": { "glow_color": [1.0, 0.35, 0.05], "pulse_speed": 3.2, "noise": "textures/noise.png" } }
```

Değer kuralları (`modules/render/src/material_file.cpp:73-125`):

| Uniform tipi | Kabul edilen değer | Not |
|---|---|---|
| `float`, `int`, `bool` | sayı ya da `true/false` | Kelimeler [ADR-0042](../decisions/0042-typed-shader-parameter-words.md)'ye göre yazılır: float bitleri, int32, bool 0/1 |
| `vec2/3/4` | Tam boyda dizi **ya da tek sayı** | Tek sayı her bileşeni doldurur (`modules/render/src/material_file.cpp:102-106`) |
| `sampler2D` | Varlık yolu (string) | `source_color` sRGB, `hint_normal` düz normal varsayılanı (`modules/render/include/astral/render/material_file.hpp:46-51`). Boş string = doku yok |

- Yazılmayan uniform `.ashader` varsayılanını alır.
- Tipi yanlış değer ya da shader'da olmayan ad **uyarı** verir, materyal yine kurulur (`modules/render/src/material_file.cpp:116`, `modules/render/src/material_file.cpp:120-124`).
  Bu uyarılar `LogLevel::Warn` ile günlüğe düşer (`modules/app/src/material_library.cpp:233`). Yazım hatası materyali bozmaz ama sessizce varsayılan değeri kullanır.

### 1.2 Standart PBR materyali

```json
{ "format": "astral.material", "version": 1, "type": "pbr",
  "values": { "base_color": [0.8, 0.76, 0.7, 1], "roughness": 0.89, "base_color_texture": "textures/stone.png" } }
```

(`examples/materials_showcase/assets/workflow/materials/stone.material`)

- Doku yuvaları (string yol): `base_color_texture`, `metallic_roughness_texture`, `normal_texture`, `emissive_texture`,
  `occlusion_texture`, `detail_albedo_texture`, `detail_normal_texture`, `detail_height_texture` (`modules/app/src/material_library.cpp:186-194`).
  Renk yuvaları sRGB, diğerleri doğrusal yüklenir. `occlusion_texture`'ın **kırmızı** kanalı albedo'yu çarpar, `occlusion_strength` (0..1, varsayılan 1) ağırlığını belirler; paketli ORM dokusunu hem bu yuvaya hem `metallic_roughness_texture`'a verebilirsin (ADR-0055 D4). Raster G-buffer ve RT/path tracer vuruşlarında aynı uygulanır (`modules/render/shaders/gbuffer.frag`, `modules/render/shaders/include/astral/rt.glsl`).
- Diğer adlar `MaterialDesc` reflect alanlarıdır: `base_color`, `roughness`, `metallic`, `emissive`, `normal_scale`, `shading_model`,
  `clearcoat`, `sheen`, `anisotropy`, `transmission`, `subsurface`, `detail`, `wetness`, `mapping`
  (`modules/render/include/astral/render/renderer.hpp:465-466`). İç içe alanlar nesne olarak yazılır (örnek: `"clearcoat": {"strength": 1}`).
- `metallic_roughness_texture` glTF düzenindedir: G = roughness, B = metallic (`modules/render/include/astral/render/renderer.hpp:104`).
- Bilinmeyen ad **uyarıdır** (`is not a PBR parameter`), materyal kurulur ve geçerli değerler uygulanır; özel shader yoluyla aynı davranış (ADR-0055 D1). Yazım hatası çalışan sahneyi hot reload sırasında öldürmez, günlükte görünür.
  

## 2. Ebeveyn zinciri (`parent`)

```json
{ "format": "astral.material", "version": 1,
  "parent": "materials/stone.material",
  "values": { "mapping": { "projection": "Triplanar", "tile_size_m": 2, "blend_sharpness": 4, "world_space": true } } }
```

(`examples/materials_showcase/assets/workflow/materials/stone_triplanar.material`)

- Çocuk önce okunur, aynı ad için **çocuğun değeri kazanır** (`modules/render/src/material_file.cpp:62-64`).
- `shader`/`type` zincirde ilk bulunan yerden alınır. Çocuk başka bir `shader` yazarak shader'ı değiştirebilir
  (`modules/render/src/material_file.cpp:58-61`, test `modules/render/tests/material_file_test.cpp:48`).
- En fazla 16 seviye; döngü hatadır (`parent cycle through ...`) (`modules/render/src/material_file.cpp:9`, `modules/render/src/material_file.cpp:33-40`).
- **İç içe nesneler alan alan birleşir** (ADR-0055 D2): çocuk `mapping: {"tile_size_m": 4}` yazarsa ebeveynin `projection` ve `blend_sharpness` değerleri korunur, yalnız `tile_size_m` değişir. Diziler ve skalerler (örn. `base_color`) tamamen değiştirilir, eleman eleman birleşmez (`modules/render/src/material_file.cpp`, testler `modules/render/tests/material_file_test.cpp`).
- Hot reload zincirin tüm dosyalarını izler (`MaterialFile::dependencies`, `modules/render/include/astral/render/material_file.hpp:33`).

## 3. Doku projeksiyonu (`mapping`, yalnız `type: pbr`)

`TextureMapping` (`modules/render/include/astral/render/renderer.hpp:85-95`):

| Alan | Varsayılan | Hangi modda etkili |
|---|---|---|
| `projection` | `MeshUV` | `MeshUV`, `PlanarXY`, `PlanarXZ`, `PlanarYZ`, `Triplanar` (enum adıyla yazılır) |
| `uv_scale`, `uv_rotation` | `(1,1)`, `0` | **Yalnız `MeshUV`.** Planar/Triplanar'da okunmaz (`modules/render/shaders/include/astral/projection.glsl:14-20`) |
| `uv_offset` | `(0,0)` | Her modda (`modules/render/shaders/include/astral/projection.glsl:48-51`) |
| `tile_size_m` | `1` | Planar/Triplanar: bir doku tekrarı kaç metre. En az 0.001 (`modules/render/src/stores/material_store.cpp:92-95`) |
| `blend_sharpness` | `4` | Triplanar: yüz ağırlıklarının üssü, [1, 32]'ye kırpılır (`modules/render/src/stores/material_store.cpp:94`) |
| `world_space` | `true` | `true`: dünya konumundan. `false`: nesnenin yerel eksenleri ve kökünden (ölçek etkilemez, eksenler normalize) (`modules/render/shaders/include/astral/projection.glsl:22-27`) |

Yön kuralı ([ADR-0051](../decisions/0051-projected-texture-orientation.md)): her dikey yüzde doku V'si dünya yukarısına (+Y) bakar. U, yüze dışarıdan bakınca sağa gider: +Z → +X, −Z → −X,
+X → −Z, −X → +Z. Zemin/tavan: U = Z, V = X (`modules/render/shaders/include/astral/projection.glsl:40-51`). Normal haritası teğet
çerçevesi de aynı tabloyu kullanır (`modules/render/shaders/include/astral/projection.glsl:64-76`). `world_space` modunda kayan köken (floating origin)
fazı CPU'da `tile_size_m`'ye göre sarılır. Bu yüzden köken kayınca desen kaymaz (`modules/render/src/stores/material_store.cpp:97-99`,
test `modules/render/tests/renderer_test.cpp:568`).

Testler: yön `modules/render/tests/renderer_test.cpp:1611`, normal haritası `modules/render/tests/renderer_test.cpp:1642`, triplanar seçim
`modules/render/tests/renderer_test.cpp:590`. RT ve path tracer aynı yardımcıyı kullanır (`modules/render/shaders/include/astral/rt.glsl:19`)
ama yalnız raster yolu test edildi ([ADR-0051](../decisions/0051-projected-texture-orientation.md)).

**Özel `.ashader` yüzeylerinde projeksiyon yok.** Shader materyali kurulurken `MaterialDesc` yalnız `shader`, `params` ve `textures` ile doldurulur;
`mapping` varsayılan `MeshUV` kalır (`modules/app/src/material_library.cpp:229-238`). Planar ya da triplanar eşleme istersen `WORLD_POSITION` ile
shader içinde kendin yaz.

## 4. Materyali kullanmak

Bütün yollar `MaterialLibrary::get` içinden geçer. Materyal ilk kullanımda yüklenir ve önbelleğe alınır; aynı yol aynı handle'ı verir
(`modules/app/src/material_library.cpp:253-261`).

| Nerede | Nasıl | Kaynak |
|---|---|---|
| Sahne, temel şekil | `astral::PrimitiveMesh { shape, material_file }`; `material_file` doluysa satır içi `material`'ı geçersiz kılar | `modules/app/include/astral/app/components.hpp:28-33`, `modules/app/src/services/render_bridge.cpp:286-293` |
| Sahne, satır içi | `astral::PrimitiveMesh.material` (`MaterialDesc`); aynı açıklamalar tek materyali paylaşır | `modules/app/src/services/render_bridge.cpp:288-292` |
| glTF model | `ModelRef.material_overrides: [{slot, material_file}]`. `slot` kaynak materyal adıyla **tam ve tek** eşleşmeli, yoksa uyarı | `modules/assets/include/astral/assets/asset_manager.hpp:122-131`, `modules/app/src/services/render_bridge.cpp:317-336` |
| Arazi | `TerrainRef.material_file` | `modules/app/include/astral/app/components.hpp:36-42`, `modules/app/src/services/terrain_bridge.cpp:90-91` |
| C++ | `ctx.materials().load_material(path)` → `MeshRenderer{mesh, material}` | `modules/app/include/astral/app/material_service.hpp:22-27` |
| Gökyüzü | `ctx.sky = load_material(...)` (`shader_type sky`), ya da `Atmosphere.sky_material` (yalnız `drive_sky = true` iken) | `modules/app/include/astral/app/app.hpp:112-113`, `modules/app/src/services/atmosphere.cpp:117-121` |
| Son işlem | `ctx.post.push_back(load_material(...))` (`shader_type post`); sırayla, tonemap sonrası | `modules/app/include/astral/app/app.hpp:114-115` |
| Efekt | `.vfx` izinde `"material"` (particle / volume / smoke); `volume` izinde zorunlu | `modules/app/src/services/vfx_runtime.cpp:83-87`, `modules/app/src/services/vfx_runtime.cpp:103` |

Örnek C++ bağlama (`examples/materials_showcase/src/stations/gallery.cpp:14-17`):

```cpp
ctx.sky = ctx.materials().load_material(gallery_sky_material);
if (const MaterialHandle post = ctx.materials().load_material(gallery_post_material); post.valid()) {
    ctx.post.push_back(post);
}
```

Başsız (headless) çalışmada ya da materyal kurulamazsa `load_material` geçersiz handle döner ve bu durum günlüğe yazılır. Geçersiz materyal
varsayılan materyalle çizilir (`modules/app/include/astral/app/material_service.hpp:22-27`, `modules/app/include/astral/app/components.hpp:14-15`,
test `modules/app/tests/material_app_test.cpp:123`).

Luau'dan materyal yüklemek ya da değiştirmek için API yok. `PrimitiveMesh.material` gibi iç içe reflect alanları script'ten yazılamaz
(kök `AGENTS.md` "Yapma").

## 5. Hot reload ve hatalar

- İzleme `AppDesc::hot_reload && pack yok` iken açıktır (`modules/app/src/app.cpp:250`, `modules/app/include/astral/app/app.hpp:232`).
  Paketten çalışan oyunda dosya izlenmez.
- `MaterialLibrary::update` her 0,25 sn dosya zamanlarına bakar. Sırası: dokular, değişen `.ashader`'lar (onlara bağlı materyaller yeniden
  kurulur), sonra her materyalin zinciri (`modules/app/src/material_library.cpp:263-304`).
- **Handle sabit kalır:** materyal yerinde güncellenir (`renderer_.update_material`). Bozuk düzenlemede hiçbir şey değişmez, son çalışan
  sürüm kalır (`modules/app/src/material_library.cpp:212-251`, test `modules/app/tests/material_app_test.cpp:44`).
- Eksik doku sonradan diske gelirse yüklenir ve materyaller yeniden bağlanır (`modules/app/src/material_library.cpp:63-94`,
  test `modules/app/tests/material_app_test.cpp:133`).
- Eski shader handle'ı, onu kullanan materyal kalmayınca yok edilir (`modules/app/src/material_library.cpp:96-114`).
- Hata biçimi `shader <yol>` / `material <yol>` + `dosya:satır: mesaj` (`modules/app/src/material_library.cpp:131-143`). CRLF dosyalar LF ile
  aynı sonucu verir (`modules/render/tests/material_crlf_test.cpp:35`).
- Özel shader'ların birleşik RT derlemesi başarısız olursa hata `MaterialService::shader_error()` ile okunur
  (`modules/app/include/astral/app/material_service.hpp:28-29`). Ayrıntı: [ashader-language.md §9](ashader-language.md#9-raster-ve-ışın-izleme-farkları).

## 6. Canlı ayar: Materials paneli

F1 → **Materials**: yüklü her materyalin uniform'ları listelenir. `range(...)` kaydırıcı olur, `source_color` renk seçici olur
(`modules/app/src/panels/materials_panel.cpp:33-44`). Dokular yalnız gösterilir, buradan değiştirilemez
(`modules/app/src/panels/materials_panel.cpp:59`). Değişiklik **yalnız çalışma anında** geçerlidir, dosyaya yazılmaz
(`modules/app/src/panels/materials_panel.cpp:88`, `modules/app/src/material_library.hpp:51-52`). Standart PBR (`type: pbr`) materyallerin uniform'u
olmadığı için panelde düzenlenecek parametreleri yoktur (`modules/app/src/material_library.cpp:313-320`).

## 7. Raster ve ışın izleme: özet

| Konu | Raster | RT (hibrit / path tracer) |
|---|---|---|
| Opak özel `surface` | Evet | `surface()` gövdesi RT parçasına çevrilir. `vertex()`, `NORMAL_MAP`, türevler yok |
| `blend_*` şeffaf yüzey | İleri geçiş | Yansıma/GI'da **yok** (TLAS'a girmez) |
| `alpha_test` | Evet | Ayrı alfa işlevi |
| Doku projeksiyonu (PBR) | Test edildi | Aynı kod, test edilmedi |
| `sky`, `post` | Evet | Aynı (ekran geçişleri) |

Ayrıntılar ve kaynaklar [ashader-language.md §4 ve §9](ashader-language.md#9-raster-ve-ışın-izleme-farkları) içinde.

## 8. Paketleme

`astral-cook` `.ashader` dosyalarını derleyerek doğrular ama SPIR-V'yi pakete koymaz. Shader'lar çalışma anında derlenir; SPIR-V disk
önbelleği (`ASTRAL_SHADER_CACHE`, [ADR-0047](../decisions/0047-spirv-disk-cache.md)) ikinci açılışı hızlandırır (`modules/assets/src/cook.cpp:280-292`).

## Bilinen sınırlar

- **Entity başına materyal ezme yok.** `MaterialOverride` bileşeni yok; yalnız glTF için slot adıyla `ModelRef::material_overrides` var
  (`modules/assets/include/astral/assets/asset_manager.hpp:122-131`). İki entity aynı `.material`'ı farklı renkte gösteremez, ayrı dosya gerekir.
- Ebeveyn birleştirmesi sığdır: iç içe nesneler (`mapping`, `clearcoat`, ...) bütün olarak değişir (§2).
- Özel shader yolunda yanlış ad ya da tip yalnız uyarıdır. PBR yolunda aynı hata materyali kurdurmaz. İki yol tutarsız.
- Doku projeksiyonu yalnız `type: pbr` materyallerinde var. Planar/Triplanar'da `uv_scale` ve `uv_rotation` okunmaz (§3).
- Projeksiyonun RT/path tracer görünümü test edilmedi ([ADR-0051](../decisions/0051-projected-texture-orientation.md)).
- Materials panelindeki değişiklikler kaydedilmez. Dokular ve PBR materyalleri panelden düzenlenemez.
- Luau'dan materyal API'si yok.
- Hot reload 0,25 sn aralıkla dosya zamanı yoklar. Dosya sistemi olayı kullanılmaz. Çok sayıda materyalde her yoklama tüm zinciri `stat` eder
  (çıkarım, ölçülmedi).
- Shader dili sınırları (derinlik okuma yok, `ui` türü yüklenmez, etkisiz `render_mode`'lar, SSS yarıçapı): [ashader-language.md](ashader-language.md#bilinen-sınırlar).

## Kararlar

- [ADR-0022](../decisions/0022-shader-and-material-language.md): shader ve materyal dili, `.material` ebeveyn zinciri.
- [ADR-0042](../decisions/0042-typed-shader-parameter-words.md): tipli parametre kelimeleri.
- [ADR-0047](../decisions/0047-spirv-disk-cache.md): SPIR-V disk önbelleği.
- [ADR-0051](../decisions/0051-projected-texture-orientation.md): planar/triplanar doku yönü.
