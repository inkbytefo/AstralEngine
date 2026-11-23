# Mimari

## Temel fikir: kütüphane, framework değil

Oyun main loop'un sahibidir ve motoru çağırır. Motor oyunu asla çağırmaz.
Bu performans için değil, **kontrol ve test edilebilirlik** için: güncelleme sırası,
fizik adımı ve thread'ler oyunun kararıdır. Ayrıntı: [ADR-0001](decisions/0001-library-not-framework.md).

```cpp
while (window.poll()) {
    float dt = clock.tick();
    game.update(world, dt);   // oyunun kodu
    physics.step(dt);
    renderer.render(world, camera);
}
```

Oyunlar ise genellikle bu döngüyü elle yazmaz: üstteki `Astral::App` katmanı döngünün sahibi olur,
oyun `Game` + component + system yazar ([ADR-0006](decisions/0006-app-layer-for-games.md)).
Alt katman her zaman doğrudan kullanılabilir kalır.

## Katmanlar

```
Katman 4  │ editor (bağımsız masaüstü çalışma alanı, Astral::ImGui) — oyun ve sunucu linklemez
Katman 3  │ app (Game + System + Context; + Astral::ImGui) · editor_core (GPU'suz belge/transaction/AI)
Katman 2  │ world (ECS) ← script (Luau) · physics (Jolt) · audio · render · assets (→ world, render)
Katman 1  │ shader (.ashader / glslang) · gpu (Vulkan RHI) ← platform (pencere/input, SDL3)
Katman 0  │ core (log, handle, clock, jobs, math)
```

CMake'te tanımlı doğrudan yönlü bağımlılık grafiği (`Astral::*` modül kenarları, `tools/module_layers.json` ile aynı):

| Modül | Katman | Doğrudan bağımlılıklar (`DEPS` / `PRIVATE_DEPS`) |
|---|---|---|
| `core` | 0 | — |
| `shader` | 1 | `core` |
| `gpu` | 1 | `core` |
| `platform` | 1 | `core`, `gpu` |
| `world` | 2 | `core` |
| `physics` | 2 | `core` |
| `audio` | 2 | `core` |
| `script` | 2 | `core`, `world` |
| `render` | 2 | `core`, `gpu`, `shader` |
| `assets` | 2 | `core`, `world`, `render` |
| `app` | 3 | `core`, `platform`, `gpu`, `world`, `script`, `render`, `assets`, `physics`, `audio` (+ üçüncü parti sarmalayıcı `Astral::ImGui`) |
| `editor_core` | 3 | `core`, `world`, `assets` (GPU/pencere/ImGui bağımsız) |
| `editor` | 4 | `core`, `world`, `assets`, `app`, `editor_core` (+ üçüncü parti sarmalayıcı `Astral::ImGui`; oyun/sunucu linklemez) |

