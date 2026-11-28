# ADR-0055 — Material/shader sertleştirme, shader'da texture, PBR tamamlama, katman ve maske harmanı

- **Durum:** Önerildi (uygulama dilim dilim; her dilim bu dosyada işaretlenir)
- **Tarih:** 2026-11-27
- **İlişkili:** [ADR-0022](0022-shader-and-material-language.md), [ADR-0033](0033-advanced-pbr-and-path-tracer.md), [ADR-0042](0042-typed-shader-parameter-words.md), [ADR-0051](0051-projected-texture-orientation.md)

## Bağlam

Material/shader incelemesinde (2026-11-27, master cdf2188) şunlar bulundu:

1. Kaplama materyal başına; entity başına değer değiştirilemez (`MaterialOverride` ADR-0022'de ertelendi).
2. `MaterialStore::upload` her karede tüm tabloyu yeniden kurar.
3. Özel `.ashader` materyallerinde `mapping`/projeksiyon yok; `.material` ebeveyn zinciri yalnız üst düzey adları birleştirir.
4. Özel shader yolunda bilinmeyen uniform uyarı, PBR yolunda hata (tutarsız).
5. RT/path tracer'da `surface()` farklı: `texture()` sabit mip, `dFdx/dFdy` 0, varying yok.
6. `vec` dizisinde bool, `#define` sonrası başlık ifadesi gibi küçük ayrıştırıcı/doğrulama boşlukları.
7. `.ashader` içinde texture yalnız `.material` üzerinden bağlanır; shader dosyası kendi varsayılan dokusunu taşıyamaz.
8. Standart PBR: oklüzyon dokusu, alpha modu (mask/blend), emissive strength, F0/specular ayarı yok (glTF içe aktarma bunları üretilen `.ashader` ile çözüyor, elle yazılan PBR çözemiyor).
9. Materyal katmanı ve maske harmanı yok.

## Karar

**Tek branch (`claude/material-shader-hardening`), hotfix raporu olarak yayınlanır.** Dilimler bağımsız commit'lerdir; her biri önce başarısız testle başlar.

### D1. Sağlamlık (dilim 1) — **uygulandı** (önişlemci satırı + vec içinde bool; PBR'da bilinmeyen parametre uyarıya çekildi)
- `vec` dizisinde bool bileşenleri 0/1 olarak kabul edilir.
- Özel shader materyalinde bilinmeyen uniform adı da **hata** değil, ama PBR ile aynı sınıfta raporlanır: ikisi de `MaterialFile::warnings` benzeri tek kanaldan, aynı iletiyle (`unknown parameter`). Karar: ikisi de **uyarı + materyal kurulur**, çünkü hot reload sırasında yazım hatası çalışan sahneyi öldürmemeli. PBR'daki hata davranışı uyarıya çekilir.
- Ayrıştırıcı: önişlemci satırı (`#define`, `#include`) başlık ifadesini bozmaz.
- `ui` türü ve diğer tüketicisiz türler dokümanda "tüketicisiz" diye kalır; ADR-0022 durumu güncellenir (particle/volume derlenir).

### D2. Ebeveyn zincirinde derin birleştirme (dilim 2) — **uygulandı**
Çocuk `mapping`/`clearcoat`/`detail` gibi nesne yazarsa ebeveynin nesnesi alan alan birleşir (çocuk kazanır). Diziler ve skalerler değişmez. Sınır: `resolve_material` içinde, JSON'a özgü; render'a sızmaz.

### D3. Shader'da texture (dilim 3) — **uygulandı**
- `.ashader` başlığında `sampler2D` uniform'u varsayılan yol alır: `uniform sampler2D albedo : source_color, repeat = "textures/leaf.png";`.
- Varsayılan yol `ShaderUniform::default_texture` (string); `.material` değeri yazmazsa kullanılır, boş string "doku yok" olarak geçersiz kılar.
- Yol, shader dosyasından değil **asset kökünden** çözülür (mevcut `.material` kuralıyla aynı).
- `.ashader` dosyası doğrudan materyal olarak kullanılabilir (`MaterialLibrary::get("x.ashader")` → tüm değerler varsayılan).
- Cook düz görselleri materyal kullanımına göre sınıflamadığı için ek cook işi yoktur; yalnız `.ashader` ayrıştırması (hatalı varsayılan sözdizimi) cook'ta `dosya:satır` ile raporlanır.

### D4. Standart PBR tamamlama (dilim 4)
- **`occlusion_texture` + `occlusion_strength` — uygulandı.** GPU tablosundaki boş yuvalar kullanıldı (`detail_textures[3]`, `detail_params.w`); düzen değişmedi. Raster ve RT aynı çarpanı uygular.
- **`emissive_strength` eklenmeyecek:** `MaterialDesc::emissive` zaten sınırsız HDR ışıma (rgb); glTF'nin `emissiveStrength` çarpanı doğrudan buna katlanır. İkinci bir alan yalnız kafa karıştırır.
- **`ior` (dielektrik F0) ertelendi:** F0 şu an 0.04 sabit ve G-buffer'da boş kanal yok; eklemek G-buffer yeniden düzeni ister (ayrı ADR). `Transmission.ior` kırılma için çalışmaya devam eder.
- **`alpha_mode` (`Mask`/`Blend`) için karar:** standart PBR sabit G-buffer pipeline'ını kullanır; `Mask` için cutoff'u G-buffer'a ve gölge/RT any-hit yoluna işlemek gerekir, `Blend` ise standart yolda forward pass'i yoktur. glTF içe aktarma bu ihtiyacı üretilen `.ashader` ile zaten karşılar. Karar: `type: pbr` + `alpha_mode` verilirse materyal kütüphanesi aynı üretici ile bir shader'a yönlendirilir (dilim 4b, ayrı commit).

### D5. Örnek başına override (dilim 5)
`MaterialOverride` ECS bileşeni (reflect edilmiş): uniform adı → değer. Değerler paylaşılan parametre tamponuna **örnek başına ofsetle** yazılır; `Instance` yapısına `param_offset` eklenir. Materyal sayısı patlamaz, hot reload bozulmaz.

### D6. Performans (dilim 6)
Önce `bench` ile ölç; `MaterialStore::upload` için sürüm sayacıyla kirli takibi (yalnız değişen satırlar). SPIR-V optimizasyonu ölçülür, kazanç yoksa kapalı kalır ve "Bilinen sınırlar"a yazılır.

### D7. Materyal katmanları ve maske harmanı (dilim 7–8)
İki parça, ikisi de **metin kaynağı tek doğruluk** ilkesine bağlı (ADR-0022 §5):

1. **Yardımcı kütüphane `astral/layers.glsl`** (her yüzey shader'ına otomatik eklenir): `AstralLayer` yapısı (albedo, roughness, metallic, normal, emission, ao, alpha) ve `astral_layer_mix(a, b, mask)`, `astral_layer_height_blend(a, ha, b, hb, mask, contrast)`, maske yardımcıları: `astral_mask_slope`, `astral_mask_height_world`, `astral_mask_noise`. Her `.ashader` bunu doğrudan kullanabilir.
2. **`type: layered` materyal dosyası:**
   ```json
   { "format": "astral.material", "version": 1, "type": "layered",
     "layers": [
       { "material": "materials/rock.material" },
       { "material": "materials/moss.material",
         "mask": { "texture": "textures/moss_mask.png", "channel": "r", "contrast": 0.6, "height_blend": true } } ] }
   ```
   Motor bu dosyadan **üretilen bir `.ashader`** yazar (glTF içe aktarmanın kullandığı yolla aynı). Her katmanın `surface()` gövdesi önek adıyla fonksiyona çevrilir (RT parçasında kullanılan yeniden adlandırma tekniği), çıktıları `AstralLayer` olarak toplanır ve sırayla harmanlanır. Üst sınır 8 katman (RT uber-shader ve parametre bloğu boyutu için). Katman uniform'ları `layerN_<ad>` önekiyle tek parametre bloğunda birleşir; katman başına parametre geçersiz kılma `.material` `values`'ında aynı önekle yapılır.
3. Maske kaynakları v1: doku kanalı, skaler parametre, eğim, dünya yüksekliği, gürültü, önceki katmanın yüksekliği (height blend). **Vertex color v1'de yok** (mesh formatı değişikliği gerektirir; ayrı ADR).

### D8. RT/raster parite (dilim 9, ölçüm öncelikli)
Önce raster ve RT çıktısını aynı materyalle karşılaştıran altın test (ADR-0031 donanımsız yol + varsa GPU). Sonra: RT'de `texture()` için ışın konisi tabanlı mip seçimi, varying'lerin RT'de temsili değerle (UV'den) doldurulması. Kapanamayan fark "Bilinen sınırlar"a yazılır ve ADR-0022'nin "aynı görünür" vaadi buna göre düzeltilir.

## Sonuçlar

- Yeni public alanlar (`MaterialDesc`, `ShaderUniform::default_texture`, `MaterialOverride`) public header kuralına uyar (üçüncü parti tip yok).
- `astral-cook` sürümü artar (üretilen GLSL/varsayılan doku bağımlılıkları değişir).
- Katman sistemi RT shader'ını büyütür; `rt_simple` ve katman üst sınırı bu riski sınırlar.
- Her dilim sonunda `docs/modules/{shader,render,assets,app}.md`, `docs/guides/{ashader-language,shaders-and-materials}.md` güncellenir (docs/process.md "Faz bitti").
