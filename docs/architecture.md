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

`Astral::App` ileride bu döngüyü hazır sunan **opsiyonel** bir yardımcı olacak.

## Katmanlar

```
Katman 3  │ app                                   (opsiyonel)
Katman 2  │ render · world · physics · assets · audio
Katman 1  │ platform (pencere/input) · gpu (RHI)
Katman 0  │ core (log, handle, clock, jobs, math)
```

**Kural:** bir modül yalnızca kendinden alttaki katmanlara bağımlı olabilir. CMake bunu
zorlar: bir modül, linklemediği modülün header'ını bulamaz.

## SOLID'in motordaki karşılığı

| İlke | Uygulama |
|---|---|
| Tek sorumluluk | Her modülün tek işi var. Bir sınıf ya kaynak sahibi ya algoritmadır. |
| Açık/kapalı | Yeni render tekniği = yeni render graph pass'i; yeni format = yeni loader. Mevcut kod değişmez. |
| Liskov | Her backend aynı sözleşmeyi uygular; desteklenmeyen özellik `supports()` ile sorulur. |
| Arayüz ayrımı | Küçük arayüzler: `ILogSink`, `ITimeSource`, `IWindow`… Oyun sadece ihtiyacı olanı linkler. |
| Bağımlılığın tersine çevrilmesi | Bağımlılıklar constructor ile verilir. Singleton, service locator yok. |

## Değişmez kurallar

- Global değiştirilebilir state yok. (Durumsuz sabitler serbest — örn. `default_time_source()`.)
- Public header'larda üçüncü parti tip yok (Vulkan, SDL, Jolt, EnTT `src/` içinde kalır).
- Kaynaklara ham pointer ile değil `Handle<T>` ile erişilir.
- Sıcak veri data-oriented (SoA/ECS); OOP arayüzler sadece modül sınırlarında.
- Her modül tek başına derlenir ve test edilir; renderer olmadan headless çalışma desteklenir.

## Klasör yapısı

```
modules/<ad>/include/astral/<ad>/   public API
modules/<ad>/src/                   private uygulama (+ src/detail/)
modules/<ad>/tests/                 birim testleri
examples/                           çalışan örnekler
tests/consumer/                     find_package duman testi
tests/integration/                  modüller arası testler (gerektiğinde)
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
