# Oyun mimarisi: World, Scene, Prefab, Entity, Component, sistemler

> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi. Her iddia kaynak satırına bağlıdır; çalıştırılarak
> doğrulanmayanlar "çıkarım" veya "doğrulanmadı" diye işaretlidir. Dosya biçimleri için
> [scenes-and-worlds.md](scenes-and-worlds.md), oyun kurulumu için [writing-a-game.md](writing-a-game.md).

## 1. Zihinsel model

```text
Oyun projesi (CMake, Game, FeatureSet, assets)
 ├─ Game / özellik paketleri (C++ sistemler + Luau kuralları)
 └─ Kaynak veri: .scene  .prefab  .world + .ent  (+ .material, .ashader, glTF ...)
                    │ load / expand / stream
App ─ Context ─ World (ECS)
                 ├─ Entity (index + generation)
                 ├─ Component sütunları (archetype başına)
                 ├─ Resource'lar, olay kuyrukları, Commands
                 └─ Sistemler (C++ System + Luau modülleri), fazlara bağlı
                    │ köprüler (bridge)
          Jolt fizik · Renderer · Audio · Luau
```

- **Entity** yalnız kimliktir; **component** düz veridir; **sistem** davranıştır (`modules/world/include/astral/world/world.hpp:116-120`).
- **Scene** (`.scene`) ve **prefab** (`.prefab`) JSON belgeleridir; yüklenince World'e entity olarak girerler. **`.world`** bir
  streaming haritasıdır, ECS'nin kendisi değildir (`modules/world/include/astral/world/map.hpp:1-3`).
- Bir `App` bir `World` sahiplenir; `load_scene` World'e **ekler**, eski sahneyi silmez
  (`modules/app/include/astral/app/scene_service.hpp:17`). Birden fazla World örneği de kurulabilir; her biri benzersiz
  `WorldId` taşır (`modules/world/include/astral/world/world.hpp:35`, `modules/world/include/astral/world/world.hpp:128`).

## 2. Entity ve kimlik katmanları

`Entity` = `Handle<EntityTag>`: `index` + `generation`, `generation == 0` "hiç geçerli değil"
(`modules/world/include/astral/world/detail/archetype.hpp:13`, `modules/core/include/astral/core/handle.hpp:13-18`).
Silinen slot yeniden kullanılırken `generation` artar, eski handle çözülmez.

| Kimlik | Ne | Kalıcı mı | Kaynak |
|---|---|---|---|
| `Entity` | Çalışma zamanı handle'ı | Hayır; kaydetme/ağ anahtarı yapma | `modules/world/include/astral/world/detail/archetype.hpp:13` |
| `WorldEntityRef{WorldId, Entity}` | Handle + sahip World; `world.owns(ref)` | Hayır | `modules/world/include/astral/world/world.hpp:38-43`, `modules/world/include/astral/world/world.hpp:130` |
| `SceneId` / `DocumentEntityId` | Sahne/prefab belgesindeki kayıt numarası (`"2"`, prefab içi `"2/5"`) | Evet, dosyada | `modules/world/include/astral/world/scene.hpp:59-90` |
| `MapEntity::id` | Harita yerleşiminin kalıcı numarası (1 … 2³²−1) | Evet | `modules/world/include/astral/world/map.hpp:41-48` |

- `World::alive(e)` canlılığı doğrular; `e.valid()` yalnız `generation != 0` der (`modules/world/include/astral/world/world.hpp:135`).
- Ölü entity'ye `world.get<T>(e)` `nullptr` döner; ölü entity'ye **`world.add(e, ...)` süreci durdurur**
  (`ASTRAL_ASSERT(s, "add() on a dead entity")`, `modules/world/include/astral/world/world.hpp:388`, her yapılandırmada açık).
  Kareler arası tuttuğun handle'ı kullanmadan önce `alive` kontrol et. Luau'da `entity:alive()` var (`modules/script/src/api.cpp:19`).
- `Name{value}` insan okur addır, benzersiz kimlik değildir; `find_by_name` eşitler arasında sıra vermez
  (`modules/world/include/astral/world/scene.hpp:45-48`, `modules/world/include/astral/world/scene.hpp:161`).

## 3. Component

Component = düz struct. Kurallar ve ne denetlendiği:

