# Platform (`Astral::Platform`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core, Astral::Gpu (public); SDL3 (static, `release-3.2.26`), Vulkan başlıkları (private) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

SDL3 üzerinde ince bir işletim sistemi katmanı: pencere oluşturma, olay döngüsü, klavye/fare/gamepad/metin
girişi, pano, imleç şekli ve çalıştırılabilir dosyanın klasörü. Pencereyi GPU'ya Vulkan yüzeyi olarak
verir (`Window::surface_source`). Ses, dosya sistemi soyutlaması, pencere durumu (tam ekran, konum), dokunmatik
ve çoklu pencere **kapsam dışıdır**. Modül SDL3'ü `FetchContent` ile statik olarak derler
(`modules/platform/CMakeLists.txt:14`, `:28`); kullanıcı kodu SDL tiplerini görmez (public header'larda SDL yok).

## Public API

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Window::create(WindowDesc)` | Pencere açar; video sürücüsü yoksa `nullptr` | `modules/platform/include/astral/platform/window.hpp:39` |
| `WindowDesc` | başlık, boyut (1280x720), `resizable`, `hidden` | `modules/platform/include/astral/platform/window.hpp:27` |
| `Window::poll(Input&)` | OS olaylarını `Input`'a işler; kapanış istenince `false` | `modules/platform/include/astral/platform/window.hpp:45` |
| `Window::keep_responsive()` | Olayları tüketmeden OS'e cevap verir (uzun yüklemede "yanıt vermiyor"u önler) | `modules/platform/include/astral/platform/window.hpp:48` |
| `Window::size()`, `pixel_density()` | Piksel cinsinden çizilebilir boyut; yüksek DPI oranı | `modules/platform/include/astral/platform/window.hpp:53`, `:63` |
| `Window::set_mouse_captured` | Göreli fare kipi (FPS kamerası) | `modules/platform/include/astral/platform/window.hpp:57` |
| `Window::set_text_input` | Yazılan metni `Input::text()`'e yönlendirir | `modules/platform/include/astral/platform/window.hpp:60` |
| `Window::clipboard_text` / `set_clipboard_text`, `set_cursor(CursorShape)` | Pano ve 9 imleç şekli | `modules/platform/include/astral/platform/window.hpp:66`, `:69` |
| `Window::supports_vulkan`, `surface_source` | GPU'ya yüzey ve instance eklentileri verir | `modules/platform/include/astral/platform/window.hpp:72`, `:76` |
| `Input` | Kare başına durum: `down/pressed/released` (klavye, fare, gamepad), eksenler, fare konum/delta/scroll, metin | `modules/platform/include/astral/platform/input.hpp:37` |
| `Input::begin_frame()` | Kenarları (pressed/released), delta, scroll ve metni sıfırlar; `Window::poll` çağırır | `modules/platform/include/astral/platform/input.hpp:40` |
| `Input::axis(Key, Key, Key, Key)`, `wasd()` | Dört tuştan -1..1 eksen | `modules/platform/include/astral/platform/input.hpp:75`, `:77` |
| `Input::clear_keyboard` / `clear_mouse` | Debug UI girişi aldığında oyundan gizler | `modules/platform/include/astral/platform/input.hpp:52`, `:53` |
| `Key`, `MouseButton`, `GamepadButton`, `GamepadAxis` | Fiziksel (scancode) tuş/düğme numaralandırmaları | `modules/platform/include/astral/platform/input.hpp:12`, `:26`, `:29`, `:33` |
| `executable_directory()` | Çalıştırılabilirin klasörü (`.apak` yanında aranır) | `modules/platform/include/astral/platform/paths.hpp:9` |

## Nasıl çalışır

**Giriş akışı.** `Input` saf veridir: SDL'e bağımlı değildir ve doğrudan test edilebilir. `Window::poll` önce
`input.begin_frame()` çağırır, sonra `SDL_PollEvent` ile tüm kuyruğu boşaltıp `set_key`, `set_mouse_button`,
`add_mouse_motion`, `add_scroll`, `add_text`, gamepad ayarlayıcılarını çağırır
(`modules/platform/src/window.cpp:140-199`). Kenar mantığı `Input::apply`: durum değişince `pressed`/`released`
bayrağı kurulur; işletim sistemi tuş tekrarı (`down` iken tekrar `down`) yeni `pressed` üretmez
(`modules/platform/src/input.cpp:9-13`, test `modules/platform/tests/input_test.cpp:29`). Kenarlar bir sonraki
`begin_frame`'e kadar yaşar; yani `poll` kare başına bir kez çağrılmalıdır. Headless/offscreen `App`'te pencere
olmadığından `App` kendisi `begin_frame` çağırır (`modules/app/include/astral/app/app.hpp:256`).

**Tuş eşlemesi.** SDL **scancode** (fiziksel konum) kullanılır, düzen duyarlı keycode değil
(`modules/platform/src/window.cpp:16`). Eşlenmeyen tuşlar `Key::Unknown` olur ve `set_key` tarafından yok sayılır
(`modules/platform/src/input.cpp:61`). Fare için yalnızca sol/sağ/orta düğme var; X1/X2 yok sayılır
(`window.cpp:86-94`).

**Gamepad.** SDL gamepad alt sistemi `Window::create` içinde başlatılır (`window.cpp:117`). Yalnızca **ilk
bağlanan** gamepad açılır (`window.cpp:163-168`); düğmeler Xbox düzenine eşlenir, sol/sağ çubukların Y ekseni
"yukarı pozitif" olacak şekilde ters çevrilir (`window.cpp:186`, `:188`), eksenler `[-1,1]`'e kırpılır
(`input.cpp:52`). Ölü bölge, titreşim ve çoklu gamepad yoktur. `App` tarafında eylem eşlemesi
`modules/app/src/input_actions.cpp` içindedir (bu modülün dışı).

**Metin girişi.** `set_text_input(true)` `SDL_StartTextInput` çağırır (`window.cpp:223-227`); `TEXT_INPUT` olayı
`Input::add_text` ile UTF-8 olarak birikir (`window.cpp:162`). `App` içinde yalnızca DevTools/ImGui metin alanları
için açılıp kapanır (`modules/app/src/services/dev_tools.cpp:71`).

**Pencere ömrü ve headless.** `Window::create` önce `SDL_WINDOW_VULKAN` ile pencere açar; başarısız olursa
(ör. SDL "dummy" sürücüsü) Vulkan bayrağı olmadan yeniden dener ve `vulkan=false` işaretler
(`window.cpp:119-130`). Bu durumda `surface_source()` eklenti ve `create_surface` doldurmaz, yalnızca
`drawable_size` verir (`window.cpp:272`); sunum mümkün değildir. Bu, testlerin ekransız çalışmasını sağlar
(`modules/platform/tests/window_test.cpp:11-18`). SDL alt sistemleri referans sayımlıdır ve `~Impl` içinde
bırakılır (`window.cpp:104-112`). Not: `App`'in "headless" kipi (`AppDesc::headless`) hiç pencere
oluşturmaz (`modules/app/include/astral/app/app.hpp:220`); platform katmanındaki "headless" ise SDL dummy sürücüsü
demektir; ikisi farklı şeylerdir.

**`keep_responsive`.** Yalnızca `SDL_PumpEvents()` çağırır (`window.cpp:138`); olaylar kuyrukta kalır ve bir
sonraki `poll`'da işlenir. `App` bunu `Renderer::set_busy_callback` üzerinden en fazla 50 ms'de bir çağırır
(`modules/app/src/app.cpp:226-231`). Test: `modules/platform/tests/window_test.cpp:36` (100 çağrı sonrası bekleyen
kapanış isteği kaybolmaz).

**Yüzey.** `surface_source()` SDL'den Vulkan instance eklentilerini ve bir `create_surface` kapanışını verir;
`gpu::Device` bunları kullanır (`window.cpp:269-286`). Platform `gpu`'ya bağımlıdır (yalnızca `Extent2D` ve
`SurfaceSource` için, `modules/platform/include/astral/platform/window.hpp:3`); `tools/module_layers.json` bunu izin
verilen bağımlılık olarak listeler.

## Güçlü yanlar

- `Input` SDL'den bağımsız saf veri olduğu için kenar mantığı, gamepad kırpma ve metin birikimi pencere olmadan
  birim testlidir (`modules/platform/tests/input_test.cpp`, 8 test).
- Ekransız ortamda (CI'da xvfb, yerelde SDL dummy) pencere testleri çalışır; Vulkan yoksa pencere yine açılır ve
  `supports_vulkan()` ile ayırt edilir (`window.cpp:125-130`).
- Tuş eşlemesi fiziksel konuma dayalı: WASD kontrolü klavye düzenine bağlı değil.
- `keep_responsive` olay kaybetmeden yükleme sırasında pencereyi canlı tutar ve gerçek uygulamada bağlanmıştır
  (`modules/app/src/app.cpp:226`).
- Public header'larda SDL tipi yok; proje kuralı (üçüncü parti tip yok) korunuyor.

## Zayıf yanlar ve bilinen sınırlar

- **Pencere yönetimi çok sınırlı:** tam ekran, pencere konumu/boyutu ayarı, kenarlıksız kip, çoklu monitör ve
  vsync dışı sunum kipleri `Window` API'sinde yok (`modules/platform/include/astral/platform/window.hpp:36-83`).
  Pencere yeniden boyutlandırma olayı işlenmez; `size()` her çağrıda SDL'den sorulur (`window.cpp:201`).
- **Gamepad:** yalnızca tek (ilk) gamepad; o çıkarılırsa o anda bağlı başka bir gamepad otomatik açılmaz, yalnızca
  yeni `GAMEPAD_ADDED` olayı açar (`window.cpp:163-175`). Ölü bölge, titreşim, çubuk basma dışı ek düğmeler
  (paddle, touchpad), tetikleyici düğme olayı yok (`input.hpp:29-33`).
- **Klavye kapsamı kısmi:** numpad, F13+, CapsLock, PrintScreen vb. `Key::Unknown` olur (`window.cpp:26-63`,
  `input.hpp:12-24`). Fare yalnızca 3 düğme (`input.hpp:26`).
- **Dokunmatik/kalem yok.** `SDL_EVENT_FINGER_*` olayları için `poll` içinde `case` yok (`modules/platform/src/window.cpp:144`).
- **IME desteği eksik:** yalnızca biten metin (`TEXT_INPUT`) alınır; kompozisyon (`TEXT_EDITING`) olayı
  işlenmez (`window.cpp:162`). Bu, CJK girişi için yetersiz olabilir (doğrulanmadı: SDL'in bu kodda başka bir yolla
  gösterdiği bir kompozisyon yok).
- **WASD köşegeni normalize değil:** `axis`/`wasd` her ekseni ayrı -1..1 verir; W+D `(1, 1)` olur (uzunluk
  ~1.41) (`modules/platform/src/input.cpp:84-89`). Test başlığı "normalised" dese de değer 1,1'i doğrular
  (`modules/platform/tests/input_test.cpp:48-53`).
- **Fare konumu pencere koordinatındadır, piksel değil:** yüksek DPI'da `pixel_density()` ile çarpmak çağıranın işidir
  (`window.hpp:61-63`).
- **Platform kapsamı:** CI yalnızca Linux (gcc, clang, xvfb) ve Windows (MSVC) üzerinde (`.github/workflows/ci.yml:14`,
  `:55`, `:74`); macOS için CI işi yok. Windows CI'da pencere/GPU testleri GPU yoksa atlanır (`ci.yml:71`).
  Bu makinede Windows/MSVC derlemesi ve testleri geçiyor (lead'in doğrulaması).
- **Thread kuralı belgelenmemiş:** `Window` ve `Input`'un hangi thread'den kullanılabileceği başlıkta yazmıyor
  (SDL olay döngüsü genelde ana thread ister; bu kodda denetlenmiyor, doğrulanmadı).
- Kaynakta `TODO`/`FIXME` işareti yok.

## Testler

| Dosya | Kapsam |
|---|---|
| `modules/platform/tests/input_test.cpp` | tuş kenarları, tuş tekrarı, fare delta birikimi, WASD ekseni, fare kenarları, metin birikimi, gamepad (kenar, kırpma, bağlantı kesilince temizleme) |
| `modules/platform/tests/window_test.cpp` | SDL dummy sürücüyle pencere boyutu ve kapanış; `keep_responsive` kapanış isteğini kaybetmez; `surface_source` ile `supports_vulkan` tutarlılığı |
| `modules/platform/tests/paths_test.cpp` | `executable_directory` test ikilisini içeren klasör |

**Kapsanmayanlar:** gerçek OS olayından `Input`'a gerçek yol (SDL olayı enjekte edilmiyor): tuş/scancode eşleme tablosu,
fare düğme eşlemesi, gamepad ekle/çıkar, metin girişi `Window` üzerinden, pano, imleç, fare yakalama testi yok.
Gerçek bir Vulkan yüzeyi oluşturma yolu (`create_surface`) burada test edilmez (dummy sürücüde mevcut değil; GPU
testleri ayrı).

## Performans

Ölçülmedi. Not: lead'in başlangıç ölçümlerinde pencere+cihaz başlatma 0.3-0.6 s (`the_architect`, RTX 3060); bu
süre cihaz oluşturmayı da içerir ve pencereye ayrı atfedilmedi.

## Kararlar

- [ADR-0007: Platform katmanı SDL3](../decisions/0007-sdl3-platform.md)
- [ADR-0003: Kendi RHI'mız, ilk backend Vulkan 1.3](../decisions/0003-vulkan-only-own-rhi.md) (yüzey sağlama ilişkisi)
