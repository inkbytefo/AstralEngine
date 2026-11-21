# ADR-0016 — Paketleme: cook, `.apak`, `astral_add_game`

- **Durum:** Kabul
- **Tarih:** 2026-11-21
- **Bağlam:** ADR-0012 §4 (paketleme yönü), ADR-0014 (asset'ler), ADR-0015 (sahneler)

## Sorun

Oyun şimdilik kaynak dosyalarını açıyor: PNG'leri her açılışta çözüyor, mip zincirini CPU'da kuruyor,
glTF ve JSON ayrıştırıyor. Dağıtılan bir oyun kaynak klasörüne bağımlı olmamalı, GPU'ya hazır veri
okumalı ve tek bir dosya olarak taşınabilmeli. Geliştirme sırasında ise hot-reload'lu gevşek dosyalarla
çalışmaya devam edilmeli.

## Karar

### 1. Cook: aynı yol, farklı içerik

`cook_directory` (CLI'da `astral-cook`) kaynak klasörü cook klasörüne çevirir. **Mantıksal yol
değişmez:** `props/crate.glb`, cook sonrası da `props/crate.glb` adıyla durur; yalnızca içeriği cook
edilmiş model (`AMDL`) olur. Böylece `ModelRef`, sahneler ve prefab'lar üç modda da aynı yolu kullanır:
gevşek kaynak, gevşek cook klasörü ve `.apak`. Yükleyici türü dosya uzantısından değil, içeriğin
başlığından (magic) anlar.

| Kaynak | Cook çıktısı |
|---|---|
| `.gltf` / `.glb` | `AMDL`: mesh, materyal, node ağacı + hazır mip zincirli texture'lar. Kullandığı `.bin` ve görüntüler modele katılır, ayrıca kopyalanmaz. |
| `.scene` / `.prefab` / `.json` | İkili JSON (`AJB1`): aynı değerler, aynı üye sırası. Sahne kodu değişmeden çalışır. |
| diğer her şey | Olduğu gibi kopyalanır. |

### 2. Texture'lar: kullanıma göre format

Materyaller her görüntünün nasıl kullanıldığını söyler; format buna göre seçilir:

| Kullanım | Format | Not |
|---|---|---|
| Base color, emissive | BC7 sRGB | Algısal ağırlıklar |
| Metallic-roughness | BC7 lineer | |
| Normal map | BC5 (X/Y) | Z shader'da `sqrt(1 - x² - y²)` ile kurulur; mip'ler yeniden normalize edilir |

- Kodlayıcı: bc7enc + rgbcx (MIT/kamu malı). Çözücü (bc7decomp) BC desteklemeyen GPU için yedek yol olarak
  da kullanılıyor: `AssetManager` yüklemede RGBA8'e açıyor.
- Renderer'a hazır mip zinciri `ImageData::levels` + `ImageFormat` ile veriliyor. GPU katmanı `BC5Unorm`,
  `BC7Unorm`, `BC7Srgb` formatlarını ve `Feature::TextureCompressionBC` özelliğini kazandı.
- Normal Z'nin her zaman shader'da kurulması RGBA8 normal map'leri de etkiler. Birim uzunluktaki girdi
  için sonuç aynı.

### 3. Artımlı cook

Cook klasöründe `.astral-cook.json` önbelleği tutulur: dosya başına içerik hash'i (FNV-1a; kaynak +
bağımlılıklar + cook sürümü + seçenekler) ve modellerin bağımlılık listesi. Değişmeyen dosya atlanır.
Bir modelin kullandığı görüntü değişirse model yeniden cook edilir. Kaynağı silinen çıktı da silinir.
Zaman damgası yerine içerik hash'i kullanılıyor: dosya geri yüklense ya da `git checkout` damgaları
değiştirse bile yanlış sonuç çıkmaz.

### 4. `.apak`

Başlık + art arda bloklar + tablo. Tablodaki her giriş: yol, konum, saklanan boyut, gerçek boyut,
sıkıştırma bayrağı, içerik hash'i. Bloklar dosya başına zstd ile sıkıştırılır; %5'ten az kazandıran
blok ham saklanır. Okumada boyut, frame başlığı ve hash doğrulanır; hasarlı paket çökme yerine hata
verir. Paket deterministik (sıralı); önce geçici dosyaya yazılıp sonra yeniden adlandırılır.

ADR-0012 memory-map öngörüyordu. Bunun yerine her okuma kendi `ifstream`'ini açıyor: platform kodu
gerekmiyor, worker thread'lerden kilitsiz ve güvenli. Ölçüm gerektirirse mmap'e geçilir; arayüz
(`Pack::read`) aynı kalır.

### 5. Oyun ve CMake

- `AppDesc::pack`: göreli yol exe'nin klasörüne göre çözülür (`executable_directory()`). Dosya **varsa**
  her şey pakettan okunur, hot-reload kapanır. Yoksa `asset_root`'tan gevşek dosyalar ve hot-reload
  kullanılır. Ayrı bir mod bayrağı yok; geliştirme derlemesinin yanında paket olmadığı için geliştirme
  hep gevşek dosyalarla çalışır.
- `astral_add_game(name SOURCES … ASSETS dir)` iki tanım verir: `ASTRAL_GAME_ASSETS` (kaynak klasörün
  mutlak yolu) ve `ASTRAL_GAME_PACK` (`name.apak`). `<name>_package` hedefi cook eder, paketler ve
  `build/packages/<name>/` altına çalıştırılabilir bir klasör (exe + `.apak`) hazırlar. Shader'lar
  exe'ye gömülü olduğu için başka dosya gerekmiyor. `ASTRAL_PACKAGE_GAMES=ON` bu hedefi ALL'a ekler.
- Kurulu pakette `astral-cook` `Astral::astral_cook` olarak export edilir, `AstralGame.cmake` config ile
  birlikte gelir. `tests/consumer` bu akışın tamamını CI'da sınıyor.
- zstd ve bc7enc `astral_assets`'e doğrudan derleniyor: kurulacak ya da export edilecek ek bir hedef
  yok, public header'lara tipleri sızmıyor.

## Sonuçlar

- Paketlenmiş oyun kaynak klasörüne dokunmuyor; çıktı gevşek modla aynı (RGB odada ortalama piksel
  farkı 0.006/255).
- Texture belleği yaklaşık 4 kat azalıyor (BC7/BC5 = 1 bayt/texel, RGBA8 = 4). Yüklemede görüntü çözme
  ve mip üretimi yapılmıyor.
- Cook süresi BC7 kodlamasında geçiyor (512² üç texture ≈ birkaç saniye). Artımlı önbellek bunu yalnızca
  değişen dosyalara harcatıyor.

## Alternatifler

- **KTX2 / Basis Universal:** platformlar arası transcode iyi, ama ek bağımlılık ve transcode maliyeti
  getiriyor. Masaüstü hedefte BC doğrudan yeterli. Mobil (ASTC) gerekirse cook'a bir hedef formatı eklenir.
- **Uzantıyı değiştirmek (`crate.glb` → `crate.amodel`):** referansları cook sırasında yeniden yazmayı
  gerektirir ve kaynak ile cook modları arasında yollar ayrışır.
- **Sahneyi önceden hesaplanmış component blob'larına cook etmek:** daha hızlı, ama ikinci bir sahne
  yükleyicisi demek. İkili JSON, ayrıştırma maliyetini kaldırıp tek yükleme yolunu koruyor. Binlerce
  prefab örneği için şablon cook'u ileride yapılacak.
- **LZ4:** daha hızlı açılıyor ama daha az sıkıştırıyor. Asset'ler bir kez okunduğu için zstd seçildi.