| Kural | Denetim | Kaynak |
|---|---|---|
| Move-constructible ve **nothrow** | Derleme zamanı `static_assert` | `modules/world/include/astral/world/component.hpp:66-67` |
| `T*` işaretçisi yalnız bir sonraki yapısal değişikliğe kadar geçerli | Yok (sözleşme) | `modules/world/include/astral/world/world.hpp:140` |
| Yapısal değişiklik (`create/destroy/add/remove`) `each`/`parallel_each` içinde yasak | Çalışma zamanı `ASTRAL_ASSERT`; her yapılandırmada süreci durdurur | `modules/world/src/world.cpp:224-228` |
| Reflect edilebilir: `ASTRAL_REFLECT(Tip, alanlar...)`, en çok 32 alan | `static_assert` | `modules/core/include/astral/core/reflect.hpp:157` |

Depolama: aynı component kümesine sahip entity'ler bir **archetype**'ta sıkı dizilir; component ekleme/çıkarma entity'yi başka
archetype'a taşır (`modules/world/include/astral/world/world.hpp:116-118`). `ComponentId` tipin etiket adresidir; **koşular arasında kararlı değildir**, kaydetme
anahtarı olarak kullanılmaz (`modules/world/include/astral/world/component.hpp:11-16`).

```cpp
namespace arena { struct Health { int value = 100; }; struct Player { float speed = 6.0f; }; struct Enemy {}; }
ASTRAL_REFLECT(arena::Health, value)
ASTRAL_REFLECT(arena::Player, speed)
ASTRAL_REFLECT_EMPTY(arena::Enemy)     // alansız etiket component'i

Entity e = ctx.world.create(Transform{}, arena::Health{}, arena::Player{});
if (auto* h = ctx.world.get<arena::Health>(e)) h->value -= 10;   // mutable get = "değişti" damgası
ctx.world.add(e, arena::Enemy{});
ctx.world.remove<arena::Enemy>(e);
ctx.world.each<const arena::Health>([&](Entity ent, const arena::Health& hp) {
    if (hp.value <= 0) ctx.world.commands().destroy(ent);          // yapısal değişiklik = komut kuyruğu
});
```

(`ASTRAL_REFLECT_EMPTY` kullanımı `modules/app/include/astral/app/components.hpp:300` ile aynı kalıp.)

### 3.1 Kayıt: sahne dosyasında ve Luau'da görünmek

`ctx.types` (`TypeRegistry`) kaydedilebilir component'leri adıyla tutar. Motorun kendi component'leri `App` kurulurken
kayıtlıdır; seninkileri `ctx.types.add<T>()` (özellik içinde `feature.component<T>()`) ile **sahneyi yüklemeden önce** eklersin
(`modules/world/include/astral/world/serialize.hpp:288-320`). Yüklenmeyen/bilinmeyen component adı `LoadResult::errors`'a
yazılır ve atlanır (`modules/world/src/serialize.cpp:161`).

Motorun kayıtlı component'leri (`modules/app/src/app.cpp:45-79`, `modules/world/src/serialize.cpp:83-88`):

| Grup | Component'ler |
|---|---|
| Dünya | `Name`, `Transform`, `Parent`, `LevelInstanceMember`, `CrossCellRef` |
| Görünüm | `Camera`, `Atmosphere`, `PointLight`, `RectLight`, `Sdf`, `ModelRef`, `PrimitiveMesh`, `TerrainRef` |
| Fizik | `RigidBody`, `CharacterController`, `Joint`, `JointMotor`, `WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil` |
| Diğer | `AudioSource`, `AudioListener`, `StreamingSource`, `Script` (Luau, `modules/script/src/host.cpp:92`) |

Çalışma zamanı üretimi olan `ModelPiece`, `TerrainTileDraw` ve VFX iç component'leri kaydedilmez/kaydetmeye dahil edilmez
(`types.exclude<...>`, `modules/app/src/app.cpp:57`, `modules/app/src/services/vfx_runtime.cpp:185-189`,
`modules/app/src/services/render_bridge.cpp:144`).

