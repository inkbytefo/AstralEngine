# `.ashader` dil referansı

> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi. Kaynak gerçekleri: ayrıştırıcı `modules/shader/src/ashader.cpp`, şablon üretici
> `modules/render/src/surface_shader.cpp` ve `modules/render/src/surface_shader_rt.cpp`, motor include'ları `modules/render/shaders/include/astral/`.
> Shader örnekleri bu oturumda derlenmedi; derleme davranışı kodun okunmasından ve `modules/render/tests/` içindeki testlerden çıkarıldı.
> Materyal dosyaları, doku projeksiyonu ve C++ tarafı için [shaders-and-materials.md](shaders-and-materials.md).

`.ashader` = küçük, Godot benzeri bir **başlık** (`shader_type`, `render_mode`, `shading_model`, `uniform`, `varying`) + düz **GLSL 460** gövdesi.
Ayrıştırıcı yalnız başlığı okur; gövde motorun ürettiği şablona yerleştirilip glslang ile derlenir (`modules/shader/include/astral/shader/ashader.hpp:1-5`).
Hedef Vulkan 1.3 / SPIR-V 1.5 (`modules/shader/include/astral/shader/compile.hpp:1-5`).

## 1. Dosya yapısı ve satır numaraları

- Yorumlar `//` ve `/* */` desteklenir; CRLF `\n`'e çevrilir (`modules/shader/src/ashader.cpp:26-45`).
- Üst düzey `;` ile biten ifadeler başlık sayılır (`shader_type`, `shading_model`, `render_mode`, `uniform`, `varying`); `{ ... }` blokları fonksiyon/yapıdır.
  Fonksiyon adı = bloktan önceki son `(`'nin solundaki tanımlayıcıdır (`modules/shader/src/ashader.cpp:241-340`).
- Başlık ifadeleri gövdeden **boşlukla** silinir, satırlar korunur; gövde `#line 1` ile başlar, sonu `#line 100000` ile biter. Bu yüzden derleyici hataları
  doğrudan `.ashader` satırını gösterir; satır ≥ 100000 olan hatalar "(engine template)" etiketiyle `line = 0` raporlanır
  (`modules/render/src/surface_shader.cpp:61-81`).
- Hata biçimi: `dosya:satır: mesaj`. `shader_type` yoksa `1. satır: missing 'shader_type'` (`modules/shader/src/ashader.cpp:341`).
- `shader_type` dosyanın **başında** olmalı: `shading_model` ifadesi ondan önce yazılırsa tür henüz varsayılan `surface` sayılır (`modules/shader/src/ashader.cpp:255-264`).

## 2. İlk shader

```glsl
shader_type surface;

uniform vec3 base_color : source_color = vec3(0.08, 0.03, 0.02);
uniform vec3 glow_color : source_color = vec3(1.0, 0.25, 0.04);
uniform float pulse_speed : range(0.0, 10.0, 0.1) = 2.5;
uniform float glow_strength : range(0.0, 8.0, 0.1) = 3.0;

void surface() {
    float wave = 0.5 + 0.5 * sin(TIME * pulse_speed + WORLD_POSITION.y * 4.0);
    ALBEDO = base_color;
    ROUGHNESS = mix(0.85, 0.35, wave);
    EMISSION = glow_color * wave * glow_strength;
}
```

`assets/materials/ember.material`:

```json
{ "format": "astral.material", "version": 1,
  "shader": "shaders/pulsing_ember.ashader",
  "values": { "glow_color": [1.0, 0.35, 0.05], "pulse_speed": 3.2 } }
```

Sahnede `astral::PrimitiveMesh` bileşeninin `material_file` alanına verilir:

```json
"astral::PrimitiveMesh": { "shape": "Sphere", "material_file": "materials/ember.material" }
```

(`shape` enum **adıyla** `Cube | Sphere | Plane`, `modules/app/include/astral/app/components.hpp:23-33`; bileşen adı `astral::PrimitiveMesh`, `astral::Transform`, `astral::Name`.)

## 3. `shader_type`

| Tür | Giriş fonksiyonu | Çizildiği yer | Durum | Kaynak |
|---|---|---|---|---|
| `surface` | `surface()` ve/veya `vertex()` | G-buffer + gölge + ışın izleme varyantı; `blend_*` ise ileri (forward) şeffaflık | Çalışır | `modules/render/src/surface_shader.cpp:359-422` |
| `sky` | `sky()` (zorunlu) | Tam ekran; yalnız geometri olmayan pikseller | Çalışır | `modules/render/src/surface_shader_rt.cpp:257-290` |
| `post` | `post()` (zorunlu) | Tonemap sonrası tam ekran zinciri | Çalışır | aynı |
| `particle` | `particle()` (zorunlu), isteğe bağlı `motion()` | `.vfx` parçacık izi | Çalışır | `modules/render/src/surface_shader_rt.cpp:69-116` |
| `volume` | `volume()` (zorunlu) | `.vfx` hacim/duman (compute) | Çalışır | aynı |
| `ui` | — | — | **Ayrıştırılır ama kullanılamaz**: "shader_type ui is not supported yet" | `modules/app/src/material_library.cpp:157-158` |

