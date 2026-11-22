# Astral::Assets

Katman 3 (Core + Render). Dosyadan yükleme: görüntüler ve glTF 2.0 modelleri, asenkron, hot-reload'lu
([ADR-0014](../decisions/0014-assets.md)).

```cpp
AssetManager assets({.root = "assets", .renderer = &renderer, .jobs = &jobs, .logger = &log});
AssetId crate = assets.load_model("props/crate.glb");   // hemen döner
assets.update();                                         // her kare (ana thread)
if (const Model* m = assets.model(crate)) { /* m->nodes[i].pieces: {MeshHandle, MaterialHandle} */ }
```

App'te bunu elle yapmazsın: `ctx.assets` hazır ve `ModelRef` component'i yeter:

```cpp
ctx.world.create(Transform{.position = {0, 0, -3}}, ModelRef{"props/crate.glb"});
```

## İki katman

| | Ne | Thread |
|---|---|---|
| `load_gltf`, `decode_image`, `load_image` | Saf CPU: dosya → `ModelData` / `ImageData` | herhangi biri (worker'da çalışır) |
| `AssetManager` | Kimlik, durum, asenkron yükleme, GPU'ya aktarma, hot-reload | `update()` ana thread |

GPU'suz (headless) da çalışır: modeller veri olarak yüklenir, handle'lar geçersizdir.

## AssetId

Varlık köküne göre yolun normalize edilmiş (küçük harf, `/`) kalıcı hash'i: `"Props\\Crate.glb"` ve
`"props/crate.glb"` aynı asset; her makinede, her çalıştırmada aynı kimlik. Aynı yolu tekrar istemek yeni
yükleme başlatmaz.

## glTF desteği

- `.gltf` (harici `.bin`/görüntüler veya `data:` URI) ve `.glb` (gömülü görüntüler).
- Üçgen mesh'ler: `POSITION`, `NORMAL` (yoksa üretilir), `TEXCOORD_0`, indeksler (u8/u16/u32).
- Metallic-roughness materyal: base color, metallic-roughness, normal (+scale), emissive texture'ları;
  `KHR_materials_emissive_strength`. Renk görüntüleri sRGB, veri görüntüleri lineer yüklenir.
- Node hiyerarşisi (TRS veya matris), varsayılan sahne.
- Desteklenmeyenler (uyarı + atlanır): skin, animasyon, morph target, sparse accessor, alfa karışımı,
  `TEXCOORD_1`. Bilinmeyen *zorunlu* uzantı → yükleme başarısız.

## Hot-reload

`update()` izlenen dosyaların (glTF, buffer'lar, görüntüler) değişme zamanlarını `reload_poll_seconds`
aralıkla kontrol eder; değişince arka planda yeniden yükler:

- Yapı aynıysa mesh/materyal/texture **handle'ları korunarak yerinde** güncellenir — sahnedeki her şey
  anında yeni hâli çizer, `Model::version` değişmez.
- Node ağacı veya parça sayısı değiştiyse yeni kaynaklar oluşur, `version` artar; App `ModelRef`
  entity'lerinin parçalarını yeniden oluşturur.
- Bozuk bir düzenleme son çalışan sürümü korur (hata loglanır), dosya tekrar değişince yeniden dener.

## Belgeler (M5c)

`AssetId load_document(path)` + `const Document* document(id)` → `{Json json; uint32_t version}`. JSON
(sahne, prefab, ayar) **senkron** okunur (küçük dosyalar), hot-reload'da yeniden okunur ve `version`
artar; ayrıştırılamayan düzenleme son sağlam sürümü korur. Prefab kütüphanesi bunu kullanır.

## Cook ve paket (M6)

Karar: [ADR-0016](../decisions/0016-packaging.md).

```cpp
CookReport r = cook_directory({.source = "content", .output = "build/cooked", .jobs = &jobs});
write_pack("build/cooked", "build/game.apak");
AssetManager assets({.root = "content", .renderer = &renderer, .jobs = &jobs, .pack = "build/game.apak"});
```

| Kaynak | Cook çıktısı (aynı yol) |
|---|---|
| `.gltf` / `.glb` | `AMDL` cook edilmiş model: texture'lar kullanıma göre BC7 sRGB (renk), BC7 (veri), BC5 (normal) + tam mip zinciri; `.bin` ve görüntüler içine katılır |
| `.scene` / `.prefab` / `.json` / `.material` | ikili JSON (`AJB1`); `feature.json` da buna girer (App ikili/metin ayırt eder) |
| `.luau` | bytecode (`"ALBC"` + Luau bytecode) — `astral-cook` Script modülünün derleyicisini bağlar, `CookOptions::compile_script`; sözdizimi hatası `dosya:satır` ile build'i kırar. Derleyici verilmezse kaynak kopyalanır |
| `.actions`, diğerleri | olduğu gibi kopyalanır |
| diğer | kopya |

- `AssetManager` türü içerikten anlar: gevşek klasörde kaynak da cook edilmiş dosya da açılır;
  `pack` verilirse her şey `.apak`'tan okunur (hot-reload yok). `from_pack()`.
- Artımlı: `.astral-cook.json` içerik hash'leri ve model bağımlılıkları; değişmeyen atlanır, kaynağı
  silinen çıktı silinir, seçenek değişirse hepsi yeniden.
- `.apak`: dosya başına zstd (kazandırmıyorsa ham), okumada boyut + hash doğrulaması; `Pack::open/read/find`
  thread-safe. Hasarlı paket hata verir, çökmez.
- BC desteklemeyen GPU: yüklemede `decompress_image` ile RGBA8'e açılır.
- Komut satırı: `astral-cook <kaynak> -o <cook> [--pack oyun.apak] [--uncompressed] [--quality 0..4] [--level N] [--jobs N] [-q]`.
- CMake: `astral_add_game(oyun SOURCES … ASSETS içerik/)` → `oyun_package` hedefi `build/packages/oyun/`
  altına exe + `oyun.apak` koyar ([guides/writing-a-game.md](../guides/writing-a-game.md#paketleme)).

## Bilinen sınırlar

- Gevşek kaynak modda texture'lar sıkıştırılmamış RGBA8 (cook edilince BC7/BC5).
- Paket okuması dosya başına `ifstream` (memory-map yok); okumalar tek seferlik ve tam (kısmi/streaming
  okuma yok).
- Cook tek makinede; dağıtık/önbellek paylaşımlı cook yok. BC7 kodlaması yavaş kısım (kalite 1 varsayılan).
- Mobil formatlar (ASTC/ETC2) yok.
- Yükleme sırasında GPU aktarımı ana thread'de ve bloklayıcı (büyük modellerde kare takılması; ileride
  kademeli aktarım).
- Asset'ler hiç boşaltılmaz (referans sayımı yok); `AssetManager` yok edilince hepsi gider.
- Animasyon/skin yok (animasyon fazı).
