# ADR-0050 — Ray-traced yansımalarda pürüzlülük eşiği

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `render` (`RenderSettings::max_reflection_roughness`, `reflection_roughness_fade`, `rt_lighting.comp`)
- **İlişkili:** [ADR-0011](0011-hybrid-ray-traced-lighting.md)

## Bağlam

`rt_lighting.comp` her opak pikselde bir yansıma ışını atıyordu; mat duvarlarda bile. `the_architect`'te `rt.lighting` RT High'da kare süresinin yaklaşık %60'ıydı (10.43 ms).

## Karar

- `max_reflection_roughness` (varsayılan 1.0 = her şeyi izle, görüntü değişmez) ve `reflection_roughness_fade` (varsayılan 0.15). Eşiğin üstündeki pikseller yansıma ışını atmaz, aynasal katkıyı baskın lob yönündeki DDGI probundan alır. Sky IBL kullanılmadı: kapalı odalara gökyüzü ışığı sızdırırdı.
- Geçiş aralığında seçim stokastiktir ve denoiser tarafından yumuşatılır. Anizotropik ve örneklenen clearcoat pikselleri her zaman izlenir.

## Sonuçlar

- `the_architect`, 1080p, Release: eşik 0.5 → 8.04 ms (−%23), 0.35 → 7.54 ms (−%28), 0.65 → kazanç yok. Varsayılan çıktı değişmedi (`rt_showcase` capture'ı bayt bayt aynı).
- Varsayılan hâlâ 1.0; oyunlar eşiği kendileri seçmeli.