- Bilinmeyen tür hatası: `(surface, particle, volume, post, sky, ui)` (`modules/shader/src/ashader.cpp:252`).
- `particle` ve `volume` shader'larında **özel `varying` yasaktır** (`expected particle() with no custom varyings`, `modules/render/src/surface_shader_rt.cpp:72-77`).
- `sky`/`post` için eksik `sky()`/`post()` hata verir (`modules/render/src/surface_shader_rt.cpp:266-269`).
- `astral-cook` `.ashader` dosyalarını derlenip derlenmediği için doğrular ama SPIR-V'yi pakete **koymaz** (`modules/assets/src/cook.cpp:280-292`);
  `ui` türü cook'ta doğrulanmaz.

## 4. `render_mode`

Virgülle ayrılmış liste, birden çok satır olabilir. Türe göre izin verilen adlar (`modules/shader/src/ashader.cpp:372-383`); listede olmayan ad `unknown render_mode` hatasıdır.

| Tür | İzin verilenler |
|---|---|
| `surface` | `alpha_test`, `cull_disabled`, `cull_front`, `unshaded`, `blend_mix`, `blend_add`, `shadows_disabled`, `rt_simple` |
| `particle` | `blend_mix`, `blend_add`, `unshaded` |
| `volume` | `unshaded` |
| `post` | `after_tonemap` |
| `sky` | (hiçbiri) |
| `ui` | `blend_mix`, `blend_add` |

Etkiler (yalnız kodda **gerçekten** bir şey yapanlar):

| Mod | Etki | Kaynak |
|---|---|---|
| `alpha_test` | `surface()` sonrası `if (ALPHA < ALPHA_SCISSOR) discard;`; gölge geçişine de fragment aşaması üretilir; ışın izlemede ayrı bir alfa işlevi (`astral_alpha_<ID>`, `ASTRAL_RT_ALPHA_TEST`) çalışır ve TLAS örneği "opak değil" işaretlenir | `modules/render/src/surface_shader.cpp:297`, `modules/render/src/surface_shader.cpp:397-416`, `modules/render/src/frame/instances.cpp:58-60` |
| `cull_disabled` | İki yüzü de çizer; arka yüzde `FRONT_FACING=false` ve `NORMAL` tersine çevrilir. Gölge geçişi her zaman `cull none` | `modules/render/src/surface_shader.cpp:289-290`, `modules/render/src/stores/shader_store.cpp:77-91` |
| `cull_front` | Yalnız arka yüzleri çizer | `modules/render/src/stores/shader_store.cpp:53` |
| `shadows_disabled` | Güneş gölge geçişine girmez (`casts_shadows = false`) | `modules/render/src/surface_shader.cpp:373` |
| `unshaded` | Aydınlatmayı atlar: çıktı `ALBEDO + EMISSION` (HDR; AO, roughness, metallic yok sayılır). G-buffer'da `SHADING_UNSHADED`, ileri geçişte doğrudan renk, RT'de `s.unshaded` | `modules/render/src/surface_shader.cpp:194`, `modules/render/src/surface_shader.cpp:211-217`, `modules/render/src/surface_shader.cpp:317-321`, `modules/render/shaders/include/astral/rt.glsl:640` |
| `blend_mix` | Yüzey G-buffer'dan çıkar, ileri şeffaflık geçişinde alfa karışımıyla (`src_alpha, 1-src_alpha`) çizilir | `modules/render/src/surface_shader.cpp:378-392`, `modules/render/src/stores/shader_store.cpp:54-69` |
| `blend_add` | Aynı geçiş, toplamsal karışım; ek olarak hava perspektifi/sis katkısı çıkarılır | `modules/render/src/surface_shader.cpp:199-205` |
| `particle` + `blend_mix` | Karışım alfa; **varsayılan toplamsaldır** (`out.additive = !blend_mix`) | `modules/render/src/surface_shader_rt.cpp:79` |
| `post` + `after_tonemap` | Tek geçerli mod ve zaten tek davranış (post her zaman tonemap sonrası) | `modules/render/src/paths/raster_path.cpp:111-118` |

