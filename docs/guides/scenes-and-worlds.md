# Sahneler, prefab'lar ve açık dünya: dosya biçimleri ve streaming

> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi. Biçimler `modules/world/src/*.cpp` okuyucularından ve
> `templates/blank/assets/main.scene`'den türetildi; JSON örnekleri bu oturumda motora yüklenerek denenmedi (kaynakta ilgili test
> dosyaları belirtildi). Mimari arka plan için [game-architecture.md](game-architecture.md).

## 1. Dosya türleri

| Tür | `format` | Ne | Runtime karşılığı | Okuyucu |
|---|---|---|---|---|
| `.scene` | `astral.scene` v1 | Entity kayıtları listesi (seviye, oda, menü) | Dünyaya **eklenen** entity'ler (`SceneId`) | `modules/world/src/scene.cpp:113` |
| `.prefab` | `astral.scene` v1 | Tek köklü yeniden kullanılır entity ağacı | `PrefabInstance` + `PrefabPart`'lar | `modules/world/src/prefab.cpp:386` |
| `.world` | `astral.map` v1 | Harita: ayarlar, streaming ızgaraları, veri katmanları | `MapRuntime`, hücre akışı | `modules/world/src/map.cpp:50` |
| `.ent` | `astral.entity` v1 | Bir haritaya yerleşmiş tek entity (veya level instance) | `CellMember` + kalıcı `MapEntity::id` | `modules/world/src/map.cpp:97` |
| `.cell` | `astral.cell` v1 | Cook'un ürettiği hücre: `.ent` belgeleri listesi | Hücre yüklenir/boşaltılır | `modules/world/src/map.cpp:203-239` |
| `astral.world` | `astral.world` v1 | `save_world` düz anlık görüntüsü (kayıt) | Harita değil | `modules/world/include/astral/world/serialize.hpp:1-15` |

Format ve sürüm alanı yanlışsa belge **reddedilir** ve hiçbir entity yaratılmaz: `load_scene` ve `load_world` bunu açıkça yapar
(`modules/world/include/astral/world/scene.hpp:150-152`, `modules/world/include/astral/world/serialize.hpp:405`). Diğer sorunlar
(`LoadResult::errors`) raporlanır ve ilgili kayıt/alan atlanır: yükleme **işlemsel (transactional) değildir**, yarım yüklenmiş sahne kalabilir.
`ModelRef` (glTF), `PrimitiveMesh` (küp/küre/düzlem + materyal) ve `.material` dosyaları sahneye görsel veri taşır;
`MeshRenderer` ham `MeshHandle/MaterialHandle` tutar ve dosyaya yazılmaz (`modules/app/include/astral/app/components.hpp:14-33`).

## 2. `.scene`

Gerçek şablon sahnesi `templates/blank/assets/main.scene` (kamera, zemin, küp):

```json
{
  "format": "astral.scene",
  "version": 1,
  "sun": { "direction": [-0.4, -1.0, -0.5], "intensity": 1.6 },
  "environment": { "ambient": 0.3, "exposure": 0.6 },
  "entities": [
    { "id": 1, "components": {
        "astral::Name": { "value": "camera" },
        "astral::Transform": { "position": [0, 1.2, 4.5], "rotation": [-0.1, 0, 0, 0.995] },
        "astral::Camera": {} } },
    { "id": 2, "components": {
        "astral::Name": { "value": "floor" },
        "astral::Transform": { "position": [0, -0.6, 0], "scale": [12, 1, 12] },
        "astral::PrimitiveMesh": { "shape": "Plane", "material": { "base_color": [0.3, 0.32, 0.36, 1], "roughness": 0.8 } } } }
  ]
}
```

- **Kayıt kimliği (`id`)** zorunludur: tamsayı 0 … 2³²−1; eksik/geçersiz/yinelenen `id` hata verip o kaydı atlar
  (`modules/world/src/scene.cpp:132-141`, `modules/world/src/prefab.cpp:75-80`). Yüklenen her entity'ye `SceneId` konur.
