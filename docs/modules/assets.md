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

## Bilinen sınırlar

- Texture'lar sıkıştırılmamış RGBA8 (BC7/BC5 sıkıştırma cook adımında, M6).
- Yükleme sırasında GPU aktarımı ana thread'de ve bloklayıcı (büyük modellerde kare takılması; ileride
  kademeli aktarım).
- Asset'ler hiç boşaltılmaz (referans sayımı yok); `AssetManager` yok edilince hepsi gider.
- Animasyon/skin yok (animasyon fazı).
