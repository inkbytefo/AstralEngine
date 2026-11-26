# Assets (`Astral::Assets`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core, Astral::World, Astral::Render (`modules/assets/CMakeLists.txt:56`); üçüncü parti: stb_image, bc7enc/rgbcx, zstd v1.5.7 (FetchContent, modül içinde derlenir) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Diskten (veya `.apak` paketinden) varlık yükler: glTF 2.0 modelleri, görüntüler, JSON belgeleri (sahne, prefab, ayar),
ham baytlar; yüklenen modeli `Renderer` kaynaklarına çevirir ve hot-reload yapar. Ayrıca yayın hattının "cook" adımını
(`cook_directory`), model/görüntü ikili biçimlerini, `.apak` paket biçimini ve terrain yükseklik karolarını içerir.
Yapmadığı şeyler: animasyon/skin (glTF'de atlanır), referans sayımı veya varlık boşaltma, kısmi/streaming okuma,
KTX2/HDR görüntü, mobil doku formatları.

## Public API

Header'lar `modules/assets/include/astral/assets/` altındadır.

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `AssetId`, `AssetId::from_path` | Yolun normalize (küçük harf, `/`) FNV tabanlı kalıcı kimliği | `modules/assets/include/astral/assets/asset_manager.hpp:28` |
| `AssetManager` (`load_model`, `load_document`, `read`, `update`, `wait_all`, `state`, `model`, `document`) | Kimlik/durum, asenkron model yükleme, GPU aktarımı, hot-reload; yalnız ana thread | `modules/assets/include/astral/assets/asset_manager.hpp:81` |
| `AssetManagerDesc` | `root`, `renderer`, `jobs`, `hot_reload`, `reload_poll_seconds` (0.5 s), `pack` | `modules/assets/include/astral/assets/asset_manager.hpp:69` |
| `Model`, `Model::Piece`, `Model::Node`, `Document` | Çizime hazır model (parça başına `MeshHandle`+`MaterialHandle`) ve JSON belgesi; ikisinde de `version` | `modules/assets/include/astral/assets/asset_manager.hpp:39` |
| `ModelRef`, `ModelMaterialOverride` | Entity'ye "bu modeli göster" component'i; glTF materyal slotu → `.material` dosyası geçersiz kılma. Serileştirilebilir | `modules/assets/include/astral/assets/asset_manager.hpp:122` |
| `load_gltf`, `ModelData`, `ModelMaterial`, `ModelNode`, `ModelImage` | Saf CPU glTF yükleyici; thread-safe | `modules/assets/include/astral/assets/model.hpp:97` |
| `decode_image`, `load_image` | PNG/JPEG/TGA/BMP (8 bit) → RGBA8 | `modules/assets/include/astral/assets/model.hpp:20` |
| `cook_directory`, `CookOptions`, `CookReport` | Artımlı cook: model, harita, terrain, shader doğrulama, belge, script | `modules/assets/include/astral/assets/cook.hpp:74` |
| `cook_image`, `decompress_image`, `cook_model_images`, `TextureUse` | BC7 (renk/veri), BC5 (normal) + mip zinciri; BC desteklemeyen GPU için RGBA8'e açma | `modules/assets/include/astral/assets/cook.hpp:38` |
| `write_cooked_model`, `read_cooked_model`, `is_cooked_model` | `AMDL` cook edilmiş model biçimi | `modules/assets/include/astral/assets/cook.hpp:46` |
| `write_pack`, `Pack` (`open`, `read`, `find`), `PackEntry` | `.apak`: dosya başına zstd, okumada boyut + hash doğrulama | `modules/assets/include/astral/assets/pack.hpp:37` |
| `TerrainManifest`, `read_terrain_manifest`, `read_terrain_tile`, `write_terrain_tile` | `.aterrain` manifesti ve 16 bit yükseklik karoları (PNG veya `ATH1`) | `modules/assets/include/astral/assets/terrain.hpp:17` |
| `export_model_material` | Bir glTF materyalini ve görüntülerini yeni bir klasöre `.material` olarak çıkarır (editör inspector'ı kullanır) | `modules/assets/include/astral/assets/material_export.hpp:12` |

Komut satırı aracı `astral-cook` bu modülü kullanır (`tools/astral_cook/main.cpp`; bu belgede ayrıca incelenmedi).

## Nasıl çalışır

**İki katman.** `load_gltf`, `decode_image`, cook fonksiyonları ve `Pack` saf CPU'dur ve worker'da çalışabilir.
`AssetManager` ana thread'e aittir: `entries` haritası kilitsizdir (`modules/assets/src/asset_manager.cpp:75`), yükleme işi
yalnız `load_model_data`'yı (dosya/pack okuma + parse) `JobSystem`'e verir (`modules/assets/src/asset_manager.cpp:168`).
`update()` biten işleri toplar (GPU aktarımı burada, ana thread'de) ve izlenen dosyaları yoklar
(`modules/assets/src/asset_manager.cpp:487`). `desc.renderer == nullptr` ise (headless) model yalnız veri olarak
yüklenir, handle'lar geçersizdir (`modules/assets/src/asset_manager.cpp:341`).

**Model yükleme.** `load_model_data` dosyanın ilk 4 baytına bakarak `AMDL` (cook edilmiş) ile glTF kaynağını ayırır; pack
modunda her şey cook edilmiş sayılır (`modules/assets/src/asset_manager.cpp:114`). Aynı yol ikinci kez istenirse yeni yükleme
başlamaz (`modules/assets/src/asset_manager.cpp:440`).

**GPU aktarımı ve hot-reload.** `stage_resources` önce doku, sonra materyal, sonra mesh üretir; sayılar aynıysa mevcut
handle'ları `update_texture/update_material/update_mesh` ile yerinde günceller, değilse yenisini yaratır; eskiler ancak
hepsi hazır olunca yok edilir (aşamalı yayın, MSR AS06) (`modules/assets/src/asset_manager.cpp:244`,
`modules/assets/src/asset_manager.cpp:258`, `modules/assets/src/asset_manager.cpp:318`). Düğüm ağacı/parça bağları aynıysa
`Model::version` artmaz; değiştiyse artar ve App `ModelRef` parçalarını yeniden oluşturur
(`modules/assets/src/asset_manager.cpp:196`, `modules/assets/src/asset_manager.cpp:360`). GPU aktarımı başarısızsa önceki sürüm
kalır (`modules/assets/src/asset_manager.cpp:332`). İzleme dosya `mtime` karşılaştırmasıdır; her `reload_poll_seconds`
aralığında tüm izlenen dosyalara bakılır (`modules/assets/src/asset_manager.cpp:403`). Bozuk düzenleme son çalışan
sürümü korur (`modules/assets/src/asset_manager.cpp:389`). Pack modunda hot-reload kapatılır
(`modules/assets/src/asset_manager.cpp:94`).

**Belgeler.** `load_document` JSON'u **senkron** okur (metin veya ikili `AJB`), hot-reload'da tekrar okur ve `version`'ı
artırır; ayrıştırılamayan düzenleme son sağlam sürümü korur (`modules/assets/src/asset_manager.cpp:139`). Prefab kütüphanesi
(World) bunu `DocumentSource` ile kullanır.

**glTF yükleyici.** `.gltf` (harici `.bin`/görüntü veya `data:` base64 URI) ve `.glb` okunur
(`modules/assets/src/gltf.cpp:467`). Desteklenenler: üçgen mesh (`POSITION`, `NORMAL`, `TEXCOORD_0`, `TEXCOORD_1`,
indeksler); accessor bileşen tipi float/uint32/uint16/uint8 (normalized dahil) (`modules/assets/src/gltf.cpp:172`);
eksik normaller alan ağırlıklı üretilir (`modules/assets/src/gltf.cpp:389`); metallic-roughness materyal; normal,
emissive ve occlusion dokuları; `alphaMode` (Opaque/Mask/Blend) ve `doubleSided`; doku başına sampler (wrap/filtre) ve
`KHR_texture_transform` (ölçek/ofset/dönüş/UV seti); `KHR_materials_emissive_strength`
(`modules/assets/src/gltf.cpp:257`); düğüm hiyerarşisi (TRS veya matris; aynalı matris `scale.x` negatif tutularak
korunur) (`modules/assets/src/gltf.cpp:417`); düğüm döngüsü ve çift ebeveyn reddedilir
(`modules/assets/src/model_hierarchy.hpp:12`). Bir görüntü hem renk hem veri olarak kullanılıyorsa iki kopya üretilir
(`modules/assets/src/gltf.cpp:212`). Kök düğümler varsayılan sahneden veya (sahne yoksa) ebeveynsiz düğümlerden gelir
(`modules/assets/src/gltf.cpp:446`). Accessor sayısı 2^28 ile sınırlıdır (`modules/assets/src/gltf.cpp:168`).
**Skin, animasyon, morph target, sparse accessor, üçgen dışı primitive atlanır ve `ModelData::warnings`'e uyarı yazılır**
(`modules/assets/src/gltf.cpp:358`, `modules/assets/src/gltf.cpp:437`, `modules/assets/src/gltf.cpp:463`,
`modules/assets/src/gltf.cpp:336`, `modules/assets/src/gltf.cpp:165`, `modules/assets/src/gltf.cpp:339`); `AssetManager` bu uyarıları
log'a yazar (`modules/assets/src/asset_manager.cpp:369`). `extensionsRequired` içinde yalnız `KHR_materials_emissive_strength`
ve `KHR_texture_transform` kabul edilir, diğer zorunlu uzantı yüklemeyi başarısız yapar (`modules/assets/src/gltf.cpp:499`).
`COLOR_0`, `TANGENT` ve zorunlu olmayan diğer uzantılar (ör. clearcoat) okunmaz/uyarı verilmez (kodda ilgili okuma yok:
`modules/assets/src/gltf.cpp:342`).

**glTF materyali → shader.** Her glTF materyali motorun yüzey shader'ı olarak ele alınır: `imported_surface_source`
bir `.ashader` kaynağı üretir (alpha_test/blend_mix/cull_disabled render modları, isteğe bağlı sampler ve UV dönüşümü
kodu), `AssetManager` bunu `compile_surface_shader` ile derler ve anahtar (alfa modu, çift yüz, occlusion, özel sampler,
özel UV) başına bir `ShaderHandle` paylaşır (`modules/assets/src/imported_material.hpp:14`,
`modules/assets/src/imported_material.hpp:123`, `modules/assets/src/asset_manager.cpp:208`). Test: `assets_app_test`
"glTF masked and blended materials build Vulkan shader variants" (`modules/app/tests/assets_app_test.cpp`).

**ModelMaterialOverride.** `ModelRef::material_overrides` slot adı → `.material` dosyası eşler. Çözümleme App'te
`RenderBridge::resolve_model_overrides` içinde yapılır: slot adı modelde tek olmalı, aynı slot için tek override olmalı,
yoksa uyarı verilip atlanır (`modules/app/src/services/render_bridge.cpp:315`). Override yalnız bir glTF modelinin slotuna
uygulanır; entity başına genel bir `MaterialOverride` yoktur (lead doğrulaması).

**Cook (`astral-cook 7`).** `cook_directory` kaynak klasörü tarar (`.` ile başlayan dosya/klasörler atlanır), uzantıya göre
şunları yapar (`modules/assets/src/cook.cpp:26`):

| Kaynak | Çıktı |
|---|---|
| `.gltf`/`.glb` | `AMDL` (sürüm 3; sürüm 1–3 okunur) cook edilmiş model; `.bin` ve görüntüler içine katılır; modeller `JobSystem` ile paralel (`modules/assets/src/cook.cpp:152`) |
| `.world` + `<ad>.entities/` | İndeks + `<ad>.cells/*.cell`; önce `expand_map_level_instances` ile level instance'lar açılır (`modules/assets/src/cook.cpp:210`, `modules/assets/src/cook.cpp:225`) |
| `.aterrain` | Manifest ikili JSON + `.tiles/<x>_<z>.aheight` (`ATH1`) karoları; paylaşılan kenar örnekleri eşleşmezse cook hata verir (`modules/assets/src/cook.cpp:300`, `modules/assets/src/cook.cpp:318`) |
| `.ashader` | **Kaynak olarak kopyalanır**; cook yalnız `parse_ashader` + glslang derlemesiyle doğrular, hatayı `dosya:satır` ile raporlar (`modules/assets/src/cook.cpp:280`). SPIR-V cook edilmez; oyun çalışma anında derler |
| `.scene`/`.prefab`/`.json`/`.material`/`.vfx` | İkili JSON (`AJB`) (`modules/assets/src/cook.cpp:444`) |
| `.luau` | `compile_script` verilmişse bytecode, aksi halde kaynak kopya; derleme hatası build'i kırar (`modules/assets/src/cook.cpp:438`) |
| diğer | Olduğu gibi kopya |

Artımlılık: `.astral-cook.json` önbelleği her kaynak için içerik hash'i (FNV-1a 64 bit) + bağımlılık listesi tutar; hash,
`cook_version` ("astral-cook 7") ve doku seçeneklerinin (`;bc`/`;rgba8`, `;q<kalite>`) tohumuyla başlar, dolayısıyla biçim
değişince her şey yeniden pişer (`modules/assets/src/cook.cpp:21`, `modules/assets/src/cook.cpp:24`,
`modules/assets/src/cook.cpp:513`). Kaynağı silinen çıktı silinir; cook edilemeyen kaynak son sağlam çıktısını korur
(`modules/assets/src/cook.cpp:528`). Doku kodlaması: Color → BC7 sRGB, Data → BC7 lineer, Normal → BC5 (mip'ler yeniden
normalize edilir), `bc7_quality` 0–4 (varsayılan 1) (`modules/assets/src/texture_codec.cpp:54`,
`modules/assets/include/astral/assets/cook.hpp:32`). Doku sınıflaması materyal kullanımından çıkarılır
(`modules/assets/src/texture_codec.cpp:133`).

**`.apak`.** Başlık `"APAK"` + sürüm 1 + tablo ofseti + giriş sayısı; bloklar, sonra tablo. Her dosya zstd ile sıkıştırılır,
en az %5 kazandırmıyorsa ham yazılır; yazma geçici dosyaya yapılıp yer değiştirilir (`modules/assets/src/pack.cpp:21`,
`modules/assets/src/file_util.hpp:40`). Girişler `AssetId` ile bulunur; `Pack::read` her çağrıda yeni `ifstream` açar
(kilitsiz thread-safe), zstd çerçeve boyutunu ve içerik hash'ini doğrular (`modules/assets/src/pack.cpp:132`). Tablo
ofset/boyut doğrulaması `open` içindedir (`modules/assets/src/pack.cpp:80`).

**Terrain varlıkları.** Manifest: `format "astral.terrain"` v1, `samples` 5..1025 ve 2^n+1, `spacing_m`, yükseklik
aralığı, karo adresleri; karo dosyası `.png` veya `.aheight` olmalı ve yol güvenli olmalı
(`modules/assets/src/terrain.cpp:17`, `modules/assets/src/terrain.cpp:43`). Karo = 16 bit gri PNG (stb) veya `ATH1`
(u32 sihir + u16 örnekler) (`modules/assets/src/terrain.cpp:88`). Terrain'in çalışma anı kullanımı (mesh, fizik) bu modülde
değildir; bkz. [ADR-0039](../decisions/0039-terrain-foundation.md).

## Güçlü yanlar

- Saf CPU/GPU ayrımı: `model.hpp` Renderer'ı include etmez (derleme zamanı testle sabitlenmiş:
  `modules/assets/tests/model_header_test.cpp:25`); headless sunucu/test yüklemesi mümkün.
- Bozuk girdiye karşı sertleştirilmiş: wrap eden boyutlar, negatif/kesirli indeksler, bozuk yüzde kaçışı, çok büyük sayım,
  düğüm döngüsü ve çift ebeveyn reddedilir (`modules/assets/tests/msr_regression_test.cpp` A11, A12, AS01–AS05).
- Cook ve pack yazımı atomik (geçici dosya + rename); hasarlı pack çökmez, hata verir (`modules/assets/tests/cook_test.cpp`
  ".apak ... damage is detected").
- Cook artımlıdır ve kaynak/seçenek/sürüm değişimini hash'ler; başarısız kaynak son sağlam çıktıyı korur (AS09) ve silinen
  hücre çıktısı yeniden üretilir (AS10) (`modules/assets/tests/cook_test.cpp`).
- Aynı `ModelRef`/sahne gevşek kaynak klasörde, cook edilmiş klasörde ve `.apak`'ta çalışır (kaynak yolu korunur)
  (`modules/assets/include/astral/assets/cook.hpp:8`, test `cook_test.cpp` "the asset manager reads cooked assets...").
- Hot-reload aşamalı yayınlar: GPU aktarımı başarısız olursa önceki sürüm kalır (`modules/assets/src/asset_manager.cpp:332`).
- glTF alfa modu, çift yüz, occlusion, sampler ve `KHR_texture_transform` + ikinci UV seti cook'tan sağ çıkar
  (`modules/assets/tests/gltf_test.cpp` son iki test).

## Zayıf yanlar ve bilinen sınırlar

- **Vitrin yazımında gözlendi (2026-11-27):** `AssetManager` tek asset kökü destekler (dışarıdan model açan `examples/asset_viewer` kendi feature dosyaları için yedek yol yazmak zorunda kaldı); çalışma anı `Model` köşe/üçgen sayısı ve sınır kutusu tutmaz; `.aterrain` katman/splatmap verisi taşımaz (`examples/world_showcase` katmanları shader'da hesaplar).
- **Skin ve animasyon yok:** glTF'deki `skin`, `JOINTS_0`, `animations`, morph target yalnız uyarı verilip atlanır
  (`modules/assets/src/gltf.cpp:358`, `modules/assets/src/gltf.cpp:437`, `modules/assets/src/gltf.cpp:463`). İskeletli karakter
  veya animasyonlu model yüklenemez (statik poz).
- **`COLOR_0`, `TANGENT`, çoğu glTF uzantısı yok**, ve zorunlu olmayanlar için uyarı bile yok (sessiz atlanır)
  (`modules/assets/src/gltf.cpp:342`). Zorunlu uzantılardan yalnız iki tanesi kabul edilir; Draco/meshopt/KTX2/quantization
  içeren dosya yüklenmez (`modules/assets/src/gltf.cpp:499`). Accessor'da işaretli bileşen tipleri desteklenmez
  (`modules/assets/src/gltf.cpp:172`).
- **GPU aktarımı ana thread'de ve bloklayıcı:** `finish`/`stage_resources` `update()` içinde çalışır, kare başına bütçe yoktur
  (`modules/assets/src/asset_manager.cpp:258`, `modules/assets/src/asset_manager.cpp:373`). Lead'in `the_architect` başlangıç
  ölçümünde kalan maliyetlerden biri sıralı doku çözme/yükleme olarak kayda geçti (bkz. Performans).
- **Varlık boşaltma yok:** referans sayımı veya `unload` API'si yok; `AssetManager` yok olunca hepsi gider
  (`modules/assets/include/astral/assets/asset_manager.hpp:81`).
- **Occlusion dokusu cook'ta sıkıştırılmaz:** `cook_model_images` yalnız normal/renk/emissive/metallic-roughness
  görüntülerini sınıflar; yalnız occlusion için kullanılan görüntü `uses` içinde olmadığı için cook edilmeden RGBA8 olarak
  yazılır, mip'ler yüklemede üretilir (`modules/assets/src/texture_codec.cpp:140`, `modules/assets/src/cooked_model.cpp:236`).
  Kod okumasından çıkarıldı; boyut etkisi ölçülmedi.
- **Cooked mesh sıkıştırılmaz:** her köşe 10 float (pozisyon, normal, uv, uv1), indeksler u32; nicemleme/meshopt yok
  (`modules/assets/src/cooked_model.cpp:213`).
- **Gevşek kaynak modunda dokular cook'suz RGBA8** (BC7/BC5 yalnız cook sonrası); bu, kaynaktan çalışan editörde VRAM ve
  yükleme süresi maliyetidir (`modules/assets/src/image.cpp:46`).
- **Cook tek makine, tek süreç:** paylaşımlı/dağıtık önbellek yok; BC7 kodlaması yavaş kısım. Her görüntü içinde BC7
  kodlaması tek thread'dir; paralellik model/görüntü düzeyindedir (`modules/assets/src/cook.cpp:175`,
  `modules/assets/src/texture_codec.cpp:80`).
- **Önbellek bütünlüğü yalnız hash/mtime'dır;** hot-reload izlemesi `mtime` yoklamasıdır (olay tabanlı değil), saat çözünürlüğü
  düşük dosya sistemlerinde hızlı ardışık düzenleme kaçabilir (çıkarım, test edilmedi) (`modules/assets/src/asset_manager.cpp:403`).
- **Pack okuması kopyalar:** her `read` dosyayı açar, sıkıştırılmış bloğu belleğe alıp açar; memory-map ve kısmi okuma yok
  (`modules/assets/src/pack.cpp:139`). Pack hot-reload yapmaz (`modules/assets/src/asset_manager.cpp:94`).
- **Pack açılamazsa her yükleme sessizce başarısız olur** (yalnız bir log satırı; `AssetManager` kurulumu hata döndürmez)
  (`modules/assets/src/asset_manager.cpp:93`).
- **Başlık yorumu kodla uyuşmuyor:** `AssetManagerDesc::jobs` yorumu "Null = update() içinde senkron" der; kodda `jobs`
  yoksa iş `start` içinde, yani `load_model()` çağrısının içinde hemen çalışır
  (`modules/assets/include/astral/assets/asset_manager.hpp:72` ↔ `modules/assets/src/asset_manager.cpp:182`). Ayrıca
  `load_gltf` başlık yorumu kameraların uyarı verip atlandığını söyler (`modules/assets/include/astral/assets/model.hpp:95`);
  `gltf.cpp` içinde kamera için uyarı yok.
- **Görüntü formatı:** yalnız 8 bit PNG/JPEG/TGA/BMP; HDR, KTX2, DDS yok (`modules/assets/src/image.cpp:6`).
  Mobil formatlar (ASTC/ETC2) yok.
- **Cook'a bağlı `.ashader` doğrulaması kaynakta kalır:** shader derleme maliyeti çalışma anında ödenir; SPIR-V önbelleği
  render/shader tarafında (bkz. [shader.md](shader.md)), cook çıktısına girmez (`modules/assets/src/cook.cpp:437`).
- **`load_model` ve `load_document` aynı id uzayını paylaşır:** bir yol önce biriyle yüklendiyse diğeri yeniden yükleme
  başlatmadan o girdiyi döndürür (`modules/assets/src/asset_manager.cpp:440`, `modules/assets/src/asset_manager.cpp:453`).
  Pratikte yol uzantısı ayırır; yanlış kullanımda model(id) null döner. Test yok.
- **Bilinen sınır `AssetManager` testlerinde:** modülün testleri headless'tir; GPU aktarım/in-place güncelleme yolu
  (`stage_resources`) bu modülün testlerinde çalışmaz, yalnız App düzeyi testlerle kısmen kapsanır (doğrulanmadı).

## Testler

`modules/assets/tests/` (doctest, `modules/assets/CMakeLists.txt:63`):

| Dosya | Kapsam |
|---|---|
| `gltf_test.cpp` (+ `gltf_writer.hpp` yardımcı) | .gltf/.glb yükleme, görüntü hem renk hem veri, normal üretimi, bozuk dosya, materyal çıkarma, alfa/çift yüz/occlusion/sampler, UV1 + `KHR_texture_transform` |
| `asset_manager_test.cpp` | Kimlik kararlılığı, asenkron yükleme, hot-reload (aynı yapı → `version` sabit; yeni yapı → artar), belge hot-reload ve bozuk düzenleme |
| `cook_test.cpp` | BC7/BC5 mip zinciri, cook edilmiş model gidiş-dönüş ve hasar reddi, `cook_directory` artımlılığı, `.apak` yaz/oku/hasar, pack ve gevşek cook klasöründen yükleme, `.world` → hücre, `.material` ikili JSON + shader derleme hatası satırı, `.luau` derleyici, AS08–AS10 |
| `msr_regression_test.cpp` | A11, A12, AS01–AS05 (wrap eden boyutlar, bozuk sayımlar, düğüm döngüsü, aynalı matris, indeks dönüşümleri, yüzde kaçışı) |
| `terrain_test.cpp` | 16 bit örnek ve satır yönü, terrain cook/pack ve paylaşılan kenar denetimi, belirsiz karo adresi/yol reddi |
| `model_header_test.cpp` | `model.hpp`'nin Renderer'a bağlı olmaması |
| App düzeyinde | `modules/app/tests/assets_app_test.cpp`: `ModelRef` düğüm ağacı, parça yaşam döngüsü, slot override, maskeli/karışımlı materyal shader varyantları |

Kapsanmayanlar: GPU yükleme/yerinde güncelleme yolu bu modülde; `ASTRAL_SHADER_CACHE`/glslang paralelliğiyle glTF yüzey
shader'ı derlemesi; çok büyük/uzun glTF yükleme zamanı; pack'te eşzamanlı okuma stres testi; fuzz yok (doğrulanmadı).
TSan/ASan Windows'ta çalıştırılmadı.

## Performans

Bu modüle özel ölçüm yok: **Ölçülmedi.** Bağlam olarak lead'in ölçümü (examples/the_architect, `--capture --frames 4`, RTX 3060):
SPIR-V önbelleği sonrası Release 2.53 s soğuk / 1.76 s sıcak, Debug 11.2 s / 3.95 s; kalan maliyetler arasında
sıralı doku çözme/yükleme sayıldı. Bu rakamlar AssetManager'a atfedilmedi, kaynak: lead raporu (yeniden ölçülmedi).

## Kararlar

- [0014 — Varlıklar](../decisions/0014-assets.md)
- [0016 — Paketleme](../decisions/0016-packaging.md)
- [0021 — Dünya ve seviye sistemi](../decisions/0021-world-and-level-system.md) (`.world`/`.cell` cook)
- [0036 — Dünya yazarlığı ve level instance'lar](../decisions/0036-world-authoring-and-level-instances.md) (cook sırasında açılım)
- [0039 — Terrain temeli](../decisions/0039-terrain-foundation.md)
- [0041 — Kayıpsız JSON tamsayıları](../decisions/0041-lossless-json-integers.md) (AJB uint64 etiketi, cook sürümü 7)
- [0042 — Tipli shader parametre sözcükleri](../decisions/0042-typed-shader-parameter-words.md)
