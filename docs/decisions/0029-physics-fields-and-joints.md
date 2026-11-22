# ADR-0029 — Fizik altyapısı ve alanlar (M12)

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** ADR-0010 (Jolt), ADR-0023 (tetik hacmi), roadmap M12. Rüzgâr ve su hem simülasyonu (CPU) hem görüntüyü (GPU: bitki, partikül, bulut, su) sürecek.

## Kararlar

1. **Katmanlar:** 16 nesne katmanı, simetrik çarpışma matrisi (varsayılan hepsi çarpışır). Jolt nesne katmanı = `katman * 2 + hareketli`; iki statik gövde hiç test
   edilmez. Sorgular katman maskesi ve `QueryFilter` alır. Katman adları yok (sayı; oyun sabit tanımlar): ad tablosu bir veri sorunu, fizik çekirdeğinin değil.
2. **Şekiller:** convex hull (hacimsiz noktalar reddedilir), mesh, heightfield, compound. Mesh/heightfield dinamik gövde ve süpürme sorgusu olamaz (açık hata).
   Heightfield gövdeye ortalı, `cell_size` aralıklı.
3. **Yüzey kimliği:** gövde başına `surface` (oyunun tablosu); sorgular ve temaslar taşır. Ses/VFX eşlemesi M14'ün işi.
4. **Eklemler:** Fixed/Point/Hinge/Slider/Distance (Jolt `TwoBodyConstraint`); dünyaya bağlamak için `Body::sFixedToWorld`. Eklemli gövdeler birbiriyle çarpışmaz
   (özel `GroupFilter`: alt grup = gövde indeksi, devre dışı çift tablosu; adımlar arasında değişir), gövde silinince eklemleri silinir.
5. **Alanlar saf işlevdir:** `wind_of(WindDesc, konum, zaman)`, `volume_weight`, `water_surface_height` — durum yok. Gürültü: tamsayı hash (lowbias32) + yumuşak
   değer gürültüsü; `fields.glsl` aynı hash/gürültü/formülleri taşır ve GPU'da CPU ile `2e-4` içinde eşit (CTest, lavapipe'ta koşar). Rüzgâr = küresel + kutu hacimleri
   (ekleyen ya da değiştiren, kenar sönümlü); su = kutu hacmi (üst yüzey), sinüs dalgaları, akış, yoğunluk; örtüşen hacimlerde en derin kazanır.
6. **App:** `WindZone`/`WaterBody` entity'leri her sabit adımın başında `FieldWorld`'e yazılır; `AirDrag`, `Buoyant`, `Airfoil` kuvvetleri adımdan hemen önce uygulanır.
   `Buoyant`: gövdenin altındaki noktada su derinliğinden batan oran; `volume = 0` → kütle/500. Kanat: hücum açısı `atan2(akış·yukarı, −akış·ileri)`, stall sonrası sönme.
7. **Olaylar:** sensör temasları `TriggerEntered/Exited{trigger, other}`, diğerleri `Contact{kind, a, b, point, normal, surface_a, surface_b}`; hepsi `ctx.events`'te kayıtlı
   (script dinler). Tetik hacmi ADR-0023'te vaat edilen yardımcıdır: kural script'te.
8. **Script erişimi arayüzle:** `ScriptPhysics` (raycast, spherecast, overlap, impulse/force/velocity, rüzgâr, su) App tarafından uygulanır; `Astral::Script` Physics'e bağlanmaz.
9. **Görsel interpolasyon** isteğe bağlı (`AppDesc::physics_interpolation`, varsayılan kapalı): Transform simülasyonun bir adım kadar gerisinde; testler ve oyun mantığı için varsayılanı değiştirmemek güvenli.
10. **Ertelenen:** Jolt'u motorun `JobSystem`'ine bağlamak (Jolt'un `JobSystem` arayüzü bariyer semantiği istiyor; kendi havuzu çalışıyor, `PhysicsDesc::threads` ile boyutlanıyor); SixDOF/motorlu eklem;
   bakım gerektiren alan GPU tüketicileri (M14/M16/M17).

## Sonuçlar

- Yüzen kasalar, rüzgârla sürüklenen nesneler, menteşeli köprü, heightfield zemin ve tetik pedi `examples/physics_playground`'da; görüntü testi `physics_playground`.
- Alan işlevleri değişirse `fields.glsl` birlikte değişmeli (parite testi bunu zorlar).