- **Üst düzey `sun` ve `environment`** isteğe bağlıdır; varsa `ctx.sun`/`ctx.environment`'ı değiştirir
  (`modules/app/include/astral/app/scene_service.hpp:17-19`, `modules/app/src/services/world_bridge.cpp:35-57`). Alanlar
  `DirectionalLight` ve `Environment`'ın reflect alanlarıdır (`modules/render/include/astral/render/renderer.hpp:454-455`).
  Aktif bir `Atmosphere` bileşeni varsa güneş/gökyüzünü o sürer (`modules/app/include/astral/app/components.hpp:44-61`).
- **Component değerleri:** `"ad::Tip": {alan: değer}`. Eksik alan varsayılanını korur, bilinmeyen alan yok sayılır, bilinmeyen component adı
  hata olarak raporlanır (`modules/world/include/astral/world/serialize.hpp:14-15`). `Vec3` `[x,y,z]`, `Quat` `[x,y,z,w]`, enum **adıyla**
  (`"shape": "Plane"`), entity referansı kayıt `id`'si (`"astral::Parent": {"entity": 1}`).
- **Kendi component'in** dosyada görünsün diye `ASTRAL_REFLECT` + `ctx.types.add<T>()` (özellik içinde `feature.component<T>()`) **sahneyi
  yüklemeden önce** (bkz. [game-architecture.md](game-architecture.md#31-kayıt-sahne-dosyasında-ve-luauda-görünmek)).
- Kod: `ctx.scenes().load_scene("levels/arena.scene")` (asset köküne göre); dönen `LoadResult::errors` ve log bakılmalı.
  `save_scene(yol)` dünyayı (kayıtlı component'li her entity + `sun`/`environment`) `asset_root/yol` altına yazar
  (`modules/app/src/services/world_bridge.cpp:59-72`); bu bir sahne anlık görüntüsüdür, oyun kaydı değil.
- **Hot reload:** prefab, materyal, shader, script, actions, model dosyaları çalışırken yeniden yüklenir
  (`modules/app/src/services/frame_loop.cpp:109-116`). Bir kez yüklenmiş **kök `.scene` dosyası yeniden uygulanmaz**; kaynakta böyle bir yol yok.

## 3. `.prefab` ve instance'lar

Prefab = `astral.scene` biçiminde, **tam olarak bir kökü** olan belge (kök: `astral::Parent` taşımayan tek kayıt; hiç/birden fazla ise hata,
`modules/world/src/prefab.cpp:399-410`). Çocuklar `astral::Parent` ile köke bağlanır:

```json
{ "format": "astral.scene", "version": 1, "entities": [
  { "id": 1, "components": { "astral::Name": {"value": "lamp"}, "astral::Transform": {}, "astral::PrimitiveMesh": {"shape": "Cube"} } },
  { "id": 2, "components": { "astral::Name": {"value": "light"}, "astral::Parent": {"entity": 1},
                             "astral::Transform": {"position": [0, 1, 0]}, "astral::PointLight": {"intensity": 15, "radius": 8} } }
] }
```

Sahnede yerleştirme yalnız **farkı** yazar:

```json
{ "id": 3, "prefab": "prefabs/lamp.prefab",
  "components": { "astral::Transform": { "position": [3, 0, 0] } },
  "parts": { "2": { "components": { "astral::PointLight": { "intensity": 8 } } } },
  "removed": [], "deleted": [] }
```

(`modules/world/include/astral/world/scene.hpp:1-26`):

- **Referanslar:** üst düzey kayıt `3`, prefab içi parça `"3/2"` (iç içe prefab'ta segment eklenir). `parts` anahtarı parçanın yoludur.
- **Override = birleştirme yaması:** nesneler alan alan birleşir, diğer her şey yer değiştirir (`modules/world/include/astral/world/scene.hpp:17-19`). `removed` kökten çıkarılan
  component adlarını, `parts.<yol>.removed` parçadan çıkarılanları, `deleted` silinen prefab entity yollarını (çocuklarıyla) listeler
  (`modules/world/src/prefab.cpp:49-62`, `modules/world/src/prefab.cpp:176-195`).
- **İç içe ve varyant:** prefab başka prefab içerebilir; kökün kendisi başka prefab'ın instance'ı olabilir (varyant). Kendini içeren prefab hata
  (`modules/world/include/astral/world/scene.hpp:20-21`, `modules/world/src/prefab.cpp:456-461` yol çakışması denetimi).
- **Çalışma zamanında:** instance `PrefabInstance{path}` taşıyan entity; parçaları `PrefabPart{instance, path}` taşır, instance ile birlikte
  yok edilir ve `save_scene`'de ayrı kayıt olmaz, instance'ın override'ı olarak yazılır (`modules/world/include/astral/world/scene.hpp:23-26`, `modules/world/src/scene.cpp:232-240`).
- **Kodla:** `Entity e = ctx.scenes().spawn_prefab("prefabs/lamp.prefab", {.position = {3, 0, 0}})` parçaları hemen yaratır
  (`modules/app/include/astral/app/scene_service.hpp:22-24`); `find_part(ctx.world, e, "light")` parçayı `Name` ile bulur (`modules/world/include/astral/world/scene.hpp:164`). `world.create(Transform{...},
  PrefabInstance{"x.prefab"})` ile de yerleştirilir: entity'nin kendi component'leri prefab kökününkilerden önceliklidir (`modules/world/include/astral/world/scene.hpp:50-55`).
- **Sync:** `sync_prefabs` her karede çalışır: kodla yaratılan instance'ları yaratır, prefab dosyası değişmiş/yolu değişmiş olanları
  yeniden yaratır (override'lar korunur), ölü instance'ların parçalarını siler (`modules/app/src/services/frame_loop.cpp:114`,
  `modules/world/src/scene.cpp:35-110`). Parça entity handle'ları yeniden yaratmada **değişir**; kalıcı tutma, `find_part` ile yeniden bul.
- **Hata davranışı:** prefab yüklenemezse instance kaydın kendi değerleriyle yaratılır ve prefab düzelince parçalar oluşur; başarılı yüklenmiş
  bir instance son iyi parçalarını korur (`modules/world/src/scene.cpp:20-24`, `modules/world/src/scene.cpp:160-165`). `spawn_prefab` hata verse bile kök entity döner;
  dönen handle tam başarı kanıtı değildir, hatalar loglanır (`modules/app/src/services/world_bridge.cpp:75-79`).

Testler: `modules/world/tests/scene_test.cpp`, `modules/app/tests/scene_app_test.cpp`.

## 4. `.world` + `.ent`: akan harita

Düzen (`modules/app/src/map_runtime.cpp:76-114`):

```text
assets/maps/
  island.world               ← harita: ayarlar, ızgaralar, katmanlar
  island.entities/           ← kaynak (geliştirme): her yerleşim ayrı dosya
    1.ent   2.ent  ...
  (cook sonrası)  island.world  +  island.cells/always.cell, main_0_0.cell, main_-1_2.cell ...
```

`island.world`:

```json
{ "format": "astral.map", "version": 1,
  "settings": {},
  "grids": [ { "name": "main", "cell_size": 128, "load_range": 384 } ],
  "layers": [ { "name": "night", "active": false } ] }
```

- `grids` boşsa tek `main` ızgara (128 m, 384 m) eklenir; `cell_size` pozitif olmalı. `settings` **serbest** nesnedir: oyunun okuması gerekir, hiçbir
  anahtar kendiliğinden yerçekimi/render ayarlamaz (`modules/world/src/map.cpp:50-70`, `modules/world/include/astral/world/map.hpp:30-34`).
- `layers`: `{name, active}` başlangıç durumudur.

`.ent` (her biri bir yerleşim):

```json
{ "format": "astral.entity", "version": 1,
  "grid": "main", "layers": ["night"], "always_loaded": false,
  "entity": { "id": 1, "prefab": "prefabs/lamp.prefab",
              "components": { "astral::Transform": { "position": [200, 0, 20] } } } }
```

- `entity` bir `astral.scene` **kaydıdır** (component'ler veya `prefab` + override'lar); `id` 1 … 2³²−1 arası, harita içinde benzersiz, **kalıcı** kimliktir.
  Eksik/yinelenen/geçersiz `id` raporlanıp dosya atlanır; bilinmeyen `grid` ilk ızgaraya düşer (hata raporlanır)
  (`modules/world/src/map.cpp:97-134`). `grid`, `layers`, `always_loaded` **belge düzeyindedir**, `entity` içinde değil.
- **Hücre atama:** `floor(x / cell_size)`, `floor(z / cell_size)` — zemin düzlemi (x, z); y kullanılmaz. Konum kaydın
  `components."astral::Transform".position` alanından okunur (yoksa `level_instance.position`); **Transform'u prefab'dan gelen** ve kayıtta yazılmayan
  bir entity konumu `(0,0,0)` sayılır, yani hücre (0,0)'a düşer (`modules/world/src/map.cpp:126-132`, `modules/world/src/map.cpp:173`). Konumu her zaman `.ent` içinde yaz.
  Örnek: x=200, z=20, `cell_size` 128 → hücre (1, 0), adı `main_1_0` (`modules/world/src/map.cpp:182-184`).
- **`always_loaded: true`** akıtılmaz; harita açılırken hemen yaratılır (oyun kuralları, gökyüzü, oyuncu için) (`modules/app/src/map_runtime.cpp:120-122`).
- **Katman görünürlüğü:** `layers` boşsa entity her zaman vardır; doluysa listelenen katmanlardan **herhangi biri açıkken** görünür
  (`modules/world/include/astral/world/map_state.hpp:29-30`). `ctx.maps().world_layers()->set("night", true)` bir sonraki güncellemede
  entity'leri yaratır/siler (`modules/app/src/map_runtime.cpp:141-146`).

### 4.1 Açma, akış, kayan orijin

```cpp
if (!ctx.maps().open_world("maps/island.world")) { ctx.quit(); return; }   // false + log: okunamadı
ctx.world.create(Transform{}, StreamingSource{});                           // yoksa aktif kamera kaynak olur
```

`MapService` (`modules/app/include/astral/app/map_service.hpp:12-35`): `open_world`, `close_world`, `finish_world_loading` (menzildeki her hücre yüklenene
kadar bloklar: yükleme ekranı, test), `world_layers`, `world_streaming` (hücre sayıları), `world_origin`, `resolve_map_entity`.

- **Kaynaklar:** aktif `StreamingSource` entity'leri; hiç yoksa ilk aktif `Camera` (`modules/app/src/services/world_bridge.cpp:108-129`). İlk kaynak
  kayan orijini de sürer. Üst düzey konum `Transform`'dan, çocuk ise `WorldTransform`'dan alınır.
- **Yükle/boşalt:** hücre karesi kaynağa `load_range` içindeyse yükleme başlar; `load_range × 1,1`'den uzaktaysa boşaltılır (histerezis,
  `modules/world/include/astral/world/streaming.hpp:45-47`). Hazır hücreler en yakından başlanarak eklenir (`modules/world/include/astral/world/streaming.hpp:52`); her karede zaman bütçesi
  `AppDesc::world_stream_budget_ms` (1 ms) dolana kadar eklenir, **kare başına en az bir hücre** (`modules/app/src/map_runtime.cpp:141-154`,
  `modules/app/include/astral/app/app.hpp:247`). Başarısız eklenen hücre geri çekilme (back-off) ile yeniden denenir (`modules/world/include/astral/world/streaming.hpp:40`).
- **Kaynak (geliştirme) mod ile cooked mod:** `.world` içinde `cells` anahtarı varsa cooked indekstir; hücreler `<ad>.cells/<hücre>.cell` dosyasından iş
  parçacığı havuzunda okunur. Yoksa `<ad>.entities/` klasörü **disk üzerinden** okunur ve bellekte pişirilir; bu yolda hücre verisi bellekte hazır
  olduğu için okuma eşzamansızlığı yoktur ve `.entities/` klasörü paket (`.apak`) içinden okunmaz — dağıtımda `astral-cook` ile pişir
  (`modules/app/src/map_runtime.cpp:63-125`, `modules/assets/include/astral/assets/cook.hpp:68-74`).
- **Kayan orijin:** dosyalar `double` dünya konumu tutar, çalışma zamanında `Transform` `float` ve hareketli bir orijine görelidir. Odak orijinden
  `AppDesc::origin_shift_distance` (2000 m) uzaklaşırsa orijin 100 m'nin katına yuvarlanarak kayar; **y asla kaymaz**; üst düzey transform'lar, fizik
  gövdeleri ve render geçmişi aynı ofseti alır ve `OriginShifted` olayı gönderilir (`modules/world/include/astral/world/origin.hpp:1-49`,
  `modules/app/src/services/frame_loop.cpp:98-107`). Yalnız harita açıkken veya arazi akıtılırken çalışır. `OriginShifted` yalnız C++ `EventReader`
  ile okunur (script'e kayıtlı değil, bkz. [game-architecture.md](game-architecture.md#53-olaylar)).
- **Hücre değişiklikleri:** hücre boşaltılırken oyunun yok ettiği entity'ler ve değiştirdiği kayıtlar (`CellDiff`) hatırlanır, hücre tekrar yüklenince
  uygulanır (`modules/world/include/astral/world/map_state.hpp:40-44`, `modules/world/include/astral/world/map_state.hpp:67-72`). Bu **bellekte** tutulur; diske yazılmaz, `close_world`/oyun
  kapanışında kaybolur.
- **Kalıcı referans:** hücreler arası referans için çalışma zamanı `Entity` yerine `MapEntity::id` + `resolve_map_entity(id)` kullan: sonuç
  `Unknown | Unloaded | Alive | Destroyed` ve `load_count` (`modules/world/include/astral/world/map_state.hpp:46-63`, `modules/app/include/astral/app/map_service.hpp:35`). Yerleşmiş entity'ler `CellMember{cell, id}`
  taşır (`modules/world/include/astral/world/map_state.hpp:16-20`). `CrossCellRef{target_id, target_label}` component'i bu kimliği veri olarak tutar
  (`modules/world/include/astral/world/level_instance.hpp:32-35`).

### 4.2 Bir `.ent` bir entity'dir

`MapState` her `.ent` kaydını **tek kayıtlık bir sahne** olarak yaratır (`modules/world/src/map_state.cpp:12-21`). Sonuç (kaynaktan çıkarım;
haritada `Parent` kullanan test bulunamadı): bir `.ent` kaydı **başka bir `.ent` kaydına referans veremez**, dolayısıyla iki `.ent` arasında
`astral::Parent` kurulamaz. Hiyerarşi gerekiyorsa tek bir **prefab** instance'ı yerleştir (parçaları instance ile gelir).

## 5. Level Instance: bir `.scene`'i haritaya yerleştirmek

Hazır bir `.scene`'i (köy, bina) haritaya `level_instance` kaydıyla yerleştirirsin ([ADR-0036](../decisions/0036-world-authoring-and-level-instances.md); `modules/world/include/astral/world/level_instance.hpp:1-5`):

```json
{ "format": "astral.entity", "version": 1, "grid": "main",
  "entity": { "id": 2, "level_instance": {
      "scene": "levels/courtyard.scene",
      "position": [500, 0, 200], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1],
      "overrides": { "3": { "components": { "astral::PointLight": { "intensity": 20 } }, "removed": [] } },
      "deleted": ["5"] } } }
```

(`modules/world/src/level_instance.cpp:214-274`.) Farkı prefab'tan:

- Kaynak `.scene` **birden çok köke** sahip olabilir. Yerleşim, kaynak sahnenin entity'lerini **dünya konumlu bağımsız harita entity'lerine** açar
  (`expand_level_instance`); yapay ebeveyn zinciri yoktur. `Parent` taşımayan entity'lerin konum/dönüş/ölçeği yerleşim dönüşümüyle birleştirilir
  (`modules/world/src/level_instance.cpp:55-68`, `modules/world/src/level_instance.cpp:118-134`).
- Her açılan entity kararlı bir kimlik alır: `(instance_id, yerel yol)` üzerinden FNV özeti, 1 … 2³¹−1 aralığında (`modules/world/src/level_instance.cpp:196-212`)
  ve `LevelInstanceMember{instance_id, source_scene, local_path}` component'ini taşır; hücre ataması entity başına yapılır, bir level instance
  birden çok hücreye yayılabilir.
- `overrides` anahtarı yerel yoldur (`"3"`, iç içe `"2/3"`); `components` birleştirilir, `removed` component çıkarır; `deleted` yerel yolları siler
  (`modules/world/src/level_instance.cpp:109-116`). Kaynak `.scene` dosyasına dokunulmaz; kaynağı düzenlersen bir sonraki açılışta yerleşimler güncellenir.
- Sınırlar: iç içe derinlik 8, instance başına 10000 entity (`modules/world/include/astral/world/level_instance.hpp:51-54`); aşım hata olarak raporlanır, döngü (`A → B → A`) hata verir
  (`modules/world/src/level_instance.cpp:166-181`). `.world` içinde `.world` desteklenmez (yalnız `.scene` yerleştirilir).
- Genişletme `.world` açılırken (kaynak mod) veya cook sırasında yapılır (`modules/app/src/map_runtime.cpp:96-98`).
- **Doğrulanmadı / çıkarım:** açılan kayıtlardaki entity referansları (`astral::Parent.entity` dâhil) yeniden eşlenmiyor (`modules/world/src/level_instance.cpp:98-135`
  yalnız `astral::Transform`'u dönüştürür) ve her kayıt tek başına yaratıldığı için (§4.2) `Parent` kullanan bir kaynak sahne level instance
  içinde beklenen hiyerarşiyi vermez. Hiyerarşi için kaynak sahnede prefab instance'ı kullan. Testi: `modules/world/tests/level_instance_test.cpp`
  (yalnız "yapay `Parent` yok" iddiasını doğruluyor, `modules/world/tests/level_instance_test.cpp:66-69`).

## 6. Sahne mi, dünya mı?

| İhtiyaç | Seçim |
|---|---|
| Menü, oda, arena, tek seviye; ayrı yükleme ekranları | `.scene` + prefab'lar |
| Büyük/akıtılan harita; uzak hücrede kalıcı değişiklik; gündüz-gece katmanları | `.world` + `.ent` |
| Aynı köyü/binayı birden çok yerde kullanmak | `.scene` + level instance |

`.world` açıldığında gelen entity'ler `load_scene` ile gelenlerden **ayrı** yönetilir: harita sahipliği `MapRuntime`'dadır, düz sahneler dünyada
kalıcıdır (`close_world` yalnız harita entity'lerini siler: `modules/app/include/astral/app/map_service.hpp:24`). Seviye geçişini kendin yaz:
`load_scene` eskiyi silmez, `open_world` yenisini önceki haritanın yerine açar (`modules/app/include/astral/app/map_service.hpp:19-22`).

## Bilinen sınırlar

- **Sahne yüklemesi işlemsel değil:** kısmi hata yarım entity bırakabilir; `LoadResult::errors` her zaman kontrol edilmeli
  (`modules/world/include/astral/world/scene.hpp:150-152`).
- **Kök `.scene` hot reload yok:** yalnız prefab/materyal/shader/script/model (`modules/app/src/services/frame_loop.cpp:109-116`).
- **`.ent` kayıtları birbirine bağlanamaz** ve level instance içinde `Parent` yeniden eşlenmez (§4.2, §5; çıkarım, test yok).
- **Konumsuz `.ent` hücre (0,0)'a düşer:** konum `.ent` içindeki `Transform`'dan okunur, prefab varsayılanından değil (`modules/world/src/map.cpp:126-132`).
- **Kaynak mod `.entities/` klasörünü diskten okur:** paketli oyunda yalnız cooked harita çalışır; kaynak modda tüm harita açılışta bellekte pişirilir
  (büyük haritalarda açılış maliyeti ölçülmedi) (`modules/app/src/map_runtime.cpp:93-114`).
- **Hücre farkları kalıcı değil** (bellekte); kalıcı dünya durumu için oyun kendi kaydını yazmalı ([game-architecture.md](game-architecture.md#8-kaydetme-ve-yükleme)).
- **`OriginShifted` yalnız C++:** script'ler kaymayı olaydan öğrenemez (`modules/app/src/services/world_bridge.cpp:148`).
- **Arazi (`TerrainRef`, `.aterrain`) bu rehberin dışında:** ayrı akıtılır (`modules/app/src/services/terrain_bridge.cpp`), burada anlatılmadı.
