# ADR-0051 — Planar ve triplanar projeksiyonda doku yönü

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `render` (`shaders/include/astral/projection.glsl`: `projection_uv`, `projected_normal`)

## Bağlam

X baskın yüz `(y, z)` örnekliyordu; doku ±X duvarlarında 90° dönüyordu (tuğla sıraları dikey çıkıyordu), ±Z duvarlarında doğruydu.

## Karar

Her dikey yüzde doku V'si dünya yukarısına (+Y), U'su dışarıdan bakıldığında ekranın sağına bakar: +Z: +X, −Z: −X, +X: −Z, −X: +Z. Zemin ve tavan (Y yüzleri) değişmedi. Normal haritası teğet çerçevesi aynı tabloyu kullanır. Raporda önerilen `(z, y)` eşlemesi ±X duvarlarından birini aynalayacağı için seçilmedi.

## Sonuçlar

- `PlanarYZ` aynı yeni yönü kullanır. Hiçbir golden sahne X duvarında projeksiyon kullanmadığı için golden değişmedi; `examples/material_workflow/stone_triplanar.material` görünümü değişir.
- Yalnız raster/G-buffer yolu test edildi; RT ve path tracer aynı yardımcıyı kullanır ama ayrıca test edilmedi.
