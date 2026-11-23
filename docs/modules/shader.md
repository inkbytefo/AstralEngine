# Astral::Shader — `.ashader` dili

Katman 1 (yalnız Core'a bağlı). Karar: [ADR-0022](../decisions/0022-shader-and-material-language.md).
GPU'yu bilmez; metni ayrıştırır ve GLSL'i SPIR-V'ye çevirir. Şablonlardan shader üretimi `Astral::Render`
içindedir ([render.md](render.md) → "Özel shader'lar"). Tam dil referansı ve örnekler:
[guides/ashader-language.md](../guides/ashader-language.md).

## `.ashader` nedir

Küçük bir başlık + düz GLSL gövdesi. Başlık Godot'nun `.gdshader`'ına benzer; gövdeyi yapay zekâ modelleri
ve insanlar zaten bilir, bu yüzden yeni bir dil ayrıştırıcısı yazılmadı.

```glsl
shader_type surface;                  // surface | post | sky  (particle, volume, ui: ileride)
render_mode alpha_test, cull_disabled;

uniform vec4 tint : source_color = vec4(1.0);
uniform float sway : range(0.0, 1.0, 0.01) = 0.3;
uniform sampler2D albedo : source_color, filter_linear_mipmap, repeat;
varying float height;

void vertex()  { VERTEX.y += sin(TIME) * sway; height = VERTEX.z; }
void surface() { ALBEDO = tint.rgb; ALPHA = 1.0; }
```

### Başlık

| Söz dizimi | Anlam |
|---|---|
| `shader_type <tür>;` | Zorunlu. `surface` (mesh yüzeyi), `post` (ekran efekti), `sky` (arka plan) |
| `shading_model <model>;` | Yalnız `surface` için isteğe bağlı (M13 / ADR-0033): `standard` (varsayılan), `clearcoat`, `sheen`, `anisotropic`, `transmission`, `subsurface` |
| `render_mode a, b;` | Türe göre: surface → `alpha_test`, `cull_disabled`, `cull_front`, `unshaded`, `blend_mix`, `blend_add`, `shadows_disabled`, `rt_simple`; post → `after_tonemap` |
| `uniform <tip> ad [: ipuçları] [= varsayılan];` | Materyalde değeri verilen parametre. Tipler: `float vec2 vec3 vec4 int bool sampler2D` |
| `varying <tip> ad;` | `vertex()`'ten `surface()`'e geçen değer |

İpuçları: `source_color` (renk seçici / sRGB doku), `hint_normal` (normal haritası), `range(min, max[, adım])`
(kaydırıcı), `filter_linear`, `filter_linear_mipmap`, `filter_nearest`, `filter_nearest_mipmap`, `repeat`,
`repeat_disable`. Bu ipuçları F1 → Materials panelini kendiliğinden üretir.

### Built-in'ler (BÜYÜK HARF)

| Tür | Okunur | Yazılır |
|---|---|---|
| `surface` `vertex()` | `UV`, `WORLD_POSITION`, `MODEL_MATRIX`, `TIME` | `VERTEX`, `NORMAL` (obje uzayı) |
| `surface` `surface()` | `UV`, `NORMAL`, `WORLD_POSITION`, `VIEW_DIR`, `FRONT_FACING`, `SCREEN_UV`, `TIME` | `ALBEDO`, `ROUGHNESS`, `METALLIC`, `EMISSION`, `ALPHA`, `ALPHA_SCISSOR`, `NORMAL_MAP`, `NORMAL_MAP_DEPTH`, `AO`, `CLEARCOAT`, `CLEARCOAT_ROUGHNESS`, `SHEEN_COLOR`, `SHEEN_ROUGHNESS`, `ANISOTROPY`, `ANISOTROPY_ROTATION`, `TRANSMISSION`, `IOR`, `THICKNESS`, `ATTENUATION_COLOR`, `ATTENUATION_DISTANCE`, `SUBSURFACE`, `SUBSURFACE_COLOR`, `SUBSURFACE_RADIUS`, `WETNESS` |
| `post` `post()` | `SCREEN_UV`, `SCREEN_TEXTURE`, `TIME` | `COLOR` (ekranda görünen renk) |
| `sky` `sky()` | `EYEDIR`, `SUN_DIR`, `SUN_COLOR`, `TIME` | `COLOR` (doğrusal HDR) |

Kurallar: uniform adları BÜYÜK HARFLE yazılamaz, `astral_` ve `gl_` ile başlayamaz (motorun adları).
Uniform'lar yalnızca kullanıcı kodunun çevresinde tanımlıdır; `scale`, `n`, `f` gibi adlar motor şablonuyla
çakışmaz. `#ifdef` anahtarı yok (varyant patlamasını önlemek için bilinçli).

## API

| Başlık | Ne |
|---|---|
| `ashader.hpp` | `parse_ashader(metin)` → `ShaderParse{source, errors[{line, message}]}`; `ShaderSource` (tür, render modları, uniform'lar, varying'ler, fonksiyonlar, gövde); `layout_uniforms` (std430 parametre bloğu düzeni); `known_render_modes` |
| `compile.hpp` | `compile_glsl(kaynak, aşama, include'lar, ad)` → SPIR-V + `#line`'a göre eşlenmiş hatalar. glslang 15.4.0, Vulkan 1.3 / SPIR-V 1.5. İş parçacığı güvenli |

Gövde, başlık satırları boşluğa çevrilmiş hâliyle aynen saklanır: üretilen GLSL `#line 1` ile başlar, derleyici
hataları doğrudan `.ashader` satır numarasını gösterir.

## Bilinen sınırlar

- `particle`, `volume`, `ui` türleri ayrıştırılır ama derlenmez (M14, M8 UI ile).
- Post: yalnız `after_tonemap` (HDR öncesi nokta yok).
- glslang çalışma anında derler (yüklemede ~yüz milisaniyeler); önceden derlenmiş SPIR-V önbelleği henüz yok,
  cook yalnızca derlenebildiğini doğrular.
- `texture()` yalnızca `sampler2D` uniform'larıyla; `sampler2DArray`, cube, 3D doku yok.
- Fonksiyon dışı genel değişkenler (`const float k = 1.0;`) iki shader'da aynı adla tanımlanırsa ışın izleme
  derlemesi başarısız olur (raster etkilenmez; `Renderer::shader_error()` nedenini söyler).
