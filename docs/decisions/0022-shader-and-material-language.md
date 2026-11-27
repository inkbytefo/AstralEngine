# ADR-0022 — Shader dili ve materyaller: `.ashader` (gdshader benzeri) + `.material`

- **Durum:** Kabul; M10'da uygulandı (aşağıdaki "M10'da uygulanan" bölümüne bak)
- **Tarih:** 2026-11-21
- **Bağlam:** Godot `.gdshader`, Unreal materyal sistemi, ADR-0008 (bindless + GLSL), ADR-0011 (hibrit RT)

## Sorun

Bugün materyal = sabit PBR parametreleri. Su, dalgalanan bayrak, parlayan rün, akan lav, eriyen buz,
hologram gibi şeyler motor koduna dokunmadan yazılamıyor. Ayrıca bir yüzey iki yerden görünür: raster
G-buffer ve ray-traced yansıma/GI. Elle yazılmış her shader iki yolda da tutarlı olmalı.

## Karar: kısa, Godot'ya benzeyen bir dil; motor gerisini üretir

### 1. Dil: GLSL gövdesi + küçük bir başlık

```glsl
shader_type surface;                  // surface | particle | volume | post | sky | ui
render_mode alpha_test, cull_disabled;

uniform vec4 tint : source_color = vec4(1.0);
uniform sampler2D albedo : source_color, filter_linear_mipmap, repeat;
uniform float sway : range(0.0, 1.0, 0.01) = 0.3;

void vertex() {
    VERTEX.x += sin(TIME * 2.0 + WORLD_POSITION.z) * sway * UV.y;   // rüzgârda sallanma
}

void surface() {
    vec4 c = texture(albedo, UV) * tint;
    ALBEDO = c.rgb;
    ALPHA = c.a;
    ROUGHNESS = 0.6;
}
```

- Gövde düz GLSL (yapay zekâ modelleri ve insanlar zaten biliyor). Yeni bir dil ayrıştırıcısı yazmıyoruz:
  yalnızca başlık (`shader_type`, `render_mode`, `uniform` + ipuçları, `varying`) kendi küçük
  ayrıştırıcımızla okunur; gövde, motorun ürettiği şablona yerleştirilip glslang ile derlenir.
- Built-in'ler büyük harf (Godot gibi): `VERTEX`, `NORMAL`, `UV`, `TIME`, `WORLD_POSITION`, `ALBEDO`,
  `NORMAL_MAP`, `ROUGHNESS`, `METALLIC`, `EMISSION`, `ALPHA`, `AO`... Hata mesajları `.ashader` satırına
  eşlenir.
- `uniform` ipuçları (`range`, `source_color`, `hint_normal`, `filter_*`, `repeat`) materyal
  panelini kendiliğinden üretir: kaydırıcı, renk seçici, texture kutusu.

### 2. Bir shader, birden çok derleme (kullanıcı görmez)

| `shader_type` | Motorun ürettiği |
|---|---|
| `surface` | G-buffer pass'i, derinlik/gölge pass'i, **RT vuruş varyantı** (materyal id'ye göre dallanan uber-shader'a eklenir), şeffaf/forward pass'i (`render_mode blend_*` ise) |
| `particle` | güncelleme (compute) + çizim (ADR-0024) |
| `volume` | yoğunluk/renk/ışıma fonksiyonu; hacimsel raymarch'a bağlanır (duman, ateş, gaz, sis — ADR-0024) |
| `post` | tam ekran pass; render graph'ta adlandırılmış noktalara takılır (`before_tonemap`, `after_tonemap`) |
| `sky` | gökyüzü/ortam fonksiyonu; IBL ve DDGI'a beslenir |
| `ui` | 2D katman (ADR-0019) için özel çizim |

Böylece aynı materyal raster'da, RT yansımada ve GI'da aynı görünür; biri unutulamaz.

### 3. `.material` = shader + değerler (+ ebeveyn)

```json
{ "shader": "shaders/foliage.ashader",
  "parent": "materials/foliage_base.material",
  "values": { "tint": [0.6, 0.9, 0.5, 1], "albedo": "textures/leaf.png", "sway": 0.45 } }
```

- Ebeveyn zinciri = Unreal'ın material instance'ı: yalnızca farklar yazılır. Parametre değiştirmek yeni
  pipeline gerektirmez (materyal tablosundaki satır güncellenir, bindless texture indeksleri).
- glTF materyalleri yerleşik `standard.ashader`'ın örnekleri olarak yüklenir; mevcut sahneler bozulmaz.
- Reflect edilmiş `MaterialOverride` component'i sahnede örnek başına değer değiştirir (instance data).

### 4. Çalışma döngüsü ve dağıtım

- Shader hot-reload: dosya kaydedilince arka planda derlenir; hata varsa eski sürüm çalışmaya devam eder,
  hata debug UI'da satır numarasıyla görünür.
- Cook tüm varyantları önceden SPIR-V'ye derler; diskte pipeline önbelleği. Oyunda derleyici yok.
- Varyant patlaması kontrollü: `render_mode` + motorun yol sayısı (raster/RT/gölge/forward) ile sınırlı;
  kullanıcı `#ifdef` anahtarları tanımlayamaz (bilinçli).

### 5. İleride: görsel düğüm grafiği

Düğüm editörü (Unreal/Godot visual shader) bir **`.ashader` üreticisi** olacak, ayrı bir sistem değil.
Metin her zaman tek doğruluk kaynağı: diff'lenir, yapay zekâ yazar, insan okur.

## Sonuçlar

- Su (M16), bitki/rüzgâr, arazi katmanları (M15), VFX (M14) aynı mekanizmayla yazılır.
- Riskler: RT uber-shader'ın büyümesi (çok sayıda `surface` shader) → materyal sayısı sınırı ve
  "RT'de basitleştirilmiş varyant" seçeneği (`render_mode rt_simple`).

## M10'da uygulanan (sapmalar ve sınırlar)

- Türler: `surface`, `post`, `sky`, `particle`, `volume` çalışıyor (güncel: ADR-0055 incelemesi); `ui` ayrıştırılır, derlenmez.
- `post` yalnız `after_tonemap`; `before_tonemap` (HDR) şimdilik yok.
- Şeffaf: ayrı ileri geçiş, tonemap sonrası, kendi aydınlatması; gölge ve RT'ye girmez. "Ekran uzayı kırılma" ve
  "RT yansımada şeffaflar" ertelendi.
- RT: uber-shader dalları yerine her özel `surface()` ayrı fonksiyon olarak RT compute shader'larına eklenir
  ve çalışma anında yeniden derlenir. Derleme hatasında hazır RT shader'ları + `shader_error()`.
- glTF materyalleri `standard.ashader` örneği olmadı (standart yol yerleşik kaldı); `MaterialOverride` ertelendi.
- Cook: `.ashader` derlenebildiği doğrulanır (hatalı shader paketlemeyi durdurur), `.material` ikili JSON'a
  çevrilir. Önceden derlenmiş SPIR-V ve disk pipeline önbelleği ertelendi: oyun şimdilik derleyiciyi
  (glslang) taşır.
- Karar: uniform'lar GLSL makrosu olarak yalnızca kullanıcı kodunun çevresinde tanımlanır; motorun kendi
  değişkenleriyle ad çakışması imkânsız.
