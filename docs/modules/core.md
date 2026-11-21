# Astral::Core

Katman 0. Hiçbir modüle bağımlı değil; diğer her modül buna bağımlı.

```cmake
target_link_libraries(MyGame PRIVATE Astral::Core)
```

## İçerik

| Header | Ne işe yarar |
|---|---|
| `astral/core/version.hpp` | `astral::version()` → `"0.1.0"` |
| `astral/core/log.hpp` | `Logger`, `ILogSink`, `ConsoleSink`, `MemorySink` |
| `astral/core/handle.hpp` | `Handle<T>`, `HandlePool<T>` |
| `astral/core/clock.hpp` | `Clock`, `ITimeSource`, `SteadyTimeSource` |
| `astral/core/jobs.hpp` | `JobSystem`, `JobCounter` |

## Logger

```cpp
astral::Logger log;
log.add_sink(std::make_shared<astral::ConsoleSink>());
log.set_level(astral::LogLevel::Debug);
log.info("yüklendi: {} mesh", count);
```

- Global logger yok; oluşturup ihtiyacı olana verirsin.
- Formatlama kilit dışında, sink'lere yazma tek mutex altında (thread-safe).
- Yeni hedef (dosya, oyun içi konsol) = `ILogSink`'ten türeyen yeni sınıf.

## Handle ve HandlePool

```cpp
astral::HandlePool<Mesh> meshes;
auto h = meshes.create(Mesh{...});
if (Mesh* m = meshes.get(h)) { ... }
meshes.destroy(h);          // h artık hiçbir şeye çözülmez
```

Silinen slot yeniden kullanılınca `generation` artar; eski handle'lar yeni nesneye
asla ulaşmaz. Thread-safe değil.

## Clock

`tick()` her karede çağrılır, saniye cinsinden dt döner; dt `max_delta` (varsayılan
0.25 s) ile kırpılır. Test ve replay için sahte `ITimeSource` verilebilir.

## JobSystem

```cpp
astral::JobSystem jobs;          // hw-1 worker
astral::JobCounter c;
jobs.parallel_for(c, n, 4096, [&](size_t b, size_t e) { ... });
jobs.wait(c);                    // beklerken kendi grubunun işlerini yürütür
```

- `wait()` yardımlı bekler, ama **yalnızca beklenen grubun** işlerini çalıştırır. Başka
  grubun işini yığına almak, bir job'un altında askıdaki job'u beklemesine yol açıp
  kilitlenebiliyordu (M0 incelemesinde bulundu, testle düzeltildi).
- Job'daki exception `wait()` tarafından çağırana yeniden fırlatılır.
- Yıkılırken kuyruktaki tüm işler bitirilir.

## Bilinen sınırlar

M0 incelemesinden devreden küçük maddeler (sonraki fazlarda kapatılacak):

- `JobSystem`: tek paylaşımlı kuyruk + tek condvar (work-stealing yok); iç içe yardımlı
  beklemede derinlik sınırı yok; `submit()` içinde bellek hatası olursa sayaç takılı kalır;
  bir `JobCounter` farklı bir `JobSystem` ile kullanılırsa `wait()` sessizce takılır.
- `HandlePool::get()` ile alınan pointer, sonraki `create()` sonrası geçersiz olabilir
  (vector büyür). 32-bit generation 2³²−1 yeniden kullanımdan sonra sarar.
- `Clock` time source'u pointer olarak tutar — geçici nesne vermek dangling olur.
- `Logger::enabled()` mutex alıyor (atomic olmalı); sink içinden aynı logger'a yazmak kilitlenir.
- Kurulum: `Threads::Threads` linklenmiyor (eski glibc'de sorun); header'lar
  `CMAKE_INSTALL_INCLUDEDIR` yerine sabit `include`'a kuruluyor; sürüm metni
  `project(VERSION)`'dan üretilmiyor.
