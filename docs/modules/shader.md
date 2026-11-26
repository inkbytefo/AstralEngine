# Shader (`Astral::Shader`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core (public); glslang 15.4.0 (private, `modules/shader/CMakeLists.txt:13`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

`.ashader` dilinin ayrıştırıcısı ve GLSL → SPIR-V derleyici sarmalayıcısı. GPU'yu, Vulkan'ı ve Renderer'ı bilmez: metni
`ShaderSource`'a çevirir, parametre bloğu düzenini hesaplar, glslang ile derler ve sonucu içerik adresli bir diskte önbelleğe
alabilir. Şablonlardan (G-buffer, gölge, ışın izleme parçası) üretilen GLSL'in kendisi `Astral::Render` içindedir
(`modules/render/src/surface_shader.cpp`); bu modül yalnızca ayrıştırma + derleme + önbellek katmanıdır. Yapmadığı şeyler:
SPIR-V optimizasyonu, SPIR-V yansıtma (reflection), `#include` çözümü dışında önişlemci desteği, shader grafı.

## Public API

Header'lar `modules/shader/include/astral/shader/` altındadır.

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `parse_ashader(text)` → `ShaderParse{source, errors}` | `.ashader` metnini ayrıştırır; hiç fırlatmaz, her sorun satırıyla raporlanır | `modules/shader/include/astral/shader/ashader.hpp:80` |
| `ShaderSource`, `ShaderType`, `ShadingModel`, `ShaderUniform`, `UniformHints`, `ShaderVarying` | Ayrıştırılmış shader: tür, render modları, uniform'lar, varying'ler, fonksiyon adları/aralıkları, gövde | `modules/shader/include/astral/shader/ashader.hpp:50` |
| `ShaderSource::body_without(fn)`, `has_render_mode`, `has_function` | Bir fonksiyonu boşlayarak vertex/fragment aşamalarına gövde ayırma (satırlar korunur) | `modules/shader/include/astral/shader/ashader.hpp:63` |
| `known_render_modes(type)`, `known_shading_models()`, `to_string(...)` | Doğrulama/araç yardımcıları | `modules/shader/include/astral/shader/ashader.hpp:83` |
| `layout_uniforms(source)` → `UniformLayout` | Parametre bloğu (std430, 4 baytlık sözcükler) ofsetleri | `modules/shader/include/astral/shader/ashader.hpp:95` |
| `uniform_word(type, value)` | Bir bileşeni parametre sözcüğüne çevirir veya `nullopt` (ADR-0042) | `modules/shader/include/astral/shader/ashader.hpp:100` |
| `compile_glsl(source, stage, includes, name, services)` → `GlslCompile` | GLSL → SPIR-V; hatalar `#line` ile `.ashader` satırına eşlenir; thread-safe | `modules/shader/include/astral/shader/compile.hpp:46` |
| `compile_glsl_batch(requests, includes, services)` | Bağımsız kaynakları (ortak include kümesiyle) sırayla ya da `JobSystem`'de eşzamanlı derler | `modules/shader/include/astral/shader/compile.hpp:58` |
| `ShaderCompileServices{cache, jobs}` | İsteğe bağlı, ödünç alınan servisler; global varsayılan yok | `modules/shader/include/astral/shader/compile.hpp:33` |
| `SpirvCache` (`lookup`, `store`, `prune`, `stats`, `path_for`), `SpirvCacheKey` | SHA-256 anahtarlı diskte SPIR-V önbelleği | `modules/shader/include/astral/shader/spirv_cache.hpp:28` |

## Nasıl çalışır

**Dil.** `.ashader` küçük bir başlık + düz GLSL gövdesidir. Ayrıştırıcı önce `//` ve `/* */` yorumlarını boşluğa çevirir
(satır sayısı korunur) ve `\r`'ları atar, böylece CRLF dosyalar aynı ayrışır (`modules/shader/src/ashader.cpp:26`,
`modules/shader/tests/ashader_crlf_test.cpp`). Sonra üst düzeyde `;` veya `{` ile biten "ifadeleri" tarar
(`modules/shader/src/ashader.cpp:288`). Başlık ifadeleri:

