# Script (`Astral::Script`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core, Astral::World (`modules/script/CMakeLists.txt:29-30`); Luau 0.748 kaynaktan derlenip kütüphaneye gömülür (`modules/script/CMakeLists.txt:6`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Oyun kurallarını Luau'da yazmayı sağlar: bileşenler üzerinde sistemler, tek varlığın davranışları, olay dinleyicileri, zamanlayıcılar, kayıt/yükleme. Betikte durum tutulmaz; durum bileşenlerde yaşar,
böylece betik her an yeniden yüklenebilir (`modules/script/include/astral/script/script.hpp:2-4`). Modül yalnızca `World`'e bağlıdır; fizik, girdi ve depolama dar arayüzlerle (`ScriptPhysics`, `ScriptInput`,
`ScriptStorage`) dışarıdan verilir, App bunları uygular (`modules/app/src/services/script_bridge.cpp:23-87`). Modül Luau türlerini public başlıklara sızdırmaz. Betikten ses, HUD, sahne yükleme, kamera, VFX (olay dışında)
ve render API'si **yoktur**; yalnızca aşağıdaki `astral.*` yüzeyi vardır.

## Public API

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `ScriptHost` | Tek Luau durumu. `load(name, source)`, `unload`, `loaded`, `modules`, `failed`, `run(phase, frame)`, `take_errors` | `modules/script/include/astral/script/script.hpp:160` |
| `ScriptEnv` | Ana bilgisayarın eriştikleri (World, TypeRegistry, EventRegistry, Logger, isteğe bağlı input/storage/physics/profiler); global yok | `modules/script/include/astral/script/script.hpp:144` |
| `ScriptLimits{budget_ms=100}` | Bir faz (ya da modül yüklemesi) bu süreyi aşarsa hata ile durdurulur | `modules/script/include/astral/script/script.hpp:155-158` |
| `ScriptPhase`, `ScriptFrame` | `PreUpdate/Update/FixedUpdate/PostUpdate`; `delta`, `fixed_delta`, `elapsed` (betikte `ctx.delta/fixed_delta/time`) | `modules/script/include/astral/script/script.hpp:35`, `modules/script/include/astral/script/script.hpp:38` |
| `Script` (bileşen) | `path` ile bu dosyanın `astral.behaviour`'ını çalıştırır | `modules/script/include/astral/script/script.hpp:30` |
| `ScriptPhysics` | Fizik/alan arayüzü: raycast/spherecast (filtreli), overlap, impulse/force/torque/force_at, hız, karakter, teleport, eklem motoru, rüzgâr/su | `modules/script/include/astral/script/script.hpp:90` |
| `ScriptInput`, `ScriptStorage` | Adlandırılmış eylemler (`down/pressed/released/value/axis`); kayıt slotu `write/read` | `modules/script/include/astral/script/script.hpp:45`, `modules/script/include/astral/script/script.hpp:136` |
| `ScriptApiCatalog`, `script_api_catalog()` | Çağrılabilir bağlamaların **tek** katalog kaynağı | `modules/script/include/astral/script/api.hpp:29`, `modules/script/include/astral/script/api.hpp:33` |
| `collect_api_discovery`, `generate_script_api` | Bileşen/olay/çağrı özeti; `astral.d.luau` + `API.md` metni | `modules/script/include/astral/script/api.hpp:50`, `modules/script/include/astral/script/api.hpp:59` |
| `compile_script`, `is_compiled_script` | Luau → bayt kodu (`"ALBC"` + bayt kodu); cook aşamasında kullanılır | `modules/script/include/astral/script/compile.hpp:13`, `modules/script/include/astral/script/compile.hpp:16` |

### `astral.*` yüzeyi (katalogdan: `modules/script/src/api.cpp:10-127`)

Aşağıdaki liste kataloğun içeriğidir; `API.md`/`astral.d.luau` aynı katalogdan üretilir. Ek olarak her bileşen/olay türü (`TypeRegistry`/`EventRegistry`) üretilen belgelerde alanlarıyla listelenir.

