# ADR-0039 — Astral Terrain temeli

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-24
- **Kapsam:** M15’in ilk uygulanabilir terrain sürümü
- **İlişkili kararlar:** [ADR-0018](0018-terrain-and-sdf-world.md), [ADR-0021](0021-world-and-level-system.md), [ADR-0036](0036-world-authoring-and-level-instances.md), [ADR-0038](0038-physics-api-v2.md)
- **Uygulama:** [Görev planı](../archive/legacy/superpowers/plans/2026-11-24-m15-terrain-foundation.md)

## Veri ve koordinatlar

`.aterrain` sürümlü bir manifesttir. Örnek aralığı, tile başına örnek sayısı, yükseklik aralığı ve tile adresleri manifestte bulunur. Tile adresi işaretli tam sayı XZ koordinatıdır. Her Y değeri metre cinsindendir. Örnekler X yönünde artarak, satırlar +Z yönünde artarak dizilir. Başlangıç profili 65 × 65 örnek ve 1 m örnek aralığıdır. 64 aralık, 64 × 64 m tile verir. Cooker, 16 bit gri tonlu kaynak heightmap’i belirtilen yükseklik aralığına dönüştürür ve tile başına sürümlü bir veri kaydı üretir. Komşular ortak sınır örneklerini aynı kaynak değerinden alır. Cook sırasında boyut, biçim, sonlu değer, yinelenmiş adres ve ortak kenar tutarlılığı doğrulanır.

Kaynak manifest örneği:

```json
{
  "format": "astral.terrain",
  "version": 1,
  "samples": 65,
  "spacing_m": 1,
  "min_height_m": -10,
  "max_height_m": 90,
  "tiles": [
    {"x": -1, "z": 0, "file": "height/west.png"},
    {"x": 0, "z": 0, "file": "height/east.png"}
  ]
}
```

Dosya yolları asset köküne göredir. Kenar başına örnek sayısı 5 ile 1025 arasında, ikinin kuvveti artı bir olmalıdır. Cook edilmiş tile, `ATH1` imzasından sonra little-endian 16 bit örnekler taşır. Kaynak ve paket okuyucuları aynı metre dönüşümünü kullanır. Cooker, binary manifestteki dosya yollarını `<manifest>.tiles/<x>_<z>.aheight` olarak yazar.

`TerrainRef` asset yolunu tutar. `Transform` ilk sürümde ötelemeyi belirler. Döndürülmüş, ölçeklenmiş veya parent’lı terrain için açık doğrulama hatası verilir. Tile adresi ve örnek koordinatı floating origin kaymalarında sabit kalır. Sahneye mesh ve collider handle’ları yazılmaz.

## Runtime

- **Render.** CPU işi mevcut `MeshData` yoluna vertex, normal ve indeks üretir. Tile kenarlarındaki normaller komşu örneklerden hesaplanır. Kamera mesafesi ve hysteresis, tile başına LOD seçer. Komşu LOD farkı en fazla bir seviyedir. İnce kenarın indeksleri kaba kenara bağlanır. Hazır mesh ve kenar kombinasyonu birlikte yayınlanır. Raster ve ışın izleme mevcut mesh hattını kullanır. LOD değişiminde BLAS maliyeti ayrıca ölçülür.
- **Fizik.** Yüklü tile için Jolt statik heightfield, render ile aynı kaynak örneklerden hazırlanır. Physic tile’ın çözünürlüğü kamera LOD’undan bağımsızdır. Backend’in merkezli XZ yerleşimi, hücre üçgen diyagonali ve yükseklik sıkıştırması test edilir. Görsel ve fizik yüzeyi için tolerans fixture’da ölçülüp kayda alınır.
- **Streaming.** Mevcut `StreamingSource` konumları kullanılır. Görsel yükleme mesafesi ve collider ön yükleme mesafesi ayrı ayarlanır. Fizik kapsaması, karar verilen hareket alanından geriye düşerse fizik kullanan aktörün geçişi collider hazır olana kadar ertelenir. Yükleme hatası tanı üretir. I/O ve mesh üretimi arka planda yürür. Render/fizik kayıt ve silme işlemleri mevcut sahip iş parçacığı ve adım sınırlarında gerçekleşir. İş sonucu, terrain örneği ve tile isteği hâlâ geçerliyse uygulanır.
- **Materyal.** İlk sürüm tek PBR materyali ve metre bazlı XZ koordinatları kullanır. UV’ler global örnek koordinatından üretilebilir. Materyal dalındaki dünya projeksiyonu ve triplanar, birleştiğinde aynı ölçek sözleşmesini kullanır.

## M15 ile ilişki

Bu ADR, [ADR-0018](0018-terrain-and-sdf-world.md) içindeki M15’in ilk render uygulamasını CPU tile mesh olarak aşamalandırır. GPU clipmap ve görüşten bağımsız RT geometrisi kararı profil ölçümünden sonra yeniden değerlendirilir. Katman boyama, eğim bazlı triplanar, delikler, fırçalar ve bitki yerleştirme M15 devam işlerine kalır. Yerel SDF hacimleri M19 kapsamında ele alınır.
