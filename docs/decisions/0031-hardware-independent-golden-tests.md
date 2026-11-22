# ADR-0031 — Donanımdan bağımsız görüntü testleri

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** M8.6 görüntü testleri (`tests/golden/`) referans PNG'lerini Linux CI'da Mesa lavapipe (yazılımsal Vulkan) üzerinde üretir. Gerçek donanım GPU'larında (ör. NVIDIA GeForce RTX 3060) sürücü/donanım farkları — özellikle RT ışın kesişimi, doku filtreleme ve kayan nokta yuvarlama — nedeniyle beş sahne (`scene_viewer`, `open_world`, `open_world_night`, `open_world_nudged`, `shader_showcase`) mevcut piksel-bazlı eşiği (%0,5 kötü piksel, kanal farkı > 8) aşarak sürekli kırmızıya düşüyordu (%1,8–38,4 kötü piksel, maks. fark 22–141). Sürekli yanlış alarm veren testler yerel geliştirmede güveni bitirir.

## Kararlar

1. **Yakalamayı üreten cihazın kimliğini doğrudan raporla:** `gpu::Device::adapter_info()` seçilen fiziksel cihazın adını, `vendor_id`, `device_id`, Vulkan `driver_id`, `driver_name` ve `AdapterType` (`discrete`, `integrated`, `virtual`, `cpu`, `other`) bilgilerini verir (public header'larda Vulkan tipi yok). `run_game()` yakalama (`--capture`) sırasında aynı cihazdan `capture device: name=... | vendor=0x... | ...` satırını stdout'a yazar. Böylece sınıflandırma ayrı bir sorgu aracı yerine tam olarak görüntüyü üreten Vulkan cihazına dayanır.
2. **İki CTest etiketi, tek referans seti:**
   - **`golden` (`golden_<sahne>`):** yalnızca Mesa lavapipe/llvmpipe (`driver_id == 13` veya sürücü/cihaz adında `llvmpipe`/`lavapipe`) üzerinde mevcut piksel-bazlı sıkı toleransla (varsayılan eşik 8, maks. %0,5) karşılaştırma yapar. Diğer cihazlarda (donanım GPU veya başka yazılımsal sürücü) CTest `SKIP_REGULAR_EXPRESSION` ile nedeni belirtilerek **atlanır** (`Skipped`). CI (Linux + lavapipe) bu etiketle zorunlu kapı olmayı sürdürür.
   - **`golden-hw` (`golden_hw_<sahne>`):** yalnızca yazılımsal olmayan donanım GPU'ları üzerinde çalışır; lavapipe ve diğer CPU/yazılımsal sürücülerde (ör. SwiftShader) **atlanır**.
   - Vulkan cihazı hiç yoksa her iki etiket de `SKIP: no GPU / Vulkan driver` ile atlanır.
3. **Donanım karşılaştırma metriği (bağımlılıksız C++):** `tests/golden/image_compare.cpp` içine iki parçalı algısal metrik eklendi:
   - **Ortalama parlaklık SSIM'i (`mean_luminance_ssim`):** Rec.709 parlaklığı (`0.2126 R + 0.7152 G + 0.0722 B`) üzerinde 8×8 kayan pencereyle (adım 1, ayrılabilir kutu toplamı, Wang et al. 2004 sabitleri `C1 = (0.01·255)²`, `C2 = (0.03·255)²`). Yapısal bozulmayı, siyah/boş kareyi ve %10 kaymayı yakalar.
   - **Güçlü farklı piksel oranı sınırı:** herhangi bir RGB kanalı `strong_pixel_threshold` değerinden fazla sapan piksellerin oranı `max_strong_bad_fraction` ile sınırlanır. Parlaklık SSIM'i tek başına R↔B kanal değişimini kaçırabilir (parlaklık çoğunlukla yeşile bağlıdır); bu ikinci koşul renk bozulmalarını yakalar.
   - **Kalibre edilmiş varsayılan eşikler:** `min_ssim = 0.95`, `strong_pixel_threshold = 32`, `max_strong_bad_fraction = 0.025` (%2,5). RTX 3060 vs. lavapipe referans ölçümlerinde 12 sahnenin en kötüsü `scene_viewer` (`SSIM ≈ 0.977`, `>32` oranı `%1,56`) ve `shader_showcase` (`SSIM ≈ 0.979`, `>32` oranı `%0,78`) iken, diğer 10 sahnede `>32` oranı `%0,00` ve `SSIM ≥ 0.981`'dir. Buna karşılık siyah kare (`SSIM < 0.01`), R↔B değişimi (`scene_viewer`'da `>32` oranı `%40`) ve %10 kayma (`SSIM ≤ 0.92`) güvenle yakalanır (`astral_golden_tests` birim testiyle sabitlenmiştir).
4. **RT sahnelerinin determinizmi:** RT shader'ları (`rt.glsl`, `rt_lighting.comp`, `ddgi_trace.comp`) tohumu `(piksel/prob, kare_indeksi, sabit_tuz)` üzerinden deterministik üretir; `run_game()` yakalamada sabit 60 Hz adımla sabit kare sayısı (`scene_viewer` 24, `rt_showcase` 32, `open_world` 40, `shader_showcase` 60) biriktirir. Bu nedenle motorun render koduna dokunulmadı ve yerel yakalamalar taban çizgisiyle bayt bayt aynı kaldı.
5. **Paralel CTest güvenliği ve çıktı yolları:** Çıktı yolları değişmedi (`tests/golden/out/<sahne>.ppm` ve hata hâlinde `<sahne>.diff.png`). Aynı sahnenin `golden_<sahne>` ve `golden_hw_<sahne>` testleri aynı `RESOURCE_LOCK golden_scene_<sahne>` kilidini paylaşır; böylece `ctest -L golden -j N` (regex her iki etiketi de seçer) çalıştırıldığında iki test aynı dosyaya asla eşzamanlı yazmaz, farklı sahneler ise paralel koşmaya devam eder.
6. **Referans güncelleme güvenliği:** `ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$'` yalnızca doğrulanmış lavapipe cihazında referans yazar. Donanım yolunda (`--mode hardware`) hem `RunGolden.cmake` `ASTRAL_UPDATE_GOLDEN` ortam değişkenini temizler hem `astral-golden` `--update` bayrağını reddeder ve güncelleme yoluna asla girmez.

## Sonuçlar

- Linux CI (lavapipe): 12 `golden` testi koşup geçer, 12 `golden-hw` testi atlanır.
- Geliştirici makinesi (donanım GPU): `ctest --preset debug -L '^golden$'` 12 atlama, `ctest --preset debug -L '^golden-hw$'` 12/12 geçme verir; `ctest -L golden -j N` yarış koşulu olmadan yeşil biter.
- Referans PNG'leri kazara donanım GPU çıktısıyla ezilemez; yeni referanslar yalnızca lavapipe ortamında güncellenir.