| Ad alanı | Fonksiyonlar | Not |
|---|---|---|
| `Entity` (yöntemler) | `get(name)` (canlı proxy: `c.field = v` yazar), `has`, `add(name, fields?)`, `remove`, `destroy`, `alive`, `id` | `modules/script/src/api.cpp:12-20`; ölü varlıkta hata verir (`modules/script/src/bindings.cpp:122`) |
| `astral` (üst düzey) | `system{name, phase, query, with, without, update/run}`, `behaviour{on_start, on_update, on_fixed_update}`, `on(event, fn, phase?)`, `send(event, payload?)`, `define_component(name, fields)`, `define_event(name)`, `spawn{...}`, `find(name)`, `query{...}`, `each(list, fn)`, `after`, `every`, `cancel`, `random`, `seed`, `log/warn/error`, `save`, `load`, `has_save`, `ease`, `lerp`, `tween`, `state_machine` | `modules/script/src/api.cpp:23-66`; ayrıca global `print` (= `astral.log`) `modules/script/src/bindings.cpp:586-587` |
| `astral.physics` | `raycast`, `raycast_batch`, `spherecast`, `overlap_sphere`, `overlap_box`, `impulse`, `force`, `set_velocity`, `velocity`, `force_at`, `torque`, `angular_impulse`, `set_angular_velocity`, `angular_velocity`, `move_character`, `character_state`, `teleport`, `wake`, `sleep`, `set_joint_motor_mode`, `set_joint_motor_velocity`, `set_joint_motor_position`, `break_joint` | `modules/script/src/api.cpp:69-104`. Sorgu seçenekleri `{layers, ignore (varlık ya da liste), sensors}` (ADR-0045); sorgular son tamamlanan adımı okur, komutlar sıraya alınıp sabit adım sınırında uygulanır |
| `astral.fields` | `wind(position)`, `water(position)` → `{inside, surface, depth, flow, density}` | `modules/script/src/api.cpp:107-110` |
| `astral.quat` | `axis_angle`, `mul` (b önce uygulanır), `normalize`, `rotate` | `modules/script/src/api.cpp:113-118` |
| `astral.input` | `down`, `pressed`, `released`, `value`, `axis` (adlandırılmış eylemler) | `modules/script/src/api.cpp:121-126` |

`astral.tween`, `astral.ease`, `astral.lerp` ve `astral.state_machine` C++'ta değil, Luau'da yazılmış bir ön ayarda (`prelude`) tanımlıdır; sandbox kapanmadan önce bir kez çalıştırılır
(`modules/script/src/prelude.hpp:5`, `modules/script/src/host.cpp:99-108`). Kolaylık eğrileri: `linear, in_quad, out_quad, in_out_quad, in_cubic, out_cubic, in_out_cubic, out_back` (`modules/script/src/api.cpp:59-61`).
`define_component` tipleri: `f32, i32, u32, bool, vec2, vec3, vec4, quat, entity` (`{type, default}` ile varsayılan) (`modules/script/src/api.cpp:33-35`, `modules/script/src/bindings.cpp:327`).
Değer türleri: sayı/bool/string; `vector` (Luau yerleşik); `Vec2/Vec4/Quat` `{x=,y=,..}` tabloları; enum'lar ad dizgeleri; varlık alanları varlık ya da `nil`
(`modules/script/src/api.cpp:262-263`). Tablo → JSON dönüşümü en çok 64 iç içe seviye kabul eder; kendini içeren tablo hata verir (`modules/script/src/convert.cpp:199-201`).

## Nasıl çalışır

### Durum, modüller, sandbox

Tüm betikler **tek** `lua_State`'te çalışır (`luaL_newstate`, `luaL_openlibs`); `astral` tablosu ve ön ayar kurulduktan sonra `luaL_sandbox` global tabloları salt okunur yapar
(`modules/script/src/host.cpp:93-108`). Her modül (dosya) kendi sandbox iş parçacığında (`luaL_sandboxthread`) yüklenir: global yazmaları o modüle özel kalır
(`modules/script/src/host.cpp:123-126`). Luau'nun standart kütüphanesinde `io` yoktur ve `os` yalnızca `clock/date/difftime/time` sağlar (Luau 0.748 kaynağı: `VM/src/loslib.cpp`); testle de doğrulanmıştır
(`modules/script/tests/script_test.cpp:196-202`, "a script cannot reach the operating system"). `require` bu ana bilgisayarda tanımlı değildir (modül çözümleyici yok).
Bir `Module`; sistemleri, dinleyicileri, davranış tablosunu, zamanlayıcıları ve `failed` bayrağını tutar (`modules/script/src/host_impl.hpp:47-54`).

### Faz çalıştırma sırası