| İfade | Anlam | Kaynak |
|---|---|---|
| `shader_type <tür>;` | Zorunlu. `surface`, `particle`, `volume`, `post`, `sky`, `ui` | `modules/shader/src/ashader.cpp:246` |
| `shading_model <model>;` | Yalnız `surface`: `standard` (varsayılan), `clearcoat`, `sheen`, `anisotropic`, `transmission`, `subsurface` | `modules/shader/src/ashader.cpp:255` |
| `render_mode a, b;` | Türe göre doğrulanır: surface → `alpha_test`, `cull_disabled`, `cull_front`, `unshaded`, `blend_mix`, `blend_add`, `shadows_disabled`, `rt_simple`; particle → `blend_mix`, `blend_add`, `unshaded`; volume → `unshaded`; post → `after_tonemap`; ui → `blend_mix`, `blend_add`; sky → yok | `modules/shader/src/ashader.cpp:372` |
| `uniform <tip> ad [: ipuçları] [= varsayılan];` | Tipler `float vec2 vec3 vec4 int bool sampler2D`; ipuçları `source_color`, `hint_normal`, `range(min,max[,adım])` (`hint_range` de), `filter_linear`, `filter_linear_mipmap`, `filter_nearest`, `filter_nearest_mipmap`, `repeat`, `repeat_disable` | `modules/shader/src/ashader.cpp:176` |
| `varying <tip> ad;` | `vertex()`'ten `surface()`'e değer; tip yazıldığı gibi saklanır | `modules/shader/src/ashader.cpp:273` |

Başlık ifadeleri gövdede boşlukla silinir (yeni satırlar kalır), böylece üretilen GLSL `#line 1` ile başlayıp derleyici
hatalarını doğrudan `.ashader` satırında gösterir (`modules/shader/src/ashader.cpp:281`). Fonksiyonlar (`{...}` blokları) adları ve
`[başlangıç, bitiş)` aralıklarıyla kaydedilir (`modules/shader/src/ashader.cpp:313`); `body_without` bir fonksiyonu boşlar
(`modules/shader/src/ashader.cpp:351`). Uniform adı büyük harfli (tamamı büyük harf/rakam/`_`) ise veya `astral_`/`gl_` ile
başlıyorsa reddedilir (yerleşik değişkenler büyük harftir) (`modules/shader/src/ashader.cpp:141`). Varsayılan değerler `strtod` ile
double okunur ve her bileşen `uniform_word`'den geçer; geçmeyen varsayılan ayrıştırma hatasıdır
(`modules/shader/src/ashader.cpp:99`, `modules/shader/src/ashader.cpp:113`). `.ashader` yerleşik değişken listesi (ALBEDO, ROUGHNESS,
…) bu modülde tanımlı değildir; yerleşiklerin GLSL karşılıkları Render şablonlarındadır, dolayısıyla tam referans
[render.md](render.md) ve dil rehberindedir (burada doğrulanmadı).

**Parametre sözcükleri (ADR-0042).** GPU'daki parametre bloğu 32 bitlik sözcüklerdir. `uniform_word`: `float/vecN` → IEEE
bitleri (±float max dışı ve sonsuz/NaN reddedilir), `int` → iki'nin tümleyeni (tam sayı ve int32 aralığında olmalı), `bool` → 0/1,
`sampler2D` → yok (bindless indeks başka yerde atanır) (`modules/shader/src/ashader.cpp:426`). Düzen: sözcük başına hizalama,
`vec2` 2'ye, `vec3`/`vec4` 4'e hizalanır; `vec3` 3 sözcük kaplar, sonraki skaler onun dördüncü sözcüğüne paketlenebilir; toplam 4 sözcüğe
yuvarlanır (`modules/shader/src/ashader.cpp:446`).

**Derleme.** `compile_uncached` bir `glslang::TShader` kurar: GLSL 460, Vulkan istemcisi, Vulkan 1.3 hedefi, SPIR-V 1.5, glslang'ın
varsayılan kaynak limitleri (`GetDefaultResources`) (`modules/shader/src/compile.cpp:109`). SPIR-V 1.6 bilerek kullanılmaz (`discard` →
demote-to-helper isteğe bağlı bir aygıt özelliği olurdu) (`modules/shader/include/astral/shader/compile.hpp:2`). Optimizasyon ve
hata ayıklama bilgisi kapalıdır (`modules/shader/src/compile.cpp:140`). `#include <ad>` çağrıları `GlslInclude` listesinden gelir; dosya sistemi
erişimi yoktur (`modules/shader/src/compile.cpp:36`). Derleyici günlüğündeki `ERROR: ad:satır: mesaj` satırları `ShaderError`'a çevrilir
(`modules/shader/src/compile.cpp:54`). glslang süreç başına bir kez başlatılır ve `atexit` ile kapatılır (`modules/shader/src/compile.cpp:19`).

