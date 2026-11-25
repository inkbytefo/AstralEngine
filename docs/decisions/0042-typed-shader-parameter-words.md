# ADR-0042 — Shader parametrelerinde tipli word ABI

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `shader` (`.ashader` default'ları), `render` (`evaluate_uniforms`, `MaterialDesc`), `app`/`assets` tüketicileri, MSR AS13
- **İlişkili:** [Master Stabilization Roadmap](../status/master-stabilization-roadmap.md), [Assets & Shader denetimi](../status/assets-shader-deep-audit-2026-11-25.md)

## Bağlam

GPU'daki parametre bloğu zaten 32-bit `uint` word dizisidir (`astral_pu`, `astral_pf = uintBitsToFloat`). Ama CPU tarafı `std::vector<float>` taşıyordu ve `int` uniform GLSL'de `int(astral_pf(o))` olarak float'tan geri çevriliyordu. float32 24-bit mantis taşıdığı için 16777217 → 16777216 oluyor, aralık dışı değerlerde GLSL dönüşümü tanımsızdı. Default'lar `strtof` ile okunuyordu.

## Seçenekler

- **Float konteynerde int bitlerini `bit_cast` ile taşımak:** NaN bit desenleri kopyalamada kanonikleşebilir; okuması yanıltıcı.
- **Word konteyneri `uint32_t` (seçilen).**

## Karar

- `UniformValues::params` ve `MaterialDesc::params` `std::vector<std::uint32_t>` olur. Kodlama: `float`/`vecN` → IEEE bitleri, `int` → two's complement, `bool` → 0/1, sampler → bindless index (değişmedi).
- Üretilen GLSL: `int` → `int(astral_pu(o))`, `bool` → `astral_pu(o) != 0u`; float yolları değişmez.
- `ShaderUniform::default_value` `std::vector<double>` olur; `.ashader` sayıları `strtod` ile okunur (int32 ve float tam temsil edilir).
- Ortak doğrulayıcı: `int` değer sonlu, tamsayı ve int32 aralığında olmalı; `bool` için 0/1/true/false. Hatalı default parse hatasıdır; hatalı materyal override'ı uyarı verip default'u kullanır (mevcut "wrong type" davranışı).
- Shader cook çıktısı üretilen GLSL içerdiğinden `astral-cook 7` artışına dahildir (ADR-0041 ile aynı artış).

## Sonuçlar

- int32 parametreler tam aralıkta korunur; float parametrelerin GPU görüntüsü bit bit aynıdır.
- `MaterialDesc::params` ve `ShaderUniform::default_value` public tipleri değişir; depodaki tüm tüketiciler (material library, asset manager, render bridge hash, editor inspector) aynı değişiklikte güncellenir.
- Test: `modules/render/tests/msr_regression_test.cpp` ve shader testlerinde AS13 senaryoları (int32 min/max, 2^24±1, negatif, kesirli/sonsuz red, bool, karışık layout, üretilen GLSL).
