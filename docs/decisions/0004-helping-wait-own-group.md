# ADR-0004: Yardımlı wait() sadece kendi grubunu çalıştırır

**Durum:** Kabul · 2026-11-21

**Bağlam:** `wait()` beklerken kuyruktaki herhangi bir işi çalıştırıyordu. İnceleme şunu
buldu: R, çocuklarını beklerken kuyruktaki PP'yi çalıştırır; PP de R'nin grubunu bekler.
R yığında PP'nin altında askıda olduğu için grup hiç bitmez → kilitlenme.

**Karar:** `wait(counter)` sadece `counter`'a ait işleri çalıştırır; yoksa uyur.

**Sonuçlar:** Kilitlenme yok (regresyon testi var). Bekleyen thread başka grupların işine
yardım etmez; tam verim için ileride work-stealing + bağımlılık grafı (fiber'lar düşünülebilir).