**Toplu ve paralel derleme (ADR-0047).** `compile_glsl_batch` sonuçları istek sırasıyla döndürür; `services.jobs` ve birden çok istek
varsa her biri ayrı iş olarak koşar (`modules/shader/src/compile.cpp:173`). Render, bir yüzey shader'ının aşamalarını tek toplu çağrıda derler: G-buffer
fragment + vertex + gölge vertex (+ `alpha_test` ise gölge fragment); `blend_mix`/`blend_add` için ileri (forward) fragment + vertex
(`modules/render/src/surface_shader.cpp:385`, `modules/render/src/surface_shader.cpp:403`). Post/sky ve parçacık/hacim shader'ları da aynı yolu kullanır
(`modules/render/src/surface_shader_rt.cpp:257`).

**SpirvCache (ADR-0047).** Anahtar: derleyici kimliği (`"astral-spirv-1|glslang <sürüm>|glsl460|vulkan1.3|spirv1.5|noopt|nodebug"`), aşama, ad,
GLSL kaynağı ve tüm include'ların özeti üzerinden SHA-256 (`modules/shader/src/compile.cpp:81`, `modules/shader/src/compile.cpp:97`).
Dosya: `<klasör>/<hex anahtar>.spv`; başlık = sihir `ASPV`, sürüm 1, anahtar, sözcük sayısı, FNV-1a 64 sağlama toplamı
(`modules/shader/src/spirv_cache.cpp:15`, `modules/shader/src/spirv_cache.cpp:42`). Okuma anahtar, uzunluk, sağlama toplamı ve SPIR-V
sihrini doğrular; bozuk/kesik dosya "ıska" sayılır ve yeniden derlenip üzerine yazılır
(`modules/shader/src/spirv_cache.cpp:42`, `modules/shader/src/spirv_cache.cpp:85`). Yazma geçici dosya + `rename` ile atomiktir
(`modules/shader/src/spirv_cache.cpp:106`). Derleme hataları önbelleğe yazılmaz (`modules/shader/src/compile.cpp:161`). `prune(max_bytes)` en az
yakın kullanılanları siler; "kullanım" dosyanın son yazma zamanıdır ve her isabette güncellenir
(`modules/shader/src/spirv_cache.cpp:102`, `modules/shader/src/spirv_cache.cpp:144`). Önbelleğin sahibi `App`'tir: `ASTRAL_SHADER_CACHE=off|0` kapatır,
`ASTRAL_SHADER_CACHE=<klasör>` o klasörü kullanır, aksi halde `<kullanıcı önbellek klasörü>/AstralEngine/shader-cache/<başlık>`;
açılışta 512 MiB bütçeye `prune` edilir (`modules/app/src/shader_cache_location.cpp:47`, `modules/app/src/app.cpp:212`). Servisler
`Renderer::set_shader_compile_services` ile verilir (`modules/app/src/app.cpp:217`) ve `AssetManager` ile `MaterialLibrary` bunu kullanır
(`modules/assets/src/asset_manager.cpp:216`, `modules/app/src/material_library.cpp:139`). Testler `ASTRAL_SHADER_CACHE`'i derleme klasörüne
ayarlar (lead doğrulaması).

**Cook ile ilişki.** `astral-cook` `.ashader` dosyasını SPIR-V'ye çevirmez; yalnız ayrıştırır, derler ve hatayı `dosya:satır` ile raporlar,
çıktıya kaynak kopyalanır (`modules/assets/src/cook.cpp:280`, `modules/assets/src/cook.cpp:437`). Derleme maliyeti çalışma anında SpirvCache ile
azaltılır.

## Güçlü yanlar

- **Hata satırı doğru:** gövde satırları korunur, `#line` ile glslang hataları `.ashader` satırını gösterir
  (`modules/shader/tests/compile_test.cpp` "compile errors carry the line ...", `modules/shader/tests/ashader_test.cpp`).