**Luau'da tanımlanan component'ler** `astral.define_component("game::Cooldown", {left = {type = "f32", default = 0}})`: tipler
`f32 i32 u32 bool vec2 vec3 vec4 quat entity` (string ve enum yok), en çok 256 bayt (`modules/world/include/astral/world/serialize.hpp:349-353`,
`modules/script/src/api.cpp:33-35`). Sahne, kayıt ve inspector bunları C++ component'i gibi bilir.
Yansıtılmış ama alanı iç içe reflect edilen yapı olan alanlar (`PrimitiveMesh.material`) `FieldKind::Other`'dır ve script'e görünmez
(`modules/world/include/astral/world/fields.hpp:28`, `modules/script/src/api.cpp:156`).

### 3.2 Değişiklik izleme ve resource

- Mutable `get<T>` / `each<T&>` o component'i "değişti" sayar; salt okuma için `const T` yaz (`modules/world/include/astral/world/world.hpp:160-165`). Bir sistemin içinde
  `world.query<const Health>().changed<Health>().each(...)` "bu sistem en son koştuğundan beri değişenler" demektir; App her sisteme
  kendi başlangıç noktasını verir (`modules/world/include/astral/world/world.hpp:178-182`, `modules/world/include/astral/world/world.hpp:282-291`).
- `world.set_resource<T>(value)` / `world.resource<T>()`: tip başına bir değer, World'e ait (`modules/world/include/astral/world/world.hpp:214-217`).
- `Commands`: `spawn/destroy/add/remove` kaydeder, `apply_commands()` uygular; kayıt iş parçacığı güvenlidir
  (`modules/world/include/astral/world/world.hpp:84-90`). App her faz sonunda uygular (`modules/app/src/services/frame_loop.cpp:118-123`).

## 4. Hiyerarşi ve transform

```cpp
Entity player = ctx.world.create(Name{"Player"}, Transform{});
Entity camera = ctx.world.create(Name{"Camera"}, Transform{.position = {0, 1.7f, 0}}, Parent{player}, Camera{});
```

- `Transform` yerel, `Parent{Entity}` onu başka bir entity'ye göre yapar, `WorldTransform` hesaplanmış dünya matrisidir
  (`modules/world/include/astral/world/transform.hpp:10-24`). `WorldTransform`'u **yazma**.
- `update_transforms` her karede `PostUpdate`'ten sonra koşar (`modules/app/src/services/frame_loop.cpp:84-86`); yani
  `Update`/`FixedUpdate` içinde okuduğun `WorldTransform` bir önceki karenindir. Var olmayan ebeveyn yok sayılır, döngüler kırılır
  (`modules/world/include/astral/world/transform.hpp:28-30`).