**Kabul edilir ama etkisizdir:** `rt_simple` (hiçbir yerde okunmuyor; `modules/shader/src/ashader.cpp:375` dışında geçmez), `particle`/`volume` için `unshaded`, `ui` modları.
Bunları yazmak hata vermez, hiçbir şey değiştirmez.

**Şeffaf (`blend_*`) yüzeylerin yeri:**
- Ayrı **ileri geçiş**: opak sahne + sis hazırlandıktan sonra, **tonemap'ten önce** HDR hedefe çizilir; yani tonemap ve bloom'dan geçer
  (`modules/render/src/paths/raster_path.cpp:115-116`, `modules/render/src/passes/transparent_pass.cpp`). (Eski belgelerdeki "tonemap sonrası" bilgisi yanlıştı.)
- Derinlik testi var, derinlik yazma yok; sıralama **nesne merkezine uzaklık** (arkadan öne), üçgen sırası değil
  (`modules/render/src/frame/instances.cpp:65-80`, `modules/render/src/stores/shader_store.cpp:62`).
- Gölge geçişi, G-buffer ve ışın izleme varyantı üretilmez (`modules/render/include/astral/render/surface_shader.hpp:40-42`); TLAS örnekleri yalnız opak listeden kurulur, yani şeffaf
  yüzeyler RT yansımalarında/GI'da yoktur (`modules/render/src/frame/instances.cpp:34-60`).

## 5. `shading_model` (yalnız `surface`)

```glsl
shading_model clearcoat;     // standard | clearcoat | sheen | anisotropic | transmission | subsurface
```

`surface` dışında bir türde yazılırsa hata (`shading_model is only supported for shader_type surface`), bilinmeyen ad hata (`modules/shader/src/ashader.cpp:255-264`).
Varsayılan `standard`. Seçilen model G-buffer'a `emissive.a` içinde, modele özgü dört skaler ise `material_ext` ekinde yazılır
(`modules/render/src/surface_shader.cpp:219-241`, [ADR-0033](../decisions/0033-advanced-pbr-and-path-tracer.md)).

| Model | Okunan çıktılar (aralık/kelepçe) | Not |
|---|---|---|
| `standard` | — | `CLEARCOAT > 1e-4` yazarsan model **kendiliğinden `clearcoat`'a yükselir** (`modules/render/src/surface_shader.cpp:229`; `WETNESS` de clearcoat ekler) |
| `clearcoat` | `CLEARCOAT` [0,1], `CLEARCOAT_ROUGHNESS` [0.02,1] | Üst dielektrik katman |
| `sheen` | `SHEEN_COLOR` (≥0), `SHEEN_ROUGHNESS` [0.05,1] | Kumaş/kadife (Charlie NDF) |
| `anisotropic` | `ANISOTROPY` [−0.95,0.95], `ANISOTROPY_ROTATION` (rad) | Teğet yön **mesh teğetinden değil**, `cross(yukarı, NORMAL)`'den türetilir ve normal etrafında döndürülür (`modules/render/shaders/include/astral/bsdf.glsl:240-245`) |
| `transmission` | `TRANSMISSION` [0,1], `IOR` (≥1.01), `THICKNESS` (≥0.001), `ATTENUATION_COLOR`, `ATTENUATION_DISTANCE` (≥0.001) | G-buffer yolunda albedo `TRANSMISSION` oranında `ATTENUATION_COLOR`'a karışır (`modules/render/src/surface_shader.cpp:234`) |
| `subsurface` | `SUBSURFACE` [0,1], `SUBSURFACE_COLOR` (≥0), `SUBSURFACE_RADIUS` | **Yarıçap yalnız ışın izlemeli ikincil vuruşlarda okunur**; birincil (G-buffer) yüzeyde sabit 0.5 (`modules/render/shaders/include/astral/primary.glsl:46-47`) |