- **Önbellek kötü duruma dayanıklı:** hasarlı/kesik/yanlış-anahtarlı dosyalar derlemeyi bozmaz, onarılır; kullanılamayan klasör derlemeyi
  başarısız yapmaz; yarışan thread'ler aynı SPIR-V'yi alır (`modules/shader/tests/spirv_cache_test.cpp`).
- **Önbellek bayat değere karşı korumalı:** anahtara motor include'ları, aşama, ad, derleyici sürümü dahil; engine header değişirse
  yalnızca ıska olur (`modules/shader/tests/spirv_cache_test.cpp` "the cache key changes with an include, ...").
- **Paralel = seri:** toplu paralel derleme seri derlemeyle bayt bayt aynı sonuç ve istek sırası verir
  (`modules/shader/tests/spirv_cache_test.cpp` "parallel batch compiles equal serial ones, in request order").
- **Sıcak başlangıç glslang'sız:** sıcak önbellekte glslang hiç çalışmaz (test "a warm start answers from disk and runs glslang zero times").
- **int32/bool tam temsil** (ADR-0042) ve CRLF toleransı testlerle sabit
  (`modules/render/tests/msr_regression_test.cpp:118`, `modules/shader/tests/ashader_crlf_test.cpp`).

## Zayıf yanlar ve bilinen sınırlar

- **SPIR-V optimize edilmez:** `disableOptimizer = true`, hata ayıklama bilgisi kapalı
  (`modules/shader/src/compile.cpp:141`). Üretilen SPIR-V'nin sürücü tarafındaki derleme/çalışma maliyeti etkisi ölçülmedi.
- **Önişlemci satırı başlıktan önce gelirse başlık ifadesi bozulur:** üst düzey tarayıcı `;` veya `{`'ye kadar okur ve ifadenin ilk
  sözcüğüne bakar; `#define N 4` satırının hemen ardından `uniform float a;` gelirse ifade `#define N 4 uniform float a` olarak
  okunur, ilk sözcük boş kalır, başlık ifadesi olarak tanınmaz ve uniform gövdede ham GLSL olarak kalır
  (`modules/shader/src/ashader.cpp:241`, `modules/shader/src/ashader.cpp:292`). Kod okumasından çıkarım; çalıştırılarak doğrulanmadı ve
  testte yok. Güvenli kullanım: başlık ifadelerini `#define` satırlarından önce yazmak. Dilin `#ifdef` anahtarı da yoktur (bilinçli,
  varyant patlamasını önlemek için ADR-0022).
- **`varying` tipi doğrulanmaz:** yazılan tip dizgesi olduğu gibi saklanır; yanlış tip derleme aşamasında glslang hatası olur
  (`modules/shader/src/ashader.cpp:273`).
- **Yalnız `sampler2D`:** `sampler2DArray`, cube, 3B doku, dizi uniform'ları ve `struct` uniform'ları yok
  (`modules/shader/src/ashader.cpp:77`).
- **`ui` türü tüketicisiz:** `shader_type ui` ayrıştırılır ve render modları tanımlıdır, ama Render/App/Assets tarafında bu türü
  derleyen veya kullanan kod bulunmadı (`modules/shader/src/ashader.cpp:152`; `ShaderType::Ui` için modül dışında referans yok).
  `post` türünde yalnız `after_tonemap` render modu var (`modules/shader/src/ashader.cpp:378`).
- **Önbellek bütünlüğü saldırıya dayanıklı değil:** sağlama toplamı FNV-1a'dır; yalnız kesilme ve bit bozulmasını yakalar
  (`modules/shader/src/spirv_cache.cpp:23`). Klasör paylaşımlıysa (`ASTRAL_SHADER_CACHE=<klasör>`) yazma yetkisi olan biri SPIR-V'yi
  değiştirebilir; SPIR-V sürücüye verilir.
- **Derleyici kimliği elle yönetilen etiket içerir:** `"astral-spirv-1"` etiketi `compile_uncached` davranışı değişince elle
  artırılmalıdır; unutulursa eski girdiler geçerli kalır (`modules/shader/src/compile.cpp:79`). glslang sürümü anahtara dahildir.
- **Budama yalnız açılışta:** `prune` çağırana bağlı; App yalnız başlangıçta çağırır (`modules/app/src/app.cpp:215`). Çalışma
  sırasında önbellek 512 MiB'ı aşabilir. "Son kullanım" dosya `mtime`'ıdır ve yalnız-okunur dosya sisteminde güncellenmez (en iyi çaba).
