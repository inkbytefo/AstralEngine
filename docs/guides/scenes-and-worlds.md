# Sahne, harita ve dünya: bir oyun alanı nasıl tanımlanır

Bu rehber kod bilmeden okunabilir. "Bir harita nerede yaşıyor, neyden oluşuyor, nasıl büyüyor" sorusunu
bugünkü hâliyle ve yol haritasındaki hedefiyle (M9, [ADR-0021](../decisions/0021-world-and-level-system.md))
anlatır.

## Bugün (v3, M8 sonu)

Bir oyun alanı **tek bir sahne dosyasıdır** (`.scene`, JSON metni). Pong'un bütün sahası
[`examples/pong/assets/court.scene`](../../examples/pong/assets/court.scene):

```json
{ "id": 1, "components": {
    "astral::Name":      { "value": "camera" },
    "astral::Transform": { "position": [0, 0, 14.5] },
    "astral::Camera":    { "fov_y": 1.01 } } },
{ "id": 2, "components": {
    "astral::Name":  { "value": "court" },
    "pong::Court":   { "seed": 7 },
    "pong::Match":   {},
    "pong::Sounds":  {},
    "astral::AudioSource": { "sound": "sounds/hum.wav", "volume": 0.12, "spatial": false } } }
```

Okuma kılavuzu:
- **Entity** = sahnedeki bir "şey" (kamera, top, ışık, kural nesnesi). Kendi başına hiçbir şey değildir;
  ne olduğunu üstündeki component'ler söyler.
- **Component** = bir özellik paketi, yalnızca veri: `Transform` (nerede), `Camera` (görüyor),
  `PrimitiveMesh` (şekil + materyal), `RigidBody` (fizik), `AudioSource` (ses çalıyor), oyunun kendi
  component'leri (`pong::Paddle`).
- **Prefab** (`.prefab`) = tekrar kullanılan hazır nesne (raket, top). Sahne prefab'ı yerleştirir ve yalnızca
  farkını yazar ("bu raket kırmızı olsun"). Prefab'ın **varyantı** da olabilir (rakip raket = raket + yapay zekâ).
- **Sistem** = davranış, kodda ya da Luau'da (`ball_physics` topu hareket ettirir). Sahne dosyası davranış içermez.
- Oyun çalışırken dosyayı düzenleyip kaydedersen sahne anında güncellenir (hot-reload). F1 ile açılan
  panelden de component'ler düzenlenir.
- Paketlemede (`<oyun>_package`) sahne ikili biçime çevrilip `.apak` içine girer.

Sınırı: bir sahne ya tamamen yüklenir ya hiç. Küçük haritalar, menüler ve odalar için doğru; açık dünya
için değil.

## Dünya = harita (M9, kullanılabilir)

Denemek için: `open_world` örneğini çalıştır (WASD ile uç, `N` gece, F1 → World paneli). Haritası
`tools/make_test_world.py` ile üretilir: 4×4 km, ~4700 nesne, ~980 hücre.

```
maps/
  island.world                 ← haritanın kimliği: ayarlar, ızgara, katmanlar
  island.entities/
    3f2a91c4.ent               ← tek bir ev (prefab örneği) ve çocukları
    8b17d002.ent               ← tek bir ağaç
    ...
  villages/fisher_village.scene  ← hazır bir köy (bölüm örneği), dünyaya yerleştirilir
```

| Soru | Cevap |
|---|---|
| Harita hangi dosya? | `.world`. İçinde dünya ayarları (gökyüzü, render, yerçekimi, oyun kuralları), streaming ızgarası, veri katmanları |
| Nesneler nerede? | Her yerleştirilmiş nesne kendi küçük dosyasında. İki kişi/ajan aynı haritada çatışmadan çalışır |
| Büyük harita belleğe nasıl sığar? | Cook nesneleri **hücrelere** (ör. 128 m kareler) dağıtır; oyuncunun çevresindeki hücreler arka planda yüklenir, uzaktakiler boşaltılır |
| Uzakta değiştirdiğim şey ne olur? | Kırılan kapı, alınan eşya hücre farkı olarak saklanır; geri dönünce öyle kalmış olur |
| Gece/gündüz, görev öncesi/sonrası? | **Veri katmanları**: nesneler `gece`, `köprü_yıkıldı` gibi katmanlara etiketlenir, oyun bunları açıp kapatır |
| Aynı köyü iki haritada kullanmak? | Köy bir `.scene`; dünyaya **bölüm örneği** olarak yerleştirilecek (henüz yok; şimdilik prefab kullan) |
| Merkezden 20 km uzakta titreme? | **Kayan orijin**: oyuncu uzaklaştıkça dünyanın orijini sessizce kaydırılır |
| Küçük oyun yine tek dosya olabilir mi? | Evet. `.scene` geçerli bir harita olarak kalır |

Arazi (M15), SDF hacimleri (M19) ve uzak görünüm vekilleri (HLOD, M22) aynı hücre ızgarasıyla gelir.

## Oyun kodu bu resmin neresinde?

Harita = veri. Kurallar = **özellik paketleri** ([ADR-0023](../decisions/0023-modular-game-code.md)): her
özellik (savaş, envanter, duman bombası) kendi klasöründe component'leri, sistemleri, Luau script'leri ve
testleriyle durur; özellikler birbirine yalnızca olaylarla ve dışa açtıkları component'lerle bağlanır.
Haritanın oyun kuralları, `.world` dosyasındaki bir kural entity'sinde ayar olarak yaşar (Pong'daki
`pong::Court` gibi).