**Kural ve denetim:**
- Bir modül yalnızca kendinden alttaki katmanlara (veya izin listesinde açıkça belirtilen aynı katmandaki ok yönüne) bağımlı olabilir; döngü yasaktır.
- Tüm motor modülleri yalnızca `modules/<ad>/CMakeLists.txt` içinde `astral_add_module(...)` ile kaydedilir; elle motor-arası `target_link_libraries` çağrısı yapılmaz (yalnız sistem kütüphaneleri için örn. Linux'ta `m`, veya `app` içindeki `Astral::ImGui` sarmalayıcısı vardır).
- Configure anında `astral_export_module_graph` (`cmake/AstralModule.cmake`), `astral_add_module` meta verilerini ve hedef link özelliklerini (`DEPS`, `PRIVATE_DEPS`, `LINK_LIBRARIES`, `INTERFACE_LINK_LIBRARIES`) okuyup `${CMAKE_BINARY_DIR}/astral_module_graph.json` dosyasına yazar.
- CTest `layer_check` (`tools/check_layers.py`), bu çıktıyı `tools/module_layers.json` ile doğrular: izin listesinde olmayan yeni/özel kenar, üst katmana bağımlılık, döngü veya tanınmayan `Astral::` / `astral_*` hedefi testi kırar.

**İzin verilen bir kenarı değiştirmek:**
1. Bağımlılığın döngü oluşturmadığını ve katman yönüne (`katman(kaynak) >= katman(hedef)`) uyduğunu doğrulayın.
2. `modules/<ad>/CMakeLists.txt` içinde `DEPS` / `PRIVATE_DEPS` alanını (ve gerekirse kök `CMakeLists.txt` içindeki `add_subdirectory` sırasını) güncelleyin.
3. Aynı değişiklikte `tools/module_layers.json` izin listesini ve bu sayfadaki yönlü grafik tablosunu güncelleyin (yeni bir mimari sınır açılıyorsa `docs/decisions/` altına ADR ekleyin).
4. Projeyi yeniden configure edip `ctest --preset debug -R layer_check` çalıştırarak doğrulayın.

## SOLID'in motordaki karşılığı

| İlke | Uygulama |
|---|---|
| Tek sorumluluk | Her modülün tek işi var. Bir sınıf ya kaynak sahibi ya algoritmadır. |
| Açık/kapalı | Yeni render tekniği = yeni render graph pass'i; yeni format = yeni loader. Mevcut kod değişmez. |
| Liskov | Her backend aynı sözleşmeyi uygular; desteklenmeyen özellik `supports()` ile sorulur. |
| Arayüz ayrımı | Küçük arayüzler: `ILogSink`, `ITimeSource`, `IWindow`… Oyun sadece ihtiyacı olanı linkler. |
| Bağımlılığın tersine çevrilmesi | Bağımlılıklar constructor ile verilir. Singleton, service locator yok. |

**Oyun tarafında** aynı ilkeler özellik paketleriyle uygulanır: component yalnızca veri, sistem tek iş,
sistemler birbirini çağırmaz (olaylarla konuşur), servisler `Context` arayüzlerinden, global yok.
Ayrıntı ve yapay zekâ dostu düzen: [ADR-0023](decisions/0023-modular-game-code.md).

## Değişmez kurallar

- Global değiştirilebilir state yok. (Durumsuz sabitler serbest — örn. `default_time_source()`.)
- Public header'larda üçüncü parti tip yok (Vulkan, SDL, Jolt `src/` içinde kalır).
- ECS ve çekirdek veri yapıları bizim: hazır ECS kütüphanesi (EnTT vb.) kullanılmaz ([ADR-0005](decisions/0005-own-ecs.md)).
- Kaynaklara ham pointer ile değil `Handle<T>` ile erişilir.
- Sıcak veri data-oriented (SoA/ECS); OOP arayüzler sadece modül sınırlarında.
- Oyun kodu özellik paketleridir; özellikler birbirini çağırmaz, olay ve component ile konuşur, sınırı `astral-features check` denetler ([ADR-0023](decisions/0023-modular-game-code.md)).
- Script (Luau) durum tutmaz: durum component'lerdedir ([ADR-0017](decisions/0017-gameplay-scripting.md), [ADR-0026](decisions/0026-script-host-and-features.md)).
- Her modül tek başına derlenir ve test edilir; renderer olmadan headless çalışma desteklenir.

## Klasör yapısı

```
modules/<ad>/include/astral/<ad>/   public API
modules/<ad>/src/                   private uygulama (+ src/detail/)
modules/<ad>/tests/                 birim testleri
templates/blank/                    boş oyun şablonu (yeni oyun için kopyala)
examples/                           şablonla aynı düzende çalışan oyunlar
tests/consumer/                     find_package duman testi
tests/integration/                  modüller arası testler (gerektiğinde)
tests/golden/                       görüntü testleri (`ctest -L golden`)
tools/                              astral-cook, astral-features
cmake/                              derleme yardımcıları
docs/                               bu dokümanlar
```

Yeni modül eklemek: `modules/<ad>/CMakeLists.txt` içinde `astral_add_module(NAME <ad> ...)`
ve `astral_add_tests(MODULE <ad> ...)`, kök `CMakeLists.txt`'ye katman sırasıyla
`add_subdirectory`. Kurulum/export otomatik gelir.

## İsimlendirme

- Namespace `astral::`. GPU tipleri `astral::gpu::` (isim çakışmasını önlemek için).
- Dosyalar `snake_case.hpp/.cpp`, her ana sınıf için bir çift.
- CMake hedefi `astral_<ad>`, alias `Astral::<Ad>`.