`ScriptHost::run(phase)` modülleri **yükleme sırasıyla** gezer; her modül için önce olay dinleyicileri, sonra o fazın sistemleri çalışır; ardından (yalnızca Update/FixedUpdate) davranışlar, en sonda (yalnızca Update) zamanlayıcılar
(`modules/script/src/host.cpp:349-380`). App'te her fazın C++ sistemlerinden **sonra** çağrılır; `FixedUpdate` her sabit fizik adımında koşar
(`modules/app/src/services/frame_loop.cpp:118-123`, `modules/app/src/services/frame_loop.cpp:133-136`). Per-entity sistemler önce eşleşen varlıkları toplar, sonra çağırır (betik çalışırken varlık yaratıp silebilir);
her çağrıdan önce varlığın hâlâ canlı ve eşleşir olduğu yeniden denetlenir (`modules/script/src/host.cpp:224-247`). Davranışlarda `on_start` bir varlık+yol çifti için bir kez çalışır; yol değişirse ya da `Script` çıkarılıp
eklenirse yeni örnektir (`modules/script/src/host.cpp:297-308`, test: `modules/script/tests/msr_regression_test.cpp:107`).

### Hata yalıtımı ve zaman bütçesi

Bir geri çağrı hata verirse `protected_call` hatayı kaydeder ve **o modülü `failed` işaretler**; modül yeniden yüklenene kadar çalışmaz, diğer modüller sürer (`modules/script/src/host.cpp:78-89`, test:
`modules/script/tests/script_test.cpp:150-163`). Zaman bütçesi Luau'nun `interrupt` kancasındadır: `ScriptHost::run` ve `load` başında duvar saati (steady_clock) son tarihi kurulur ve aşılırsa
`luaL_error` ile "script exceeded its time budget" fırlatılır (`modules/script/src/host.cpp:18-25`, `modules/script/src/host.cpp:68-71`, `modules/script/src/host.cpp:119`, `modules/script/src/host.cpp:352`). Varsayılan 100 ms (`modules/script/include/astral/script/script.hpp:157`). Test: `modules/script/tests/script_test.cpp:164-170`.

### Aşamalı modül yükleme ve sıcak yeniden yükleme (ADR-0044)

`ScriptHost::load` yeni modülü ayrı iş parçacığında çalıştırır; üst düzey kod yalnızca **bildirim aşamasıdır** (`Impl::loading`): dünyayı değiştiren çağrılar (`astral.spawn`, `entity:add/remove/destroy`,
alan yazma, `astral.send`, `save/load`, tüm `astral.physics` komutları) hata verir ("… is not allowed while the module loads; call it from on_load() or a callback": `modules/script/src/bindings.cpp:59-61`).
`define_component`/`define_event` hemen kaydolur ama başarısızlıkta `rollback_dynamic` ile geri alınır; başarılı yükleme eski modülün yerine geçtikten sonra modülün global `on_load()` fonksiyonu çağrılır
(`modules/script/src/host.cpp:139-184`). Başarısız yeniden yükleme dünyayı, tipleri ve olayları değiştirmez; eski modül çalışmaya devam eder (testler: `modules/script/tests/msr_regression_test.cpp:140-211`).
Bayt kodu (`"ALBC"`) ve kaynak aynı yoldan yüklenir (`modules/script/src/host.cpp:130-138`).
App tarafında `ScriptBridge` izlenen her betiğin içerik özetini tutar ve geliştirme modunda (pack yokken) **her 0,5 sn'de** dosyayı yeniden okuyup özet değiştiyse yeniden yükler; başarılıysa "reloaded …" yazar
(`modules/app/src/services/script_bridge.cpp:225-240`, `modules/app/src/services/frame_loop.cpp:113`).

### Olaylar, zamanlayıcılar, kayıt

- **Olaylar:** C++ olayları (`events.add<T>()`) alanlarıyla tabloya, betik olayları (`define_event`) serbest tabloya dönüşür. `astral.on` yalnızca **kayıttan sonra** gönderilen olayları görür (imleç kayıt anında sona alınır:
  `modules/script/src/bindings.cpp:277-293`). Dinleyici varsayılan fazı `Update`'tir.