- **Ebeveyn silinince çocuklar silinmez** (`World::destroy` yalnız o entity'yi siler, `modules/world/src/world.cpp:276-286`). Çocukları sen
  yok et; prefab parçaları ayrıdır: instance silinince parçalar da gider (`modules/world/include/astral/world/scene.hpp:23-26`).
- `RigidBody` **yalnız kök** entity'de desteklenir; `desc` oluşturulurken bir kez okunur, şekli/hareket tipini değiştirmek için
  component'i değiştir (`modules/app/include/astral/app/components.hpp:83-87`).
- Kamera `−Z`'ye bakar, Y yukarı (`modules/app/include/astral/app/components.hpp:74-75`). Döndürme dosyada `[x, y, z, w]` (`modules/world/src/level_instance.cpp:40-47`).

## 5. Sistemler, fazlar ve olaylar

### 5.1 Kare sırası

`FrameLoop::run_frame` (`modules/app/src/services/frame_loop.cpp:64-91`):

| Sıra | Aşama | Not |
|---|---|---|
| 1 | Girdi yönlendirme | `modules/app/src/services/frame_loop.cpp:66`, `modules/app/src/services/frame_loop.cpp:93-96` |
| 2 | `world.update_events()` | Geçen karede gönderilen olaylar bu karede hâlâ okunur, sonra düşer (`modules/app/src/services/frame_loop.cpp:67`) |
| 3 | Dünya streaming | Açık harita varsa (`modules/app/src/services/frame_loop.cpp:68-71`) |
| 4 | Asset/materyal/script/prefab/model senkronu | Hot reload burada (`modules/app/src/services/frame_loop.cpp:109-116`) |
| 5 | `PreUpdate` | C++ sistemleri → Luau → komutlar uygulanır |
| 6 | `Game::on_update` | `PreUpdate`'ten sonra, `Update`'ten önce (`modules/app/src/services/frame_loop.cpp:75`, `modules/app/src/services/frame_loop.cpp:125-129`) |
| 7 | `Update` | |
| 8 | Fizik: 0…N × (alan yeniden kurulumu → `FixedUpdate` → komutlar → gövde senkronu → fizik komutları → kinematik/karakter → alan kuvvetleri → `step`) | `modules/app/src/services/physics_bridge.cpp:378-426` |
| 9 | Çarpışma/tetik olayları gönderilir, simüle edilen transform'lar yazılır | `modules/app/src/services/physics_bridge.cpp:423-425` |
| 10 | `PostUpdate` | |
| 11 | Atmosfer, `update_transforms`, primitif/ses senkronu, render | `modules/app/src/services/frame_loop.cpp:85-88` |
| 12 | Debug/HUD temizlenir, `time.elapsed`/`frame` ilerler | `modules/app/src/services/frame_loop.cpp:180-185` |

Her fazda önce o fazın C++ sistemleri, sonra Luau modülleri koşar (`modules/app/src/services/frame_loop.cpp:118-123`); ikisinin arasında paylaşılan durum
component'tir. Faz sırası kare sırası testleriyle sabitlenmiştir (`modules/app/tests/frame_order_test.cpp:85`, `modules/app/tests/frame_order_test.cpp:142`).

`Time`: `delta` önceki karenin süresi (saat `max_delta` ile kelepçeli, `modules/core/src/clock.cpp:19-28`), `fixed_delta` `1/physics_rate`
(`modules/app/src/app.cpp:296`), `elapsed`, `frame` (`modules/app/include/astral/app/components.hpp:265-270`). Fizik birikimi en çok
`max_physics_steps` (varsayılan 4) adım atar; fazlası **düşer**, birikim bir adıma kırpılır (`modules/app/src/services/physics_bridge.cpp:414`,
`modules/app/include/astral/app/app.hpp:225`).

### 5.2 C++ sistemi, erişim bildirimi ve paralellik

```cpp
class Gravity final : public System {
public:
    void configure(SystemAccess& access) override { access.query<Transform, const Falling>(); }
    void update(Context& ctx) override {
        ctx.world.each<Transform, const Falling>([&](Transform& t, const Falling& f) { t.position.y -= f.speed * ctx.time.delta; });
    }
};
ctx.add_system<Gravity>(Phase::Update);
```

- `SystemAccess` okunan/yazılan component'leri bildirir; bildirmeyen sistem **dışlayıcı** sayılır ve tek başına ana iş parçacığında koşar
  (`modules/app/include/astral/app/system.hpp:22-55`).
- Zamanlayıcı, kayıt sırasındaki **ardışık** ve birbiriyle çakışmayan sistemleri dalgalar halinde paralel koşturur
  (`modules/app/src/services/system_scheduler.cpp:108-125`); `AppDesc::parallel_systems` / `Context::set_parallel_systems` ile kapanır
  (`modules/app/include/astral/app/app.hpp:253-254`, `modules/app/include/astral/app/app.hpp:192-194`).
- Bildirilen erişimi doğrulayan bir mekanizma kodda bulunamadı: yanlış bildirim yarış durumudur (doğrulanmadı: çalışma zamanı denetimi yok).
- Sistem sırası **anlam taşır**: aynı entity'ye çakışan fizik setter'larında sonra kayıtlı olan kazanır (§7). `parallel_each(jobs, fn)` içinde
  `fn` yalnız kendi entity'sine dokunmalı (`modules/world/include/astral/world/world.hpp:166-168`).
- Hafif biçim: `ctx.add_system(Phase::Update, "ad", [](Context& c) {...}, SystemAccess{}.query<...>())` (`modules/app/include/astral/app/app.hpp:144-150`).

### 5.3 Olaylar

```cpp
struct Damage { Entity target; int amount = 0; };
ASTRAL_REFLECT(Damage, target, amount)            // script'e açmak için; yalnız C++ içinde kalacaksa gerekmez
ctx.events.add<Damage>();                         // adla gönder/dinle (Luau): FeatureContext::event<T>() de yapar
ctx.world.send(Damage{target, 30});               // C++ gönderimi

class Hurt final : public System {                // C++ okuyucu
    EventReader<Damage> damage_;
public:
    void update(Context& ctx) override {
        damage_.read(ctx.world, [&](const Damage& d) { /* ... */ });
    }
};
```

- Olay iki `update_events()` çağrısı yaşar; bir `EventReader` **en az iki karede bir** okumalı yoksa olay kaçar
  (`modules/world/include/astral/world/world.hpp:46-47`, `modules/world/include/astral/world/world.hpp:329-331`). Her okuyucu her olayı bir kez görür.
- `World::send` iş parçacığı güvenlidir (olay kuyruğu mutex'li, `modules/world/include/astral/world/world.hpp:450-456`). `EventReader::read` içinde aynı
  tipte gönderirsen olay **sonraki okumada** görülür (`modules/world/include/astral/world/world.hpp:55-59`).
- Luau: `astral.on("game::Damage", function(ctx, e) ... end, "FixedUpdate")`; varsayılan faz `Update`. Dinleyici kaydedildiği andan
  **sonra** gönderilen olayları görür (`modules/script/src/bindings.cpp:288-290`). Bilinmeyen olay adı hata verir.
- Motor olayları: `Contact`, `TriggerEntered`, `TriggerExited`, `JointBroken`, `JointDetached` (script'e açık,
  `modules/app/src/app.cpp:70-74`), `PlayVfx`, `StopVfx`, `DisperseSmoke` (`modules/app/src/services/vfx_runtime.cpp:181-184`).
  `OriginShifted` yalnız C++ `EventReader<OriginShifted>` ile okunur; `EventRegistry`'ye eklenmiş bir kayıt bulunamadı
  (`modules/app/src/services/world_bridge.cpp:148`).
- Fizik olayları adımlardan **sonra** gönderilir (`modules/app/src/services/physics_bridge.cpp:423`): aynı karenin `PostUpdate`'inde ve sonraki karenin
  fazlarında okunabilir; `Update`'te dinleyen bir Luau modülü onları bir sonraki karede görür.

## 6. Luau ile gameplay

Luau yalnız `astral` tablosuna uzanır; tam yüzey oyunun kendi `API.md` + `astral.d.luau` çıktısıdır
(`oyun --script-api docs/api`; katalog `modules/script/src/api.cpp:9-125`). Özet:

```lua
astral.system { name = "x", phase = "FixedUpdate", query = {"A", "B"}, with = {...}, without = {"C"},
                update = function(ctx, entity, a, b) end }            -- her eşleşen entity için
astral.system { phase = "Update", run = function(ctx) end }           -- fazda bir kez
astral.on("event::Name", function(ctx, payload) end, "FixedUpdate")  -- varsayılan faz: Update
astral.send("event::Name", { field = value })
astral.query({"A","B"})  astral.each({"A","B"}, function(entity, a, b) end)
astral.spawn { ["astral::Name"] = {value = "x"} }  astral.find("x")
entity:get("A")  :has  :add  :remove  :destroy()  :alive()  :id()
astral.after(s, fn)  astral.every(s, fn)  astral.cancel(id)  astral.random()  astral.seed(n)
astral.tween  astral.state_machine  astral.ease  astral.lerp  astral.quat.*  astral.input.*
astral.physics.*  astral.fields.wind/water  astral.save/load/has_save
```

- `ctx` içinde `delta`, `fixed_delta`, `time` (`modules/script/src/host.cpp:354-359`). Sistem sırası: modül yükleme sırasıyla,
  her modülde önce dinleyiciler sonra sistemler; ardından `behaviour`'lar ve `Update`'te zamanlayıcılar (`modules/script/src/host.cpp:363-378`).
- `astral.behaviour{on_start, on_update, on_fixed_update}` `astral::Script{path}` component'li entity'ler içindir (`modules/script/src/api.cpp:26-28`).
- Değerler: `vector` (Vec3), `{x=,y=}` (Vec2), `{x=,y=,z=,w=}` (Vec4/Quat), enum adı string, entity alanı entity ya da `nil`
  (`modules/script/src/api.cpp:135-150`).
- **Aşamalı modül yükleme (ADR-0044):** modülün üst düzey kodu bir *bildirim aşamasıdır*. Dünyayı değiştiren çağrılar (`astral.spawn`,
  `entity:add/remove/destroy`, alan yazma, `astral.send`, `astral.save/load`, fizik komutları) üst düzeyde hata verir; global
  `on_load()` işlevinde veya bir callback içinde çağır (`modules/script/src/bindings.cpp:59-61`, `modules/script/src/host.cpp:173-183`).
  Başarısız yükleme dünyayı, tip ve olay kayıtlarını değiştirmez (`modules/script/tests/msr_regression_test.cpp:140`, `modules/script/tests/msr_regression_test.cpp:161`).
- **Hata ve bütçe:** callback hatası modülü durdurur (`ScriptHost::failed`), `dosya:satır: mesaj` loglanır; dosyayı düzeltince yeniden
  yüklenir (`modules/script/include/astral/script/script.hpp:167-177`). Bir faz (veya modül yüklemesi) **100 ms**'yi aşarsa hata ile
  kesilir (`modules/script/include/astral/script/script.hpp:155-158`; `AppDesc`'ten ayarlanamaz, `modules/app/src/services/script_bridge.cpp:113`).
- Sandbox: Luau standart kütüphaneleri açık (`luaL_openlibs`) ve `luaL_sandbox` ile salt okunur yapılmış
  (`modules/script/src/host.cpp:95`, `modules/script/src/host.cpp:108`). Dosya/süreç erişimi ve `require` yoktur (Luau'nun kendi davranışı; bu repoda ayrıca
  sınanmadı). Determinizm için `os.time/os.clock` yerine `astral.random()`/`ctx.time` kullan.
- Durumu script'te değil **component'te** tut: hot reload ve kayıt bunu varsayar (`modules/script/include/astral/script/script.hpp:2-4`).
- Dinamik component şeması çalışırken değişmez; adı tekrar tanımlama (`modules/world/include/astral/world/serialize.hpp:349-353`).

## 7. Fizik komut sözleşmesi (ADR-0034, ADR-0043)

Fiziği `PhysicsService` (`ctx.physics_service()`) üzerinden kullan; `ctx.physics` (`PhysicsWorld`) doğrudan, adımı sana bırakan alt
seviye yoldur ve sözleşmenin dışındadır (`modules/app/include/astral/app/app.hpp:78`, test: `modules/app/tests/physics_step_contract_test.cpp:198`).

**Sabit adım döngüsü** (`modules/app/src/services/physics_bridge.cpp:378-426`):

1. Kare başında gövde senkronu (yeni `RigidBody`'ler gerçek gövde olur).
2. `accumulator += delta`; her sabit adım için (en çok `max_physics_steps`): alan dünyası yeniden kurulur → **`FixedUpdate` sistemleri**
   (C++ sonra Luau) → `apply_commands` → gövde ve eklem senkronu → **kuyruktaki fizik komutları uygulanır** → kinematik hareket →
   karakterler → alan kuvvetleri → `PhysicsWorld::step`.
3. Adımlardan sonra temas/tetik olayları `World::send` ile gönderilir, dinamik gövdelerin `Transform`'u yazılır.

**Komutlar** (`add_impulse`, `add_force`, `add_force_at`, `add_torque`, `add_angular_impulse`, `set_linear_velocity`,
`set_angular_velocity`, `set_body_transform`, `wake/sleep`, eklem motoru ve `break_joint`) kuyruğa girer, iş parçacığı güvenlidir ve
**adım sınırında** uygulanır (`modules/app/include/astral/app/physics_service.hpp:22-31`, `modules/app/include/astral/app/physics_service.hpp:56-79`):

- Sıra anahtarı `(order_key, üretici, sıra)`. `order_key == 0` iken entity'ye göre belirlenir; eşit anahtarlarda **üretici sırası**
  kazanır: sistem dışı kod önce, sonra sistemler **kayıt sırasıyla** (sonraki kazanır), sonra gönderim sırası (ADR-0043,
  `modules/app/include/astral/app/physics_service.hpp:57-60`; test `modules/app/tests/physics_step_contract_test.cpp:380`). Bir sistem kendi paralel iş parçacıklarından
  çakışan komut verirse açık `order_key` vermeli.
- Sıfır adımlı karede biriken komutlar kaybolmaz, sonraki adıma taşınır (`modules/app/tests/physics_step_contract_test.cpp:50`); komut gecikmiş spawn'dan
  sonra verilse de gövde senkronundan sonra uygulanır (`modules/app/tests/physics_step_contract_test.cpp:89`). Ölü entity veya gövdesiz entity komutları atlanır
  (ADR-0034 §3).
- Paralel ve seri zamanlama aynı sonucu verir (`modules/app/tests/physics_step_contract_test.cpp:165`).

**Sorgular** (`raycast`, `raycast_batch`, `spherecast`, `overlap_sphere`, `overlap_box`, `linear_velocity`, `angular_velocity`) **son tamamlanmış
adımın** anlık görüntüsünden okur; `query_state().step_index` hangi adım olduğunu söyler (`modules/app/include/astral/app/physics_service.hpp:15-27`, `modules/app/include/astral/app/physics_service.hpp:42-54`).
`contacts()` `FixedUpdate` içinde önceki tamamlanmış adımın, başka yerde bu karedeki bütün adımların temaslarını verir (`modules/app/include/astral/app/physics_service.hpp:34-37`).
Yani `FixedUpdate`'te bir komut verip aynı sistemde sonucunu sorgudan okuyamazsın.

**Karakter:** `CharacterController::move` (yatay hız, m/s) her kare yazılır; `jump` bir sonraki sabit adımda **bir kez** tüketilir ve o adımda
sıfırlanır (`modules/app/src/services/physics_bridge.cpp:299-313`). Luau: `astral.physics.move_character(entity, velocity, jump?)` aynı
component alanlarını yazar (kuyruğa girmez, `modules/app/src/services/script_physics.cpp:100-107`), `character_state(entity)` son sabit adımın
hız ve zemin durumunu verir. Karakter `Transform`'u fizik yazar; her kare doğrudan `Transform` yazma.

**Tetik ve temas:** `BodyDesc::sensor = true` → `TriggerEntered`/`TriggerExited`; sensör olmayanlar `Contact` (`modules/app/src/services/physics_bridge.cpp:325-345`).
Raycast sonucundan entity: `entity_of(hit)` (`modules/app/include/astral/app/components.hpp:115`). Luau raycast seçenekleri `{layers, ignore, sensors}`;
`ignore` bir entity veya en çok 64 entity listesidir (`modules/script/src/bindings_services.cpp:171-204`).
`AppDesc::physics_interpolation` açıkken dinamik gövdeler son iki adım arasında çizilir ve `Transform` simülasyonun biraz gerisinde kalır
(`modules/app/include/astral/app/app.hpp:250-252`).

Rüzgâr/su alanları `WindZone`/`WaterBody` entity'leriyle kurulur; `AirDrag`, `Buoyant`, `Airfoil` kuvvetleri kendiliğinden uygular
(`modules/app/include/astral/app/components.hpp:117-166`).

## 8. Kaydetme ve yükleme

Üç ayrı mekanizma var; birbirinin yerine geçmez.

| Mekanizma | Ne yazar | Nereye | Notlar |
|---|---|---|---|
| `ctx.scenes().save_scene(yol)` | Dünyadaki kayıtlı component'li her entity (+ `sun`/`environment`), prefab örnekleri **fark** olarak | `asset_root/yol` (gevşek dosya) | Düzenleyici/sahne snapshot'ı; paketli oyunda `asset_root` yazılabilir olmayabilir (`modules/app/src/services/world_bridge.cpp:59-72`) |
| `save_world(world, types)` / `load_world(...)` | Düz anlık görüntü; prefab instance'ları sıradan entity olur | Sen yazarsın (`Json::dump`) | `load_world` **ekler**, dünyayı temizlemez; bilinmeyen component/alan raporlanır ve atlanır (`modules/world/include/astral/world/serialize.hpp:1-15`, `modules/world/include/astral/world/serialize.hpp:404-406`) |
| Luau `astral.save(slot, {"A","B"})` / `astral.load(slot)` | Yalnız listelenen component'ler, bunlara sahip her entity | `<save_folder>/<slot>.json` (varsayılan `saves/`), önce geçici dosya sonra `rename` | `load`: önce başka bir World'e deneme yüklemesi yapılır (bozuk dosya canlı dünyayı bozmaz), sonra listelenen component'lerden birine sahip her entity **yok edilir** ve dosyadakiler yeniden yaratılır (`modules/script/src/bindings_services.cpp:58-108`, `modules/app/src/services/script_bridge.cpp:38-62`) |

Dikkat edilecekler:

- `save_world`/`astral.save` kayıtta olmayan entity'ye giden referansı **`null`** yazar (`modules/world/src/serialize.cpp:108`). `astral.load`
  entity'leri yok edip yeniden yarattığı için, kaydedilmemiş component'lerin tuttuğu eski `Entity` handle'ları geçersizleşir.
- Belgedeki entity numarası dosyadaki sıradır, kalıcı kimlik değildir (`modules/world/src/serialize.cpp:106-120`).
- Şema sürümü: `TypeRegistry::set_schema_version(ad, sürüm, migrate)`; sürüm > 1 olan component'ler `"$schema_version"` yazar (ADR-0034 §1,
  `modules/world/include/astral/world/serialize.hpp:341-344`). Luau slot adı yalnız harf, rakam, `_`, `-` olabilir (`modules/app/src/services/script_bridge.cpp:78-84`).
- Harita hücre farkları (`CellDiff`) bellektedir, **diske yazılmaz** (§ scenes-and-worlds, `modules/world/include/astral/world/map_state.hpp:40-44`).
- Zamanlama: `save_world` mevcut World durumunu o anda yazar; `FixedUpdate`'in ortasında çağırmak yerine `Update`'te çağır (öneri; kodda
  buna dair bir denetim yok).

## 9. Sahne mi dünya mı?

| İhtiyaç | Seçim |
|---|---|
| Menü, oda, arena, tek seviye | `.scene` + prefab'lar; seviye geçişini kendin yaz (`load_scene` ekler; eskiyi sil/kapat) |
| Büyük, akıtılan harita; uzak hücrelerde kalıcı değişiklik | `.world` + `.ent`; tekrar kullanılan parça için level instance |
| Oyun kuralı verisi | Reflect edilmiş ayar component'i veya `feature.json` `settings`; `.world` `settings` serbest nesnedir, otomatik uygulanmaz (`modules/world/include/astral/world/map.hpp:30-34`) |

Ayrıntı ve dosya biçimleri: [scenes-and-worlds.md](scenes-and-worlds.md).

## Bilinen sınırlar

- **Erişim bildirimi doğrulanmıyor:** `SystemAccess` güvene dayanır; yanlış bildirim yarış durumu üretir (`modules/app/include/astral/app/system.hpp:22-55`).
- **Girdi kenarı ve `FixedUpdate` (çıkarım):** girdi karede bir kez güncellenir, `FixedUpdate` karede 0…N kez koşar; `pressed` adımlarda
  birden çok görünebilir veya hiç görünmeyebilir. Tek seferlik isteği `Update`'te component'e yaz, `FixedUpdate`'te tüket (`modules/app/src/services/frame_loop.cpp:66`, `modules/app/src/services/frame_loop.cpp:133-142`,
  `modules/app/src/services/physics_bridge.cpp:388`). Karakter `jump`'ı bunun istisnasıdır: motor tüketir (`modules/app/src/services/physics_bridge.cpp:311`).
- **`FixedUpdate` içinde `WorldTransform` bir önceki karenindir** (`modules/app/src/services/frame_loop.cpp:86`); fizik sonrası güncel konum için `Transform`'a bak
  (dinamik gövdeler için fizik yazar) veya `PostUpdate`'te oku.
- **`save_scene` paketli oyunda anlamsız olabilir** (`asset_root` yazma) ve gameplay kaydı değildir; `astral.save` yalnız seçili component'leri
  yazar ve entity kimliklerini yeniden üretir (§8).
- **Luau yürütme bütçesi sabit 100 ms**, ayarlanamaz; yoğun yükte testler kırılabilir (`modules/script/include/astral/script/script.hpp:155-158`).
- **`OriginShifted` script'e açık değil**; kayan orijin kullanan Luau kodu konum kaymasını bu olayla öğrenemez (`modules/app/src/services/world_bridge.cpp:148`).
- **`add` ölü entity'de süreci durdurur** (hata dönmez, `modules/world/include/astral/world/world.hpp:388`); Luau'da ölü entity erişimi hata verir.
- **Ebeveyn silme çocukları silmez**; `RigidBody` yalnız kök entity'de (`modules/app/include/astral/app/components.hpp:83-87`).
- **Doğrulanmadı:** `parallel_each` + `Commands` birleşiminin tüm iç durum kombinasyonlarında iş parçacığı güvenliği; yalnız `Commands::push`
  mutex'i ve `Events` kuyruğu mutex'i kodda görüldü (`modules/world/include/astral/world/world.hpp:107-111`, `modules/world/include/astral/world/world.hpp:450-456`).