`ATTENUATION_COLOR` birincil yüzeyde de albedo'dan yeniden kurulur (`modules/render/shaders/include/astral/primary.glsl:46`): raster yolunda Beer-Lambert rengi ayrı taşınmaz.
Model seçmeden de `WETNESS` [0,1] (albedo'yu koyulaştırır, roughness'ı düşürür, su filmi clearcoat'ı ekler; `modules/render/shaders/include/astral/bsdf.glsl:29-38`) yazılabilir.
Raster, hibrit RT, ReSTIR ve path tracer aynı `bsdf.glsl` matematiğini paylaşır ([ADR-0033](../decisions/0033-advanced-pbr-and-path-tracer.md)). Bu belgede **görsel doğruluk iddiası yok**; yalnız hangi
girdinin okunduğu kodla doğrulandı. Test: `modules/render/tests/m13_pbr_test.cpp`.

## 6. `uniform`

```glsl
uniform <tip> <ad> [: ipucu, ipucu, ...] [= varsayılan];
```

| Tip | Bileşen | Varsayılan yazımı | `.material` JSON |
|---|---|---|---|
| `float` | 1 | `= 0.35` | `0.35` |
| `vec2/vec3/vec4` | 2/3/4 | `= vec3(1, 0.2, 0.1)` veya tek sayı (tüm bileşenlere kopyalanır) | `[x,y,z]` veya tek sayı |
| `int` | 1 | `= 4` | `4` (kesirli değer reddedilir) |
| `bool` | 1 | `= true` | `true/false/1/0` |
| `sampler2D` | 1 (bindless indeks) | varsayılan **alamaz** | doku yolu (string) |

Başka tip yoktur: `mat*`, dizi, `struct`, `sampler3D`, `samplerCube`, `sampler2DArray` uniform olamaz (`modules/shader/src/ashader.cpp:77-86`).

- **Sayı doğrulaması ([ADR-0042](../decisions/0042-typed-shader-parameter-words.md)):** her bileşen tipli 32 bit kelimeye çevrilir; `float` için sonlu ve ±float aralığında, `int` için tam sayı ve int32 aralığında,
  `bool` için 0/1 olmalı; aksi hâlde varsayılan **okunamaz** (hata) (`modules/shader/src/ashader.cpp:426-444`, `modules/shader/src/ashader.cpp:112-139`).
- **Ayrılmış adlar:** tamamı BÜYÜK HARF/rakam/`_` olan adlar ve `astral_`/`gl_` ile başlayanlar yasak (`modules/shader/src/ashader.cpp:141-144`). Aynı ad iki kez bildirilemez.
- **Makro yalıtımı:** her uniform yalnız senin gövdenin çevresinde `#define ad ...` ile tanımlanır ve sonra `#undef` edilir (`modules/render/src/surface_shader.cpp:30-68`); `n`, `f`, `id`
  gibi şablon değişkenlerini uniform adı yapmak çakışmaz. Ama **kendi yardımcı fonksiyonların ve global sabitlerin** motor include'larıyla (`scene.glsl`, `color.glsl`,
  `bsdf.glsl`) aynı global alanı paylaşır; bu include'lar `astral_` dışında adlar da tanımlar (örn. `fresnel_schlick`, `fresnel_dielectric`, `PI`): aynı adı kullanma
  (çıkarım: yinelenen tanım glslang hatasıdır).
- **Parametre bloğu:** uniform'lar materyal başına 4 baytlık kelimelerde bir blokta durur; `float/int/bool/sampler2D` 1 kelime, `vec2` 2 (hizalı), `vec3` 3 (4'e hizalı,
  ardından gelen tek kelimelik uniform 4. kelimesine girebilir), `vec4` 4; toplam 4'ün katına yuvarlanır (`modules/shader/src/ashader.cpp:446-463`).

### 6.1 İpuçları

| İpucu | Geçerli | Etki | Kaynak |
|---|---|---|---|
| `source_color` | `vec3/vec4`, `sampler2D` | Debug panelinde renk seçici; dokuyu sRGB olarak yükler | `modules/shader/src/ashader.cpp:207`, `modules/app/src/panels/materials_panel.cpp:37-46` |
| `hint_normal` | `sampler2D` | Doku verilmezse düz normal `(0.5,0.5,1)` bağlanır (aksi hâlde beyaz) | `modules/render/include/astral/render/surface_shader.hpp:34` |
| `range(min, max[, step])` / `hint_range(...)` | herhangi bir sayısal tip (ayrıştırıcı denetlemez) | **Yalnız `float`** için debug panelinde kaydırıcı; diğer tiplerde sürükleme kutusu | `modules/shader/src/ashader.cpp:215-227`, `modules/app/src/panels/materials_panel.cpp:31-35` |
| `filter_linear`, `filter_linear_mipmap`, `filter_nearest`, `filter_nearest_mipmap` | `sampler2D` | Üç sabit örnekleyiciden birini seçer; **`_mipmap` eki ayrı davranış yaratmaz** (`Nearest*` → nearest, kalanlar → linear). Linear örnekleyiciler trilinear, nearest örnekleyici nearest mip modu kullanır | `modules/render/src/surface_shader.cpp:46-48`, `modules/gpu/src/device_init.cpp:375-392` |
| `repeat`, `repeat_disable` | `sampler2D` | Linear için `LINEAR_REPEAT` / `LINEAR_CLAMP`. `filter_nearest*` her zaman `NEAREST_CLAMP`'tir: **`repeat` yok sayılır** | `modules/render/src/surface_shader.cpp:46-48`, `modules/gpu/shaders/include/astral/bindless.glsl:24-26` |

