# ADR-0030 — Refaktör kuralları ve boyut bütçesi (M13.5)

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** 2026-11-22 mimari incelemesi ([roadmap.md → M13.5](../roadmap.md)). Katmanlar, değişmez kurallar ve testler sağlam; ancak birkaç dosya her fazda büyüyen birer "toplama noktası" hâline geldi:
  - `modules/render/src/renderer.cpp`: 1835 satır, `Renderer::Impl` 58 veri üyesi (metotlarla 69 üye), `render_ray_traced` 396 satır, `Renderer::render` 206 satır.
  - `modules/app/src/app.cpp`: 1459 satır, `App::Impl` 55 veri üyesi (106 satır), `frame()` 146 satır, `draw_debug_overlays` 114 satır, `App` kurucusu 111 satır; `Context` 31 veri üyesi ve ~25 metotla her fazda büyüyor.
  - Diğer büyük dosyalar ve fonksiyonlar: `physics.cpp` (911 satır, `create_joint` 103 satır), `bindings.cpp` (831 satır), `scene.cpp` (789 satır), `device.cpp` (721 satır, `Device::Impl` 46 veri üyesi), `debug_ui.cpp` (`draw_panels` 268 satır), `cook.cpp` (`cook_directory` 260 satır), `api.cpp` (`generate_script_api` 154 satır).
  - Bu toplama noktaları paralel geliştirmede (görev başına worktree → master) birleştirme çakışmalarına ve yapay zekâ ajanları için yüksek bağlam maliyetine yol açıyor. İlk Windows/MSVC derlemesi ayrıca CI'ın yakalamadığı dört hata (`0be42b0`) ve derleyiciye bağlı eksik `#include` sorunları ortaya çıkardı.

## Kararlar

1. **Boyut bütçesi (hedef sınırlar):**
   - `modules/*/src`, `modules/*/include`, `tools/`, `examples/*/src`, `templates/*/src` altındaki (testler ve `_deps` hariç) her C++ dosyası **≤ 600 satır**.
   - Her fonksiyon gövdesi **≤ 80 satır**.
   - Her `struct`/`class` (`Impl` yapıları dâhil) **≤ 25 veri üyesi** (satır sayısı değil, veri üyesi bildirim sayısı).

2. **"God class" tanımı ve tek sorumluluk ölçütü:**
   - Bir sınıf veya dosya **birden fazla nedenle değişiyorsa** (tek sorumluluk ilkesi ihlali) god class sayılır ve sorumluluğa göre bölünür (ör. `Renderer`: kaynak store'ları, render pass'leri, raster/RT yolları; `App`: Render/Physics/Audio/Script/World köprüleri, `DevTools`, `FrameLoop`).
   - Bölme hiçbir zaman yalnızca satır sayısı tutsun diye yapılmaz; tek bir uyumlu algoritma yapay parçalara ayrılmaz.

3. **Tutarlı ayrıştırıcı (cohesive parser) istisnası:**
   - Tek bir sorumluluğu olan uyumlu bir ayrıştırıcı veya durum makinesi sırf boyutu yüzünden bölünmez. Örnekler: `modules/core/src/json.cpp` içindeki `Parser` (216 satır, 4 veri üyesi) ve `modules/shader/src/ashader.cpp` içindeki `Parser` (167 satır, 3 veri üyesi).
   - Bütçe sınırlarını bilinçli olarak aşan her yer `tools/size_budget.json` dosyasında (`path`, `kind`, `name`, `reason`) gerekçesiyle listelenir. M13.5 R1–R3 refaktörleri indikçe geçici istisnalar kaldırılır; istisna listesi yalnızca küçülür.

4. **Davranış koruyan refaktör kuralları:**
   - M13.5 boyunca her adım saf yapısaldır: aynı çıktı, aynı kare aşama sırası, aynı dosya biçimleri, aynı log iletileri ve aynı sayısal sonuçlar korunur.
   - Public API (`modules/*/include/**`) kaynak uyumlu kalır; yalnızca görev metninde açıkça izin verilen API değişiklikleri yapılır ve eski çağrılar en az bir faz `[[deprecated]]` sarmalayıcıyla yaşar.
   - Refaktör sırasında fark edilen gerçek hatalar aynı adımda düzeltilmez; raporda "bulundu, düzeltilmedi" olarak kayda geçer ve ayrı görevde ele alınır.
   - Performans gerilemesi kabul edilmez: kare veya entity başına sıcak döngülere yeni heap tahsisi, sanal çağrı, `std::function` veya gereksiz veri kopyası eklenmez; düz yapılar referans/`std::span` ile aktarılır.

5. **Önce karakterizasyon testi, sonra taşıma:**
   - Mevcut davranışı testle sabitlenmemiş hiçbir kod taşınmaz. Önce eski kodda geçen bir karakterizasyon testi (doctest; kare aşama sırası, `RenderStats` sayaçları, render graph pass listesi, cook çıktısı vb.) eklenir, ardından refaktör bu testi yeşil tutarak yapılır.
   - Yeni oluşturulan iç bileşenler (servisler, store'lar, pass'ler) mümkün olan her yerde başsız birim testleriyle doğrulanır.

6. **Otomatik denetim ve kabul kapısı (gate):**
   - `tools/check_sizes.py` (yalnızca Python 3 standart kütüphanesi, Windows + Linux) yorumları ve ham dizgiler dâhil string sabitlerini ayıklayıp bütçe ihlallerini raporlar.
   - Ek (2026-11-25): istisna girdisi isteğe bağlı `max` taşır; istisnalı değer bu üst sınırı aşarsa yeniden `[OVER]` sayılır (istisna sessizce büyüyemez). Hiçbir bulguyla eşleşmeyen girdi `[STALE]` olarak raporlanır ve `--strict`'i düşürür (düzeltilen yerin istisnası silinir). Sınırın `--near` oranına (varsayılan 0,9) ulaşan dosya/fonksiyon/yapılar `[NEAR]` olarak listelenir; bu liste kapıyı düşürmez, bölme planı içindir. `[OVER]` satırları türe göre bölme ipucu verir.
   - CTest `code_size_check` varsayılan olarak uyarır (`ASTRAL_SIZE_BUDGET_STRICT=OFF`); M13.5 R1–R3 tamamlandığında `--strict` (`ASTRAL_SIZE_BUDGET_STRICT=ON`) sert kapı olarak açılır. CI'da Linux GCC işi her koşuda `--summary` özetini (en çok 3 satır) basar.
   - Include hijyeni için CI'da Clang 18 üzerinde engelleyici olmayan (`continue-on-error: true`) bir `clang-tidy` `misc-include-cleaner` işi yalnızca motor/örnek kaynaklarını denetler.
   - Her refaktör adımının kabul kapısı: tüm birim ve bütünleştirme testleri kendi kodumuzda 0 derleyici uyarısıyla geçer ve lavapipe görüntü testleri (golden) **bayt bayt aynı** kalır.

## Sonuçlar

- Yeni god class'ların ve aşırı uzun fonksiyonların sessizce büyümesi otomatik olarak görünür olur; M13.5 sonrası `--strict` ile engellenir.
- `tools/size_budget.json`, R1–R3 işlerinin ilerleyişini nesnel bir borç listesi olarak izler.
- Karakterizasyon testleri + bayt bayt golden kapısı, büyük yapısal bölünmelerde (Renderer, App, Physics, Script, World, Assets, GPU) davranış kaymasını önler.
- Include hijyeni kontrolü başlangıçta engelleyici değildir; mevcut eksik/fazla include'lar temizlendikçe (R4) sertleştirilebilir.