- **Süreç başına tek glslang başlatması, `atexit` ile kapanış:** statik yok etme sırası kaynaklı sorunlar doğrulanmadı; yalnız
  `initialize_once` var (`modules/shader/src/compile.cpp:19`).
- **Test kapsamı boşlukları:** `uniform_word`'ün doğrudan testi bu modülde yok (AS13 Render testlerinde: `modules/render/tests/msr_regression_test.cpp:118`);
  her ipucu/hata iletisi için ayrıştırıcı testi yok; include bulunamama hatası derleme testlerinde kapsanmaz
  (`modules/shader/tests/compile_test.cpp`).
- **Eksik ADR dosyası:** ADR-0047 (SpirvCache + `ShaderCompileServices` + paralel toplu derleme) kod yorumlarında/brife anılıyor ama
  `docs/decisions/` altında bu numarada dosya yok; kodda `docs/modules/shader.md` referansı ise bu belgedir
  (`modules/shader/include/astral/shader/spirv_cache.hpp:3`).

## Testler

`modules/shader/tests/` (doctest; `test_main.cpp` Core'dan paylaşılır, `modules/shader/CMakeLists.txt:30`):

| Dosya | Kapsam |
|---|---|
| `ashader_test.cpp` | Başlık + gövde, satırlı hata raporu, başlıkta yorum/dizge, uniform düzeni, `body_without`, `shading_model` kabul/ret |
| `ashader_crlf_test.cpp` | CRLF = LF ayrıştırma |
| `compile_test.cpp` | GLSL → SPIR-V, `#line` ile hata satırı, include listesi |
| `spirv_cache_test.cpp` | SHA-256 bilinen yanıtları, sıcak başlangıç (glslang sıfır kez), hasarlı girdi onarımı, anahtar değişimi, hataların önbelleğe yazılmaması, kullanılamaz klasör, paralel = seri, soğuk/sıcak paralel toplu, yarışan thread'ler, `prune` LRU |
| Render düzeyinde | `modules/render/tests/surface_shader_test.cpp` (soğuk/sıcak önbellek ile yüzey shader'ı), `modules/render/tests/msr_regression_test.cpp` (AS13 tipli sözcükler), `modules/render/tests/material_file_test.cpp`, `modules/render/tests/material_crlf_test.cpp` |
| App düzeyinde | `modules/app/tests/shader_cache_location_test.cpp` (`ASTRAL_SHADER_CACHE` çözümü) |

Kapsanmayanlar: önişlemci satırı + başlık sırası, include bulunamama hatası, `sampler2D` varsayılan değer reddi, Render'ın ürettiği şablonların tüm
shading_model/render_mode birleşimleri (kısmen Render testlerinde), önbellek klasörünün çoklu süreç yarışı (yalnız çoklu thread).

## Performans

Modüle özgü mikro ölçüm kayıtlı değil: **Ölçülmedi.** `bench/main.cpp` içinde `bench_shader_compile` (satır 413) `.ashader` ayrıştırma + SPIR-V
derleme süresini ölçer ama bu incelemede çalıştırılmadı. Lead'in uygulama düzeyi ölçümü (examples/the_architect, `--capture --frames 4`, RTX 3060):
SPIR-V önbelleği + paralel derleme öncesi Debug 19.1 s, Release 4.18 s; sonrası Debug soğuk 11.2 s / sıcak 3.95 s, Release soğuk 2.53 s /
sıcak 1.76 s (kaynak: lead raporu; yeniden ölçülmedi; toplam açılış süresidir, yalnız shader derlemesi değildir). `glslang` Debug derlemede büyük
shader başına saniyeler sürebilir (`modules/shader/include/astral/shader/compile.hpp:6`).

## Kararlar

- [0022 — Shader ve materyal dili](../decisions/0022-shader-and-material-language.md)
- [0033 — Gelişmiş PBR ve yol izleyici](../decisions/0033-advanced-pbr-and-path-tracer.md) (`shading_model`)
- [0042 — Tipli shader parametre sözcükleri](../decisions/0042-typed-shader-parameter-words.md)
- ADR-0047 (SpirvCache, `ShaderCompileServices`, paralel derleme): dosyası bu ağaçta yok.