Bilinmeyen ipucu hata verir. `LINEAR_REPEAT` örnekleyicisi 8× anizotropik filtreleme açar (cihaz destekliyorsa; `modules/gpu/src/device_init.cpp:382-385`).

## 7. `varying` (yalnız `surface`)

```glsl
varying float wave;      // vertex()'te yaz, surface()'ta oku
```

- `vertex` çıkışı `layout(location = 7 + i) out <tip> <ad>`, fragment girişi aynı konumda `in` (`modules/render/src/surface_shader.cpp:133-135`, `modules/render/src/surface_shader.cpp:265-267`). Konum 0-6 motorundur.
- Ayrıştırıcı tipi yazıldığı gibi saklar; kullanılabilir tipler `float vec2 vec3 vec4`. İnterpolasyon varsayılandır (`flat` yok).
- Işın izleme varyantında `vertex()` çalışmaz: varying'ler genel değişken olarak tanımlanır ve **0 değerindedir** (`modules/render/include/astral/render/surface_shader.hpp:77-81`, `modules/render/src/surface_shader.cpp:308`).
- `post/sky/particle/volume` shader'larında varying desteklenmez (particle/volume'da hata).

## 8. Yerleşik değişkenler

Tüm yerleşikler BÜYÜK HARFtir. `TIME` saniyedir.

### 8.1 `surface` — `vertex()`

İsteğe bağlı; her köşe için model→dünya dönüşümünden önce çağrılır (`modules/render/src/surface_shader.cpp:149`).

