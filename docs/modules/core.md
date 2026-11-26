# Core (`Astral::Core`)

> **Olgunluk:** Stabil · **Bağımlılıklar:** `Threads::Threads` (başka Astral modülüne bağımlı değil; katman 0, `tools/module_layers.json`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Diğer her modülün altında duran, bağımlılıksız temel katman: iş parçacığı havuzu (`JobSystem`), nesli
olan handle'lar, `Json` değeri (metin + ikili `AJB1`), derleme zamanı reflection (`ASTRAL_REFLECT`), logger,
CPU profiler, saat, ikili okuma/yazma ve küçük bir 3B matematik kütüphanesi. Global durum tutmaz: `Logger`,
`Profiler` ve `JobSystem` örnekleri oluşturulup ihtiyaç duyana verilir (`App::Impl` bunları sahiplenir,
`modules/app/src/app.cpp:170`). Yüksek verimli bir JSON ayrıştırıcı, gelişmiş bir görev sistemi (fiber,
work-stealing) veya tam bir matematik kütüphanesi **değildir**.

## Public API

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `JobSystem`, `JobCounter` | Sabit worker havuzu; `submit`, `parallel_for`, `wait` | `modules/core/include/astral/core/jobs.hpp:33` |
| `JobSystem::wait` | Beklerken yalnızca **beklenen grubun** işlerini çalıştırır; ilk istisnayı yeniden fırlatır | `modules/core/include/astral/core/jobs.hpp:50` |
| `Handle<Tag>`, `HandlePool<T, Tag>` | 32-bit index + 32-bit generation; eski handle yeni nesneye çözülmez | `modules/core/include/astral/core/handle.hpp:13`, `:27` |
| `Json` | Sıra koruyan JSON değeri; tamsayılar kayıpsız (ADR-0041); okumalar atmaz | `modules/core/include/astral/core/json.hpp:19` |
| `Json::parse` / `dump` | Metin okuma (satır/sütunlu hata) ve yazma | `modules/core/include/astral/core/json.hpp:91`, `:89` |
| `Json::to_binary` / `from_binary` / `is_binary` | `AJB1` ikili biçimi (cook çıktısı) | `modules/core/include/astral/core/json.hpp:95`, `:97`, `:99` |
| `ASTRAL_REFLECT`, `ASTRAL_REFLECT_EMPTY` | Tip başına alan listesi (1..32 alan), kalıcı ad ve FNV-1a kimlik | `modules/core/include/astral/core/reflect.hpp:152`, `:163` |
| `ASTRAL_REFLECT_ENUM`, `enum_name`, `enum_from_name` | Enum'u isimle yazıp okuma | `modules/core/include/astral/core/reflect.hpp:174`, `:62`, `:71` |
| `for_each_field`, `stable_hash` | Alanları sırayla gezme; derleyiciden bağımsız 64-bit kimlik | `modules/core/include/astral/core/reflect.hpp:36`, `:80` |
| `Logger`, `ILogSink`, `ConsoleSink`, `MemorySink` | Thread-safe logger; `std::format` tabanlı; sink'ler tek kilit altında çağrılır | `modules/core/include/astral/core/log.hpp:35`, `:27`, `:86`, `:92` |
| `Profiler`, `ProfileScope`, `ASTRAL_PROFILE_SCOPE` | İç içe adlandırılmış CPU kapsamları + kare süresi geçmişi (varsayılan 240) | `modules/core/include/astral/core/profiler.hpp:29`, `:65`, `:88` |
| `demangle` | `typeid().name()` çıktısını okunur tip adına çevirir | `modules/core/include/astral/core/profiler.hpp:81` |
| `Clock`, `ITimeSource` | Kare delta'sı (varsayılan üst sınır 0.25 s); sahte zaman kaynağı verilebilir | `modules/core/include/astral/core/clock.hpp:26`, `:9` |
| `ByteWriter`, `ByteReader` | Little-endian, varint, string; okuyucu sınır denetler, ilk hatadan sonra 0 döner | `modules/core/include/astral/core/bytes.hpp:17`, `:51` |
| `Vec2/3/4`, `Quat`, `Mat4`, `compose`, `inverse`, `look_at`, `perspective`, `orthographic` | Sağ el, +Y yukarı, kamera -Z'ye bakar, Mat4 sütun-öncelikli, Vulkan clip uzayı | `modules/core/include/astral/core/math.hpp:3`, `:162`, `:165`, `:168` |
| `ASTRAL_ASSERT` | Her zaman açık invariant denetimi; `std::abort` | `modules/core/include/astral/core/assert.hpp:8` |
| `version()` | `"MAJOR.MINOR.PATCH"` (CMake `project(VERSION)`'dan üretilir) | `modules/core/include/astral/core/version.hpp:8` |

## Nasıl çalışır

**JobSystem.** `hardware_concurrency - 1` (en az 1) worker, tek `std::deque` kuyruk, tek mutex ve tek
condition variable (`modules/core/src/jobs.cpp:20`, `:90`). `submit` sayacı artırıp işi kuyruğa ekler
(`:112`); iş bitince sayaç azalır ve `notify_all` çağrılır (`:59`). `wait` sayaç bitene kadar uyur ya da
kuyruktaki **o sayaca ait** işleri kendisi çalıştırır (`:134`, `:39`). Başka grubun işini çalıştırmamasının
nedeni yığın tersinmesi kilitlenmesidir (ADR-0004; regresyon testi
`modules/core/tests/jobs_test.cpp:99`). İşteki istisna sayaçta saklanır ve `wait` tarafından, ilkini yeniden
fırlatarak, temizlenir (`modules/core/src/jobs.cpp:63`, `modules/core/src/jobs.cpp:155`). Yıkıcı kuyruktaki her işi bitirir sonra worker'ları join eder
(`modules/core/src/jobs.cpp:101`). `parallel_for` `[0,count)` aralığını `batch` boyutlu parçalara bölüp parça başına bir iş gönderir
(`modules/core/src/jobs.cpp:125`).

**HandlePool.** `std::vector<Slot>` + serbest indeks listesi; her yeniden kullanımda generation artar,
0 ("hiç geçerli değil") atlanır (`modules/core/include/astral/core/handle.hpp:62`). Thread-safe değildir
(`:24`); `get()` pointer'ı sonraki `create()` ile geçersizleşebilir (`:25`).

**Json.** Dahili temsil `variant<nullptr_t, bool, double, string, Array, Object, int64, uint64>`
(`modules/core/include/astral/core/json.hpp:107`). `type()` üç sayı alternatifi için de `Type::Number`
döner; böylece `is_number()` kullanan kod kırılmaz (ADR-0041). Nesne `vector<pair<string, Json>>`
olarak tutulur, ekleme sırası dosyada korunur. Metin ayrıştırıcı özyinelemeli iniştir, derinlik sınırı 256
(`modules/core/src/json.cpp:128`); düz tamsayı yazımı (`.`/`e` yok) 64 bite sığıyorsa tam saklanır
(`:186`). `dump(indent)` 16 elemana kadar sayı/bool/null dizilerini tek satırda yazar (`:71`); NaN/Inf
`null` olarak yazılır (`:43`).

**AJB1 ikili biçimi.** `"AJB1"` + etiketli değerler (`modules/core/src/json_binary.cpp:15`); tamsayılar
zigzag varint (`:28`), `int64` üstü `TagUInt64` (`:17`). Tam sayı değerli `double`'lar da `TagInteger` olarak
yazılır (`:37`), yani geri okununca `is_integer()` true olur (değer eşitliği korunur, `Json::operator==`
`modules/core/src/json.cpp:386`). Okuyucu sayım değerini kalan bayt sayısıyla sınırlar (aşırı tahsisi önler,
`modules/core/src/json_binary.cpp:88`) ve 512 seviyeden derin iç içeliği reddeder (`modules/core/src/json_binary.cpp:18`).

**Reflection.** Makro her alan için `Field{ad, üye işaretçisi}` üretir; sınır 32'dir ve
`static_assert` ile okunur hata verir (`modules/core/include/astral/core/reflect.hpp:157`). 33+ alan için
makro boş alan listesi üretir ki yalnızca `static_assert` mesajı görünsün (`:136`, `:144`). İsim makroya
**yazıldığı gibi** alınır (`#Type`, `:155`), yani `"game::Health"` derleyiciden bağımsız dosya anahtarıdır.
RTTI ve kod üreteci yoktur.

**Logger.** Seviye `atomic`, formatlama kilit dışında, sink'lere dağıtım `mutex_` altında
(`modules/core/include/astral/core/log.hpp:70`, `modules/core/src/log.cpp:32`). Varsayılan seviye `Info`
(`modules/core/include/astral/core/log.hpp:82`). Sink'in aynı logger'a yazması kilitlenir (`modules/core/include/astral/core/log.hpp:34`).

**Profiler.** Yalnızca `begin_frame()` çağıran thread'den kullanılır (`modules/core/include/astral/core/profiler.hpp:10`). Kare sonunda
`current_`/`last_` swap edilir, açık kapsamlar kapatılır (`modules/core/src/profiler.cpp:28`). Kapsam adı
`const char*` olarak saklanır: ömrü profiler'dan uzun olmalı.

## Güçlü yanlar

- Bağımlılıksız ve global durumsuz: tek harici bağımlılık `Threads::Threads` (`modules/core/CMakeLists.txt:16`).
- `JobSystem::wait` iç içe/çapraz grup beklemede kilitlenmez; bu gerçek bir kilitlenmenin regresyon testiyle
  sabitlenmiş (`modules/core/tests/jobs_test.cpp:56`, `:99`).
- 64-bit tamsayılar metin ve ikili biçimde 2^53 üstünde bile kayıpsız dolaşır; `-2^63` ve `2^64-1` uç değerleri
  testli (`modules/core/tests/msr_regression_test.cpp:29`, `:43`).
- `from_binary` hasarlı girdide çökmez, aşırı tahsis yapmaz, derinlik sınırlıdır
  (`modules/core/tests/bytes_test.cpp:64`).
- `ASTRAL_REFLECT` limiti hem pozitif (32 alan çalışır, `modules/core/tests/reflect_test.cpp:53`) hem negatif
  (33 alan derlenmez ve beklenen mesajı verir, `modules/core/CMakeLists.txt:34-42`) olarak ctest'te denetlenir.
- Kaynak dosyaların hepsi ADR-0030 bütçesinin (600 satır) altında; en büyüğü `json.cpp` 480 satır.
- Repoda `ASTRAL_REFLECT(` geçen yaklaşık 87 satır var (grep; makro tanımı ve testler dahil): reflection bileşen ve ayar tiplerinde yaygın kullanılıyor.

## Zayıf yanlar ve bilinen sınırlar

- **JobSystem basit:** tek paylaşımlı kuyruk, tek mutex, her olayda `notify_all`; work-stealing, öncelik,
  bağımlılık grafı yok (`modules/core/src/jobs.cpp:20`, `:71`, `:118`). `wait` yardımı kuyrukta doğrusal tarama
  yapar (`:39`). İş tipi `std::function` (heap tahsisi olabilir; `modules/core/include/astral/core/jobs.hpp:35`). Ölçülmüş bir ölçeklenme verisi yok.
- **`submit()` kuyruğa ekleme hatasında sayacı geri alır** ve istisnayı yeniden atar (`modules/core/src/jobs.cpp:112-123`); bu yol
  testsizdir (bellek hatası üretmek gerekir). Bir `JobCounter` farklı bir `JobSystem` ile
  kullanılırsa `wait` takılır; yalnızca belgelenmiş (`modules/core/include/astral/core/jobs.hpp:32`), çalışma zamanında denetlenmez.
- **`Json` nesneleri doğrusal arama:** `find`, `operator[]` ve ayrıştırıcının her üye eklemesi (`out[key] = ...`)
  üye sayısıyla O(n) (`modules/core/src/json.cpp:316`, `:446`); çok üyeli nesnelerde ayrıştırma O(n²). Başlık
  bunu bilerek "yüksek verimli ayrıştırıcı değil" diye belirtir (`modules/core/include/astral/core/json.hpp:4`). İkili okuyucu da aynı
  `out[key]` yolunu kullanır (`modules/core/src/json_binary.cpp:105`).
- **Yinelenen anahtar sessizce birleşir:** son değer kazanır, konum ilk görünümde kalır (`modules/core/src/json.cpp:316`).
- **Ayrıştırıcı katı değil:** baştaki sıfırlar kabul edilir (`007` -> 7, `modules/core/src/json.cpp:174`); NaN/Inf yazarken
  `null`'a dönüşür, yani kayıplı (`modules/core/src/json.cpp:43`). Metin derinlik sınırı (256) için test bulunamadı
  (derinlik testi yalnızca ikili biçim için, `modules/core/tests/bytes_test.cpp:81`).
- **Sessiz tip değiştirme:** non-const `operator[]` (anahtarla) nesne olmayan değeri sessizce boş nesneye çevirir
  (`modules/core/src/json.cpp:443`), `push_back` diziye (`modules/core/src/json.cpp:466`).
- **İkili biçim tip bilgisini kısmen kaybeder:** `3.0` yazılıp `3` (tamsayı) olarak okunur (`modules/core/src/json_binary.cpp:37`).
  Değer olarak eşit, ama `is_integer()` farklı.
- **Logger sınırlı:** dosya sink'i yok (yalnızca `ConsoleSink`, `MemorySink`); `MemorySink` seviye ve konumu
  saklamaz, yalnızca mesaj metnini (`modules/core/src/log.cpp:53`). Sink içinden aynı logger'a yazmak kilitlenir
  (`modules/core/include/astral/core/log.hpp:34`).
- **Profiler tek thread:** worker thread'lerden kapsam kaydı desteklenmez (`modules/core/include/astral/core/profiler.hpp:10`); GPU süreleri ayrı
  (`Renderer::gpu_timings`, bkz. `modules/app/include/astral/app/app.hpp:86`).
- **Math minimal:** `Quat` için slerp/euler yok, `Vec2` yalnızca `+ - *`; `look_at` yukarı vektör bakış
  yönüne paralelse `normalize` sıfır vektör döndürür ve dejenere matris çıkar, korunma yok
  (`modules/core/src/math.cpp:83-91`); `inverse` tekil matriste sessizce birim matris döndürür
  (`modules/core/src/math.cpp:48`). `rotation_of` shear'ı desteklemez (`modules/core/include/astral/core/math.hpp:158`).
- **`HandlePool`:** thread-safe değil, `for_each` içinde create/destroy yasak, generation 2^32-1 yeniden
  kullanımda sarar (`modules/core/include/astral/core/handle.hpp:24-26`, `:72`).
- **Reflection sınırı 32 alan:** daha geniş tip bölünmelidir (ADR-0030); `ASTRAL_REFLECT_ENUM` için sayı sınırı
  denetimi yok (`modules/core/include/astral/core/reflect.hpp:174`). ADR-0030 ayrıca her struct için 25 veri üyesi bütçesi koyar
  (`tools/check_sizes.py:33`); reflection'ın 32'lik üst sınırı bu bütçeden gevşektir.
- `ASTRAL_REFLECT_ENUM` ve `enum_name`/`enum_from_name` core testlerinde yok; yalnızca world ve script
  testlerinde kullanılıyor (`modules/world/tests/scene_test.cpp:21`, `modules/script/tests/api_test.cpp:20`).
- Kaynak kodda bu modülde `TODO`/`FIXME` işareti yok.

## Testler

`modules/core/tests/` (hepsi doctest; hedef `astral_core_tests`, `modules/core/CMakeLists.txt:19`):

| Dosya | Kapsam |
|---|---|
| `jobs_test.cpp` | tüm işler çalışır, `parallel_for` her indeks tam bir kez, iç içe wait, istisna yayılımı, yıkıcı bekleyen işleri bitirir, çapraz grup kilitlenme regresyonu |
| `json_test.cpp`, `json_crlf_test.cpp` | kurma/okuma/yazma, kaçışlar, sayı round-trip, konumlu hata, yanlış tip varsayılanı, CRLF |
| `msr_regression_test.cpp` | C05: 64-bit tamsayı kayıpsızlığı, metin ve AJB round-trip, tamsayı/double eşitliği |
| `bytes_test.cpp` | `ByteWriter`/`ByteReader` round-trip; ikili JSON round-trip ve hasarlı girdi |
| `reflect_test.cpp` + `compile_fail/reflect_too_many_fields.cpp` | alan sırası/kimlik, üye işaretçisi, 32 alan sınırı ve 33 alan hatası |
| `handle_test.cpp` | geçersiz handle, eski handle çözülmemesi, çift destroy, ayrı tag, `for_each` |
| `log_test.cpp` | seviye filtresi, format, çoklu sink, eşzamanlı yazım |
| `clock_test.cpp`, `math_test.cpp`, `profiler_test.cpp`, `version_test.cpp` | saat kırpma, vektör/matris/quaternion, profiler iç içe kapsam ve geçmiş, `demangle`, sürüm |

**Kapsanmayanlar:** `JobSystem` için yoğun/uzun stres testi ve performans testi yok; `submit` bellek hatası yolu
test edilmiyor; metin JSON derinlik sınırı (256), NaN/Inf yazımı, yinelenen anahtar davranışı ve baştaki sıfır için
core testlerinde ve world/assets testlerinde (grep ile arandı) karşılık bulunamadı; `look_at` dejenere girdisi ve
`orthographic` hiç test edilmiyor (`modules/core/tests/math_test.cpp`).

## Performans

Ölçülmedi. Bu modül için kayıtlı bir benchmark veya ölçüm yok. Yalnızca başlıktaki iddia (`AJB1` ikili okuma
metinden "birkaç kat hızlı", `modules/core/include/astral/core/json.hpp:94`) kaynak kodda bir ölçümle desteklenmiyor
(doğrulanmadı); `modules/core/tests/bytes_test.cpp:48` yalnızca ikili biçimin metinden **küçük** olduğunu denetler.

## Kararlar

- [ADR-0004: Yardımlı wait() sadece kendi grubunu çalıştırır](../decisions/0004-helping-wait-own-group.md)
- [ADR-0013: Değişiklik takibi, reflection ve JSON serileştirme](../decisions/0013-ecs-change-detection-reflection.md)
- [ADR-0030: Refaktör kuralları ve boyut bütçesi](../decisions/0030-refactor-rules-and-size-budget.md)
- [ADR-0032: Ölçeklenebilir çalışma zamanı ve paralel zamanlayıcı](../decisions/0032-scalable-runtime-and-parallel-scheduler.md) (JobSystem'in tüketicisi)
- [ADR-0041: Json'da kayıpsız 64-bit tamsayı](../decisions/0041-lossless-json-integers.md)
