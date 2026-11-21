# ADR-0005: Kendi ECS'imiz — EnTT kullanılmaz

**Durum:** Kabul · 2026-11-21

**Karar:** `Astral::World` kendi entity/component sistemini içerir. Hazır ECS kütüphanesi
kullanılmaz. Hedef: archetype/chunk tabanlı, cache-dostu, JobSystem ile paralel sorgulanan,
okunaklı ve kullanması kolay bir API.

**Neden:** Bellek düzeni, paralellik ve API üzerinde tam kontrol; dış bağımlılık sayısı azalır;
motorun en sıcak verisi bizim tasarımımız olur.

**Sonuçlar:** Daha fazla kod ve test bizim sorumluluğumuzda. Eski AstralEngine reposundaki
kod, yeniden yazım kapsamında doğrudan alınabilir.