| Değişken | Tip | Erişim | Açıklama |
|---|---|---|---|
| `VERTEX` | `vec3` | R/W | Nesne uzayı konum; G-buffer, gölge ve hareket vektörleri yeni konumu kullanır (`modules/render/src/surface_shader.cpp:150-160`) |
| `NORMAL` | `vec3` | R/W | Nesne uzayı normal; sonra normal matrisiyle dünyaya taşınır |
| `UV`, `UV1` | `vec2` | R/W | Birinci/ikinci doku koordinatı; fragmana geçer |
| `WORLD_POSITION` | `vec3` | R | Deformasyon **öncesi** dünya konumu (yazmak sonucu etkilemez; dünya konumu `VERTEX`'ten hesaplanır) |
| `MODEL_MATRIX` | `mat4` | R | Model→dünya |
| `TIME` | `float` | R | |

### 8.2 `surface` — `surface()`

Girdiler (okunur): `UV`, `UV1`, `NORMAL_UV` (normal haritası koordinatı, varsayılan `UV`; yazılabilir), `NORMAL` (dünya, arka yüzde ters), `WORLD_POSITION`,
`VIEW_DIR` (yüzeyden kameraya), `FRONT_FACING`, `SCREEN_UV` (ışın izleme vuruşlarında `vec2(0)`), `TIME`, `PI` (`modules/render/shaders/include/astral/scene.glsl:91`)
(`modules/render/src/surface_shader_internal.hpp:59-66`, `modules/render/src/surface_shader.cpp:283-293`).

Çıktılar ve **varsayılanları** (`modules/render/src/surface_shader_internal.hpp:68-74`):

| Değişken | Tip | Varsayılan | Not |
|---|---|---|---|
| `ALBEDO` | `vec3` | `vec3(1)` | Çıkışta `ALBEDO * AO` |
| `ROUGHNESS` | `float` | `0.6` | [0.02, 1]'e kelepçelenir (`modules/render/shaders/include/astral/brdf.glsl:12`) |
| `METALLIC` | `float` | `0.0` | [0, 1] |
| `EMISSION` | `vec3` | `vec3(0)` | HDR; RT/GI'da gerçekten ışık verir |
| `AO` | `float` | `1.0` | Yalnız albedo'yu çarpar (ayrı ortam gölgeleme terimi yok) |
| `ALPHA` | `float` | `1.0` | `blend_*` şeffaflığı; `alpha_test` karşılaştırması. Opak G-buffer geçişinde **yok sayılır** |
| `ALPHA_SCISSOR` | `float` | `0.5` | `alpha_test` eşiği |
| `NORMAL_MAP` | `vec3` | `vec3(0.5,0.5,1)` | Teğet uzayı normal haritası değeri [0,1]; ekran türevleriyle bükülür (teğet gerekmez). Varsayılandan farklıysa uygulanır |
| `NORMAL_MAP_DEPTH` | `float` | `1.0` | Normal haritası xy çarpanı |
| `WETNESS` | `float` | `0.0` | §5 |
| `CLEARCOAT`, `CLEARCOAT_ROUGHNESS` | `float` | `0.0`, `0.1` | §5 |
| `SHEEN_COLOR`, `SHEEN_ROUGHNESS` | `vec3`, `float` | `0`, `0.5` | §5 |
| `ANISOTROPY`, `ANISOTROPY_ROTATION` | `float` | `0`, `0` | §5 |
| `TRANSMISSION`, `IOR`, `THICKNESS` | `float` | `0`, `1.5`, `0.01` | §5 |
| `ATTENUATION_COLOR`, `ATTENUATION_DISTANCE` | `vec3`, `float` | `vec3(1)`, `1.0` | §5 |
| `SUBSURFACE`, `SUBSURFACE_COLOR`, `SUBSURFACE_RADIUS` | `float`, `vec3`, `float` | `0`, `vec3(1)`, `0.5` | §5 |

Modelin okumadığı çıktıyı yazmak zararsızdır ve etkisizdir. Yardımcı fonksiyonlar **hem vertex hem fragment aşamasında** derlenir (`vertex()` fragment'tan,
`surface()` vertex'ten silinir, `modules/render/src/surface_shader.cpp:136`, `modules/render/src/surface_shader.cpp:271`): bir yardımcı yalnız fragment'ta bulunan bir yerleşiğe (`ALBEDO`, `VIEW_DIR`…) dokunursa vertex
aşamasında derleme hatası alırsın (çıkarım). Yardımcıyı yalnız bir aşamadan çağır ve o aşamanın yerleşiklerini parametre olarak ver.

### 8.3 `sky` — `sky()`

| Değişken | Tip | Erişim | Açıklama |
|---|---|---|---|
| `EYEDIR` | `vec3` | R | Kameradan piksele dünya yönü |
| `SUN_DIR` | `vec3` | R | Güneşe doğru birim vektör |
| `SUN_COLOR` | `vec3` | R | Güneşin `color * intensity` değeri (HDR) |
| `TIME` | `float` | R | |
| `COLOR` | `vec3` | W | Doğrusal HDR renk; başlangıç `vec3(0)`; negatifler 0'a kırpılır (`modules/render/src/surface_shader_rt.cpp:162-164`) |
| `IBL_CAPTURE` | `const bool` | R | Gökyüzü ışık haritasına çekilirken `true`; `RenderSettings::sky_ibl` açıkken aynı `sky()` yüzeyler/yansımalar için ortam ışığı üretir (`modules/render/src/surface_shader_rt.cpp:143`, `modules/render/src/surface_shader_rt.cpp:277`) |

Yalnız derinliği 1.0 (geometri olmayan) piksellere yazar (`modules/render/src/surface_shader_rt.cpp:150-155`; render hattı `modules/render/src/stores/shader_store.cpp:124-140`).
Bağlama: `ctx.sky = ctx.materials().load_material("materials/dusk_sky.material")` (`modules/app/include/astral/app/app.hpp:112-113`); `Atmosphere::sky_material` ve `drive_sky`
aynı alanı sürebilir (`modules/app/include/astral/app/components.hpp:52`, `modules/app/include/astral/app/components.hpp:60`).

### 8.4 `post` — `post()`

| Değişken | Tip | Erişim | Açıklama |
|---|---|---|---|
| `COLOR` | `vec3` | R/W | Başlangıçta o pikselin **ekran** rengi [0,1] (tonemap sonrası, gama kodlu); çıkışta [0,1]'e kırpılır |
| `SCREEN_UV` | `vec2` | R | `[0,1]` ekran koordinatı |
| `SCREEN_TEXTURE` | `sampler2D` | R | Önceki geçişin tam ekran dokusu, `LINEAR_CLAMP` örnekleyiciyle |
| `TIME` | `float` | R | |

`ctx.post` bir `std::vector<MaterialHandle>`'dır; materyaller sırayla (ping-pong) uygulanır, `post` olmayan türdekiler yok sayılır
(`modules/render/src/passes/post_pass.cpp:5-12`, `modules/render/src/passes/post_pass.cpp:15-43`). `COLOR` hedef biçim sRGB ise motor doğrusal→sRGB çevirir ve sonra geri çevirir, yani `post()` her durumda
ekran kodlu renk görür (`modules/render/src/surface_shader_rt.cpp:169-176`). **Gözlem (çalıştırılarak doğrulanmadı):** `SCREEN_TEXTURE` doğrudan örneklenince hedef sRGB ise
doku donanımda doğrusala çözülür; yani `texture(SCREEN_TEXTURE, uv)` ile `COLOR` aynı kodlamada olmayabilir (`modules/render/src/surface_shader_rt.cpp:142`, `modules/render/src/surface_shader_rt.cpp:169-176`).

### 8.5 `particle` ve `motion()`

İki aşama vardır ve `AGE`, `LIFETIME`, `LIFE_T` **aşamaya göre farklı anlam taşır** (`modules/render/shaders/particle.vert:18-41`, `modules/render/shaders/particle.frag:35-48`):

| Değişken | `particle()` (fragment) | `motion()` (vertex, isteğe bağlı) |
|---|---|---|
| `AGE` | **Efektin** yaşı (`position_age.w`), tüm parçacıklar için aynı | **Bu parçacığın** yaşı (rastgele gecikme düşülmüş) |
| `LIFETIME` | Efekt süresi | Efekt süresi |
| `LIFE_T` | Parçacığın `age/lifetime` oranı [0,1] (vertex'ten gelir) | Aynı oran |
| `UV` | Dörtgen üzerinde [0,1]² | — |
| `WORLD_POSITION` | Parçacığın dünya konumu | R/W: yazdığın konum çizilir |
| `COLOR` | `vec3` R/W, başlangıç efekt rengi | — |
| `ALPHA` | `float` R/W, başlangıç yumuşak kenar×solma, çıkışta [0,1] | — |
| `ORIGIN`, `WIND`, `RANDOM`, `SIZE` | — | `ORIGIN` yayıcı konumu, `WIND` sürüklenme, `RANDOM` parçacık başına [0,1), `SIZE` R/W metre (negatif 0'a) |

`particle()`'ta **`TIME` tanımlı değildir** (parçacık için `AGE` kullan). `LIFETIME`/`LIFE_T` her iki aşamada derlenir (`modules/render/tests/particle_builtins_test.cpp:8-29`);
`motion()` yoksa motorun kendi vertex aşaması kullanılır ve `LIFE_T` yine fragment'ta okunur. Çıktı `particle.frag` içinde kendi ACES tonemap'inden geçer ve tonemap
**sonrasına** çizilir (`modules/render/shaders/particle.frag:65-67`, `modules/render/src/paths/raster_path.cpp:117`); varsayılan karışım toplamsaldır.

### 8.6 `volume`

`volume()` her dolu hacim hücresi için bir compute çağrısıdır (`modules/render/src/surface_shader_rt.cpp:22-66`):

| Değişken | Tip | Erişim | Açıklama |
|---|---|---|---|
| `UVW` | `vec3` | R | Hacim içi [0,1]³ koordinat |
| `WORLD_POSITION` | `vec3` | R | Hücre merkezi |
| `TIME`, `AGE` | `float` | R | Saniye; efekt yaşı |
| `TINT` | `vec3` | W | Başlangıç `vec3(1)`; [0,8]'e kırpılır |
| `DENSITY` | `float` | W | Başlangıç `1.0`; [0,1] |
| `EMISSION` | `vec3` | W | Başlangıç `vec3(0)`; [0,64] |

`.vfx` içinde bir izin `material` alanı ile bağlanır; `volume` izi için `material` zorunludur (`modules/app/src/services/vfx_runtime.cpp:83-87`).

## 9. Raster ve ışın izleme farkları

Opak `surface` shader'ın `surface()` gövdesinden bir **RT parçası** da üretilir; tüm canlı özel yüzeyler tek bir `astral/rt_custom.glsl`'de birleştirilir ve beş RT compute shader'ı
(`rt_lighting`, `ddgi_trace`, ReSTIR temporal/spatial, `path_tracer`) bunlarla yeniden derlenir (`modules/render/src/surface_shader_rt.cpp:206-253`). RT vuruşunda:

- `vertex()` çalışmaz (deformasyon yok; BLAS özgün köşelerle bir kez kurulur), varying'ler 0.
- `dFdx/dFdy` 0 döner; `texture(s, uv)` sabit kaba mip (`textureLod(s, uv, 2.0)`) ile örneklenir; `SCREEN_UV = vec2(0)`.
- `NORMAL_MAP` ve (opak) `ALPHA` yok sayılır; `alpha_test` için ayrı eşik işlevi çalışır (`modules/render/src/surface_shader.cpp:303-355`).
- Her shader'ın üst düzey fonksiyon adları `astral_s<ID>_<ad>` makrolarıyla ad alanına alınır; **fonksiyon dışı global sabitler/değişkenler alınmaz**: iki `surface`
  shader'ında aynı adlı global sabit birleşik RT derlemesini bozar (`modules/render/src/surface_shader.cpp:305-310`). Sabitleri fonksiyon içinde tanımla.
- Birleşik RT derlemesi başarısız olursa (örn. yukarıdaki çakışma) motor önceden derlenmiş varsayılan RT shader'larına döner; raster aynen sürer, hata
  `MaterialService::shader_error()` ve Materials panelinde görünür (`modules/render/src/stores/shader_store.cpp:181-209`, `modules/app/include/astral/app/material_service.hpp:29`).
  Shader kümesi her değiştiğinde (oluşturma/hot reload) beş compute shader yeniden derlenir; kodun kendi yorumu bunları "motorun en yavaş derlemeleri" diye anar
  (`modules/render/src/surface_shader_rt.cpp:242`). Ölçülmedi.

## 10. Hata ve hot reload

- Hata günlüğü: `shader <yol>\n  <yol>:<satır>: <mesaj>`; derlenemeyen sürümün yerine **son çalışan** sürüm kalır, materyaller eski handle ile çizilmeye devam eder
  (`modules/app/src/material_library.cpp:117-160`).
- `MaterialLibrary::update` her **0,25 sn**'de dosya zamanlarına bakar: değişen `.ashader` yeniden derlenir, ona bağlı materyaller yeniden bağlanır; her materyalin tüm
  ebeveyn zinciri izlenir (`modules/app/src/material_library.cpp:262-300`).
- Derleme SPIR-V disk önbelleğinden de cevap verebilir (`ASTRAL_SHADER_CACHE`, bkz. [writing-a-game.md](writing-a-game.md#7-paketleme)).
- Hata ayıklama: Debug UI (F1) → **Materials** paneli her yüklü materyalin uniform'larını döker; değişiklik **yalnız bu oturum** içindir, dosyaya yazılmaz
  (`modules/app/src/panels/materials_panel.cpp:78-90`).

## 11. Neler yok

| İstek | Durum |
|---|---|
| `DEPTH_TEXTURE` / derinlik okuma | **Yok** (`modules/` içinde geçmez). `post()` derinliği göremez; şeffaf yüzeyler arkadaki sahneyi örnekleyemez |
| Ekran uzayı kırılma, `SCREEN_TEXTURE` şeffaf yüzeyde | Yok (yalnız `post`) |
| `post` HDR (tonemap öncesi) | Yok; `post` her zaman tonemap **sonrası** (`modules/render/src/paths/raster_path.cpp:116-118`) |
| `shader_type ui` | Ayrıştırılır, yüklenmez |
| `sampler3D`, `samplerCube`, `sampler2DArray`, `mat*`, dizi/struct uniform | Yok |
| Örnek başına uniform ezme | Yok: `MaterialOverride` bileşeni kodda bulunmuyor; yalnız glTF için `ModelRef::material_overrides` (slot adıyla `.material`) (`modules/assets/include/astral/assets/asset_manager.hpp:122-131`) |
| `rt_simple`, `particle`/`volume` `unshaded` | Kabul edilir, etkisiz |
| Şeffaf yüzey gölgesi/RT'de görünmesi | Yok (§4) |
| SPIR-V'nin pakete gömülmesi | Yok; çalışma zamanında derlenir (disk önbelleğiyle) |
| Kullanıcı `#ifdef` anahtarları | Tasarım gereği sunulmaz ([ADR-0022](../decisions/0022-shader-and-material-language.md)); GLSL ön işlemcisi teknik olarak kapalı mı doğrulanmadı |
| Her yüzey için SSS yarıçapı/teğet yönlü anizotropi | §5'teki kısıtlar |

## Bilinen sınırlar

- `rt_simple` ve bazı `render_mode`'lar hata vermeden etkisizdir; yazım hatası yakalanır ama "işe yaramayan geçerli ad" yakalanmaz (`modules/shader/src/ashader.cpp:372-383`).
- `SUBSURFACE_RADIUS` ve `ATTENUATION_COLOR` birincil yüzeyde tam taşınmaz (§5); anizotropi yönü mesh teğetini izlemez.
- Şeffaf yüzey sıralaması nesne merkezine göredir; birbirine geçen büyük nesnelerde hatalı sıra olabilir (`modules/render/src/frame/instances.cpp:65-74`).
- RT varyantında vertex deformasyonu, normal haritası ve türevler yok; yansımada raster'dan farklı görünüm normaldir (§9).
- Tek özel shader'ın RT derlemesi bozarsa **tüm** özel yüzeyler RT'de varsayılana düşer (`modules/render/src/stores/shader_store.cpp:181-209`).
- Yardımcı fonksiyon/global adları motor include'larıyla çakışabilir; çakışma uyarısı yok, derleme hatası alırsın (çıkarım).
- `post` içinde `SCREEN_TEXTURE` ile `COLOR` kodlama farkı (§8.4) çalıştırılarak doğrulanmadı.
- Doku projeksiyonu (`MeshUV/Planar*/Triplanar`) yalnız standart PBR (`type: pbr`) materyallerinde vardır, `.ashader` yüzeylerine otomatik uygulanmaz
  ([shaders-and-materials.md](shaders-and-materials.md)).
