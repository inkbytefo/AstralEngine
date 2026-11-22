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
Katman 3  │ app (Game + System + Context)        — oyunlar buradan yazılır
Katman 2  │ world (ECS) · script (Luau; yalnız Core+World) · render · physics (Jolt) · assets · audio
Katman 1  │ gpu (Vulkan RHI) ← platform (pencere/input, SDL3)
Katman 0  │ core (log, handle, clock, jobs, math)
```

**Kural:** bir modül yalnızca kendinden alttaki katmanlara (ve aynı katmanda ok yönünde) bağımlı
olabilir. Platform GPU'yu bilir (pencere yüzeyi verir), GPU platformu bilmez. CMake bunu
zorlar: bir modül, linklemediği modülün header'ını bulamaz.

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
examples/                           çalışan örnekler
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