- **Zamanlayıcılar:** `after`/`every`/`cancel`; yalnızca `Update` fazında, `ctx.delta` ile azalır (`modules/script/src/host.cpp:319-345`). Modül `failed` ise zamanlayıcılar çalışmaz.
- **`astral.save/load`:** Seçilen bileşenleri taşıyan varlıkları `{"format":"astral.save","version":1}` belgesine yazar; `load` o bileşenlere sahip mevcut varlıkları **yok eder** ve slottakileri yeniden yaratır
  (önce geçici bir World'e doğrulama yüklemesi yapılır). Slot adı harf/rakam/`_`/`-` olabilir; `<save_folder>/<slot>.json`, atomik yazım (geçici dosya + rename)
  (`modules/script/src/bindings_services.cpp:58-122`, `modules/app/src/services/script_bridge.cpp:38-87`).
- **Rastgelelik:** `astral.random()` tek, paylaşılan `std::minstd_rand` (tohum 1); `astral.seed(n)` sıfırlar. Çalışma ortamlarında aynı sonucu verir (`modules/script/src/bindings.cpp:505-514`), ama betik `os.time()`/`os.clock()` kullanırsa
  determinizm kendi sorumluluğundadır.

### `astral.d.luau` / `API.md` üretimi ve parite

`generate_script_api(types, events, owners)` iki metin üretir: `astral.d.luau` (bileşenler `C_<ad>`, olaylar `E_<ad>`, `Components`/`Events` eşlemeleri, `declare astral: {...}`) ve `API.md` (çağrı tablosu, bileşen/olay alanları,
sahip özellik adı) (`modules/script/src/api.cpp:217-247`, `modules/script/src/api.cpp:249-301`, `modules/script/src/api.cpp:329-336`). Çağrılabilir yüzeyin tek kaynağı `kScriptSymbols` kataloğudur. `App` bunu
`ScriptService::write_script_api(folder)` ile (oyunlarda `--script-api <klasör>` / `--script-api-only <klasör>`) yazar (`modules/app/src/services/script_bridge.cpp:208-223`).
**Parite testi** (`modules/script/tests/api_parity_test.cpp:45-123`): canlı VM'de `astral` tablosu gezilir; bulunan her fonksiyon anahtarı (`physics.raycast` …) katalogdakilerle **birebir aynı küme** olmalıdır ve
her `TypeRegistry` girdisi/alanı hem `astral.d.luau` hem `API.md` içinde geçmelidir. `Entity` yöntemleri ise yalnızca üretilen `astral.d.luau` metninde aranır, canlı VM'de denetlenmez (`modules/script/tests/api_parity_test.cpp:113-117`).

### `astral-luau-check`

Üretilen `astral.d.luau`'ya karşı betikleri Luau'nun kendi analizörüyle tip denetler (`tools/astral_luau_check/main.cpp:40-79`; kullanım ve CTest bağlantısı için bkz. [tools.md](../tools.md)). Denetleyici **Nonstrict** modda
çalışır (`tools/astral_luau_check/main.cpp:47`); dosya `--!strict` ile isterse sıkı moda geçer.
Gözlem (2026-11-26, yerel `build/debug` aracı, blank şablonunun ürettiği tanımlarla): yanlış faz sabiti, `entity:get("astral::Nope")` ve `t.positionn` (bir `entity:get` sonucunda) yakalandı; ancak
`astral.system{ update = function(ctx, _, transform, spin) ... spin.axiss ... end }` içinde **bileşen parametrelerinin alan adları denetlenmedi** (tanım `...any` verir: `modules/script/src/api.cpp:223`),
`query = {"OlmayanBilesen"}` dizgesi statik olarak denetlenmedi (çalışma zamanında "unknown component" olur: `modules/script/src/bindings.cpp:26`) ve `astral.on("astral::Contact", function(ctx, ev) ev.yok end)` içindeki olay alanı da raporlanmadı.

## Güçlü yanlar

- **Yükleme atomik.** Başarısız `load`, dünyayı/tip/olay kayıtlarını değiştirmez ve eski modülü korur (ADR-0044; 9 regresyon testi `modules/script/tests/msr_regression_test.cpp`).
- **Bir modülün hatası diğerlerini durdurmaz.** Hata `file:line` ile adlandırılır; modül `failed` olur, oyun sürer (`modules/script/tests/script_test.cpp:150-163`). Bilinmeyen bileşen/alan hataları var olan adları listeler (`modules/script/src/bindings.cpp:26`, `modules/script/src/bindings.cpp:82`).
- **Tek kaynaklı API belgesi.** Katalog + yansıtma, `API.md`, `astral.d.luau`, Inspector keşfi ve çalışma zamanı bağlamaları arasında parite testi var; yeni bağlama katalogda unutulursa test düşer.
- **Sınırlı yüzey.** `io` yok, `os` kısıtlı, dosya/ağ yok; fizik komutları kuyruğa alınır ve deterministik sırayla uygulanır (ADR-0043); kayıt yalnızca adlandırılmış slotlara.
- **Gömülü Luau, harici bağımlılık yok.** Luau kaynaktan statik derlenir; public başlıklarda Luau türü yok (`modules/script/CMakeLists.txt:1-2`).
- **Bayt kodu cook'u.** `astral-cook` betikleri derler; sözdizimi hataları derleme zamanında bulunur (`modules/script/tests/compile_test.cpp:30`).

## Zayıf yanlar ve bilinen sınırlar

- **Zaman bütçesi duvar saatidir ve bir faz için tüm modüllerle paylaşılır.** `run` başında bir kez kurulur; tüm modüllerin toplam süresi 100 ms'yi aşarsa sıradaki modül suçlanır (`modules/script/src/host.cpp:352-376`). Bütçe aşıldığında son tarih
  `time_point::max()`'e **sıfırlanır** (hata bir kez raporlansın diye), yani aynı `run` çağrısında sonra çalışan modüllerin bütçesi kalmaz (`modules/script/src/host.cpp:21-24`). Duvar saati yük altında sahte aşımlar üretir: bu PC'de paralel yük altında
  `pong_tests` ve `spinning_cubes_tests` kararsız, seri çalışınca geçiyor (ana ajan ölçümü; neden olarak bütçe varsayımı, kesin kök neden araştırılmadı).
- **Bellek sınırı yok.** `luaL_newstate` varsayılan ayırıcıyla çağrılır, betik başına bellek/yığın kotası konmamış (`modules/script/src/host.cpp:93`). Zaman bütçesi bytecode-seviyesindedir (`interrupt`); tek bir uzun yerel çağrının (örn. çok büyük `string.rep`)
  kesilebileceği doğrulanmadı.
- **Tek `lua_State`, tek iş parçacığı.** Betikler ana iş parçacığında ve ardışık çalışır; paralel betik yok. Aynı `astral.random` üreteci tüm modüller arasında paylaşılır (`modules/script/src/host_impl.hpp:80`).
- **Olay penceresi.** World olayları yalnızca iki `update_events()` çağrısı boyunca yaşar (`modules/world/include/astral/world/world.hpp:46`); `FixedUpdate` fazında dinleyen betik, ardışık iki frame'de sabit adım atılmazsa
  (yüksek FPS + düşük `physics_rate`) o olayı kaçırabilir. Kod okumasından çıkarım, test ya da yeniden üretim yok (**doğrulanmadı**).
- **`astral-luau-check` beklenenden az yakalar.** Sistem/davranış geri çağrılarındaki bileşen alanları ve `query` dizgeleri statik denetlenmez (yukarıdaki gözlem); `AstralGame.cmake`'deki "yanlış bileşen, alan ya da olay adı ya da tür derlemeyi düşürür" ifadesi
  (`cmake/AstralGame.cmake:80-84`) yalnızca `entity:get`/`add` gibi tipli yollar için doğrudur. Denetleyicinin kendi testi yok (yalnızca tüketici/şablon projelerinde dolaylı).
- **Hot reload yoklama tabanlı.** 0,5 sn'de bir tüm izlenen dosyalar ana iş parçacığında okunup özetlenir; yeniden yükleme anında modülün tüm dinleyicileri/zamanlayıcıları sıfırlanır (durum bileşenlerde olmalı) (`modules/app/src/services/script_bridge.cpp:225-240`, `modules/script/src/host.cpp:42-61`).
  Zamanlayıcılar yeniden yüklemede sıfırlanır (`modules/script/src/host.cpp:49-56`); `on_start` yeniden çalışmaz, çünkü başlamış örnekler `Impl::started` içinde yol ile tutulur (`modules/script/src/host.cpp:297-302`).
- **Bayt kodu doğrulanmaz.** `ScriptHost::load`, kaynağın ilk 4 baytı `ALBC` ise Luau bayt kodu gibi yükler (`modules/script/src/compile.cpp:12-15`, `modules/script/src/host.cpp:130`); geliştirme modunda gevşek `.luau` dosyası bu önekle başlarsa bayt kodu sayılır.
  Luau, güvenilmeyen bayt kodu için tasarlanmamıştır (Luau belgesi; bu depoda test edilmedi). Pack içeriği güvenilir kabul edilir.
- **API yüzeyi dar ve katalog elle yazılı.** Katalogdaki `luau_type` dizgeleri C++ uygulamasından türetilmez, elle yazılmıştır; parite testi yalnızca **adların** uyuştuğunu denetler, imzaların uygulamayla uyuştuğunu denetlemez.
  Ses, HUD, sahne/harita yükleme, kamera, malzeme ve açısal eklem motoru (`ScriptPhysics` arayüzünde yok) betikten erişilemez; VFX yalnızca `PlayVfx`/`StopVfx` olayıyla.
- **Betik tarafında yerleşik fizik arayüzü varsayılanları sessiz.** `ScriptPhysics`'in yeni sanal yöntemleri (örn. `force_at`, `torque`) varsayılan olarak **hiçbir şey yapmaz** (`modules/script/include/astral/script/script.hpp:117-130`); uygulamayan bir ana bilgisayar hata görmez.
- **`bindings.cpp` 590/600 satır** (`python tools/check_sizes.py` çıktısında `[NEAR]`): yeni bağlama eklemek önce dosyayı bölmeyi gerektirir (`modules/script/src/bindings.cpp:1`).

## Testler

`modules/script/tests/` altında 5 dosya, 35 `TEST_CASE`, tek yürütülebilir `astral_script_tests` (`modules/script/CMakeLists.txt:36-43`). `modules/app/tests/script_app_test.cpp` ve `services_test.cpp` App entegrasyonunu kapsar.

| Dosya | Kapsam |
|---|---|
| `script_test.cpp` (21) | proxy okuma/yazma, enum/Vec2/varlık dönüşümü, `without`, faz seçimi, hata yalıtımı (söz dizimi, çalışma zamanı, sonsuz döngü, OS erişimi), yeniden yükleme, betik tanımlı bileşenler, olaylar, davranışlar, zamanlayıcılar, tohumlu rastgelelik, `query/each`, `quat`, `tween`, `state_machine`, `save/load`, `physics/fields` çağrıları |
| `msr_regression_test.cpp` (9) | AG07 (dinleyici/sistem eklerken güvenli), AG09 (başarısız yükleme dünyayı bırakır; üst düzeyde dünya değişimi reddi; `spawn` hatasında varlık yok), AG10 (davranış örneği), AG11 (zamanlayıcı hatası), AG12 (kendini içeren tablo) |
| `api_parity_test.cpp` (2) | canlı VM ↔ katalog ↔ `astral.d.luau`/`API.md` ↔ `TypeRegistry`; `save/load` varlık bağlama |
| `api_test.cpp` (1), `compile_test.cpp` (2) | üretilen belge içeriği; bayt kodu aynı çalışır, söz dizimi hatası dosya:satır ile |

**Kapsanmayanlar:** bellek tüketimi/kota, çok modüllü bütçe paylaşımı (aşımdan sonraki modüller), `FixedUpdate` olay kaçırma, bayt kodu bozukluğu, `astral-luau-check` çıktısı, gerçek `ScriptPhysics` uygulaması (`modules/app/src/services/script_physics.cpp` yalnızca App testlerinde).

## Performans

Ölçülmedi (betik yürütme için bu depoda kayıtlı bir kıyas yok). Test ortamı notu: duvar saatli 100 ms bütçe nedeniyle paralel ctest yükünde `pong_tests`/`spinning_cubes_tests` kararsızdır (ana ajan gözlemi, RTX 3060, MSVC 19.51).

## Kararlar

- [ADR-0017 Oyun betikleme](../decisions/0017-gameplay-scripting.md)
- [ADR-0026 Betik ana bilgisayarı ve özellikler](../decisions/0026-script-host-and-features.md)
- [ADR-0028 Betik araçları ve paylaşılan özellikler](../decisions/0028-script-tooling-and-shared-features.md)
- [ADR-0034 Gameplay API ve fizik adım sözleşmesi](../decisions/0034-gameplay-api-and-physics-step-contract.md)
- [ADR-0043 Deterministik fizik komut üreticileri](../decisions/0043-deterministic-physics-command-producers.md)
- [ADR-0044 Aşamalı betik modülü yüklemesi](../decisions/0044-staged-script-module-load.md)
- `docs/decisions/0045-query-filter-ignore-list.md`: `docs/refactor` dalında, bu çalışma ağacında yok.
