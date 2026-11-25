# ADR-0041 — Json'da kayıpsız 64-bit tamsayı

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `core` (`Json`, AJB), `world` (reflection field'ları, typed serialize), MSR C05
- **İlişkili:** [Master Stabilization Roadmap](../status/master-stabilization-roadmap.md), [Core & ECS denetimi](../status/core-ecs-deep-audit-2026-11-25.md)

## Bağlam

`Json` sayıları yalnız `double` olarak saklıyordu. `int64_t`/`uint64_t` constructor'ları double'a çeviriyor, 2^53 üstündeki ardışık tamsayılar aynı değere düşüyordu (kalıcı kimlikler, `stable_hash` id'leri, 64-bit field'lar). Reflection'da 8-byte field'lar da double üzerinden okunup yazılıyordu; `store_clamped<int64/uint64>` üst sınırı `(double)max` = 2^63 (veya 2^64) olarak hesaplayıp sonra cast ettiği için tanımsız davranıştı. AJB zaten `TagInteger` (zigzag varint) taşıyordu ama okuyucu değeri double'a çeviriyordu.

## Seçenekler

- **Büyük değerleri string olarak yazmak:** format/schema'ya özel durum ekler, her okuyucu ayrıca çözmek zorunda kalır.
- **Ayrı `Type::Integer`:** `is_number()` kullanan 90+ çağrı yerini ve `type()` switch'lerini kırar.
- **İç temsil `double | int64 | uint64`, dış tip `Number` (seçilen).**

## Karar

- `Json` sayısal değeri içeride `double`, `std::int64_t` veya `std::uint64_t` olarak tutar. `type()` üçü için de `Type::Number` döner; `as_number()` her zaman double verir (eski davranış).
- Yeni okuyucular: `as_int64()` ve `as_uint64()` (`std::optional`); değer hedef tipte tam temsil edilemiyorsa `nullopt`. Double tutan sayı ancak tam ve aralıkta ise döner. `is_integer()` tamsayı temsilini bildirir.
- Constructor'lar: `int`, `int64_t`, `uint32_t` → int64; `uint64_t` → int64'e sığıyorsa int64, değilse uint64; `float`/`double` → double.
- Metin parser: kesir ve üs içermeyen, int64 veya uint64'e sığan literal tamsayı olarak saklanır; diğerleri double. `dump` tamsayıyı tam yazar.
- `operator==` sayıları değerle karşılaştırır (`Json(1) == Json(1.0)`); kesin karşılaştırma int/uint/double karışımında taşma olmadan yapılır.
- AJB: `TagInteger` int64 tamsayıyı kayıpsız taşır; int64 aralığını aşan uint64 için yeni `TagUInt64` (varint). Double değerler eskisi gibi tam ve |v| < 2^53 ise `TagInteger`, değilse `TagNumber`. Eski AJB dosyaları aynen okunur; yeni dosyadaki `TagUInt64` eski okuyucuda "unknown tag" hatasıdır, bu yüzden cook sürümü `astral-cook 6` → `astral-cook 7` (her şey yeniden cook edilir).
- Reflection: 8-byte int/uint field'ları `field_to_json` / `field_from_json` yolunda double'a uğramaz. `store_clamped` sınırı tamsayı tarafında karşılaştırır; double ≥ 2^63 (2^64) doğrudan max'a doyurulur.

## Sonuçlar

- Metin ve AJB roundtrip'i 64-bit tamsayıları korur; mevcut metin/AJB verisi migration gerektirmez.
- Luau sayıları double'dır: script'e verilen 64-bit değerler 2^53 üstünde hâlâ kayıplıdır (`world.md` "Bilinen sınırlar").
- Test: `modules/core/tests/msr_regression_test.cpp` ve `modules/world/tests/msr_regression_test.cpp` içindeki C05 senaryoları (2^53±1, int64 min/max, uint64 max, metin/AJB/field roundtrip).
