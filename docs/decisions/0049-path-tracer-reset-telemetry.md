# ADR-0049 — Path tracer sıfırlama nedeni ve sahne dondurma

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `render` (`RenderStats::path_tracer_reset`, `RenderSettings::path_tracer_freeze_scene`)

## Bağlam

Path tracer, sahne imzası (ışıklar, örnekler, gökyüzü…) değiştiğinde birikimi sıfırlar. Ekran dışındaki bir ışığın her kare 0.0005 kayması birikimi her kare sıfırlıyor ve bunun nedeni hiçbir yerde görünmüyordu.

## Seçenekler

- **İmzayı nicemlemek (quantize):** küçük değişiklikleri yok sayar ama tarafsız (unbiased) referans sözleşmesini bozar; reddedildi.
- **Neden bildirimi + isteğe bağlı dondurma (seçilen).**

## Karar

- `PathTracerReset` nedenleri: None, Resize, Geometry, Lights, Environment, Time, Camera, Resources, Settings, Manual, History. Sahne nedenleri kameradan önce kontrol edilir; beklenmeyen bir kayma kamera hareketinin arkasına saklanmaz.
- Sahne imzası geometri, ışık (+güneş) ve ortam hash'lerine ayrıldı; hepsi tam (nicemlenmemiş).
- `path_tracer_freeze_scene = true`: sahne değişse de birikim sürer (sonuç değişen durumların ortalamasıdır; değişikliğin görünmez olduğu bilinen capture'lar için). Kamera, çözünürlük, kaynak ve ayar değişiklikleri yine sıfırlar.

## Sonuçlar

- `Time` nedeni test edilmedi (zaman güdümlü özel shader gerektirir).
- Test: `modules/render/tests/m13_pbr_test.cpp` (her neden, 0.0005 ışık kayması, dondurma).
