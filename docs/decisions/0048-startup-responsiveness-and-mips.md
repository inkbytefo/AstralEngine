# ADR-0048 — Yükleme sırasında pencere tepkisi ve CPU mip üretimi

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `render` (`Renderer::set_busy_callback`, `set_job_system`, `build_mips`), `platform` (`Window::keep_responsive`), `app`

## Bağlam

Açılışta saniyelerce süren yükleme olay döngüsünü bekletiyor, işletim sistemi pencereyi "yanıt vermiyor" olarak işaretliyordu. Ayrıca `the_architect` Debug açılışında 39 dokunun CPU mip üretimi yaklaşık 5.9 s sürüyordu.

## Karar

- `Renderer::set_busy_callback(std::function<void()>)`: Renderer her doku, mesh, materyal veya shader oluşturduğunda/güncellediğinde, oluşturan iş parçacığında çağrılır. App bunu en fazla 50 ms'de bir `Window::keep_responsive()` çağıracak şekilde bağlar (`SDL_PumpEvents`; olaylar bir sonraki `poll`'a kadar kuyrukta kalır). Yüklemeyi hızlandırmaz, ekrana bir şey çizmez.
- `build_mips`: 256 girdili sRGB tablosu ve tam 2:1 hızlı yol; büyük seviyeler `JobSystem` üzerinde. Çıktı eski algoritmayla bayt bayt aynıdır (eski algoritma testte referans olarak tutulur), çünkü mip baytlarının değişmesi tüm golden görüntüleri değiştirir.

## Sonuçlar

- 30 s'lik pencereli Debug soğuk açılışta "yanıt vermiyor" durumu gözlenmedi; değişiklik öncesi aynı ölçüm yapılmadı.
- Doku decode ve yükleme hâlâ ana iş parçacığında ve sıralı (`MaterialLibrary`); öneri: decode + mip'i işlerde ön yükleme.
- Test: `modules/render/tests/mips_reference_test.cpp`, `renderer_busy_test.cpp`; `keep_responsive` için yalnız duman testi (SDL dummy sürücüsü işletim sistemi tepkisini kanıtlayamaz).
