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
| `astral/core/math.hpp` | `Vec2/3/4`, `Mat4`, `Quat`, `perspective`, `look_at`, `inverse`… |
| `astral/core/json.hpp` | `Json`: sahne/prefab/ayar verisi için küçük JSON (sıra korunur, istisna atmaz) |
| `astral/core/reflect.hpp` | `ASTRAL_REFLECT(Tip, alanlar...)`: derleme zamanı alan listesi, kalıcı ad/kimlik |

## Reflection ve JSON

```cpp
struct Health { int value = 100; float regen = 0; };
ASTRAL_REFLECT(Health, value, regen)          // global kapsamda, tipten sonra; en çok 32 alan
astral::for_each_field<Health>([&](const auto& f) { /* f.name, obj.*(f.member) */ });
Reflect<Health>::name  // "Health" — dosyalardaki kimlik; derleyiciden bağımsız
```

- Kimlik makroya **yazıldığı gibi** ad: `ASTRAL_REFLECT(game::Health, …)` → `"game::Health"`. Tipi yeniden
  adlandırmak = dosyalardaki anahtarı değiştirmek.
- `ASTRAL_REFLECT_ENUM(game::Team, Red, Blue)`: enum'u isimle dosyaya yazdırır; `enum_name(v)`,
  `enum_from_name<T>("Red")`. Açık değerli ve sırasız enumerator'lar da olur (`using enum` ile değer alınır).
- `ByteWriter` / `ByteReader` (`astral/core/bytes.hpp`): little-endian, varint, string; okuyucu sınırları
  denetler, ilk hatadan sonra sıfır döner ve `ok()` false olur. Cook edilmiş formatlar bunu kullanır.
- `Json::to_binary()` / `from_binary()` / `is_binary()`: ikili JSON (`AJB1`, etiketli değerler, tam sayılar
  varint). Hasarlı girdiyi reddeder, aşırı bellek ayırmaz, 512 seviyeden derin iç içeliği reddeder.
- `Json::erase(key)`; `dump(indent)` kısa sayı dizilerini tek satırda yazar (`[0, 1.5, 2]`).
- `Json`: nesneler ekleme sırasını korur (kaydedilen dosyalar temiz diff'lenir); okuma yanlış tipte varsayılan
  döner; `Json::parse` hatada satır/sütun verir; sayılar en kısa ve tam geri dönüşen biçimde yazılır.

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
astral::HandlePool<Mesh> meshes;          // veya HandlePool<KayıtTipi, PublicTag>
auto h = meshes.create(Mesh{...});
if (Mesh* m = meshes.get(h)) { ... }
meshes.destroy(h);          // h artık hiçbir şeye çözülmez
```

Silinen slot yeniden kullanılınca `generation` artar; eski handle'lar yeni nesneye
asla ulaşmaz. Thread-safe değil.

## Math

Bağımlılıksız 3D matematik. Kurallar: sağ el, +Y yukarı, kamera -Z'ye bakar; `Mat4` sütun-öncelikli
(`m[sütun][satır]`), `v' = M * v`; `perspective` Vulkan clip uzayına (Y aşağı, derinlik 0..1).

```cpp
Mat4 model = compose(position, Quat::from_axis_angle({0, 1, 0}, radians(45)), {1, 1, 1});
Mat4 view = look_at(eye, target, {0, 1, 0});
Mat4 proj = perspective(radians(70), aspect, 0.1f, 1000.0f);
```

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
  (dokümante edildi). 32-bit generation 2³²−1 yeniden kullanımdan sonra sarar.
- Sink içinden aynı logger'a yazmak kilitlenir (dokümante edildi).

M1'de kapatılanlar: Threads bağımlılığı, kurulum dizinleri, sürümün `project(VERSION)`'dan
üretilmesi, atomic log seviyesi, `Clock`'a geçici kaynak verilmesinin derleme hatası olması.

## Profiler (M8)

`Profiler` (global değil): `begin_frame` / `end_frame`, `ProfileScope` / `ASTRAL_PROFILE_SCOPE(profiler, "ad")`
ile iç içe kapsamlar; `last_frame()` (`ProfileSample{name, depth, start_ms, ms}`), `frame_times()` geçmişi,
`scope_ms(ad)`, `set_enabled`. Tek thread'den (ana döngü) kullanılır. `demangle` tip adlarını okunur yapar.
`bytes.hpp`: küçük ikili okuma/yazma yardımcıları (little-endian).
