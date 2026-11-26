# World (`Astral::World`)

> **Olgunluk:** Stabil (ECS çekirdeği, sahne/prefab, serileştirme) · Kullanılabilir (harita durumu, level instance) · **Bağımlılıklar:** Astral::Core (`modules/world/CMakeLists.txt:16`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Motorun kendi archetype ECS'i ve onun üstündeki veri katmanı: entity/component/query/olay/kaynak, transform hiyerarşisi,
reflection tabanlı JSON serileştirme, sahne + prefab belgeleri, harita (`.world`/`.ent`/`.cell`) ve hücre streaming
kararları, floating origin matematiği. Yalnızca `Astral::Core`'a bağlıdır; pencere, GPU veya dosya sistemi bilmez
(belgeler `DocumentSource` ile dışarıdan gelir, hücre okuma işini App yapar). Sistem zamanlayıcı (paralel sistemler,
ADR-0032) bu modülde **değildir**; `modules/app/src/services/system_scheduler.cpp` içindedir, World ona yalnızca
`ScopedTick` ve thread-safe `Commands` ile hizmet eder.

## Public API

Header'lar `modules/world/include/astral/world/` altındadır.

### ECS çekirdeği

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `World` | Entity, component, kaynak, olay, komut kuyruğu sahibi. Kopyalanamaz; `parallel_each` dışında thread-safe değil | `modules/world/include/astral/world/world.hpp:120` |
| `create()`, `create(Cs&&...)`, `spawn()` (`EntityRef`), `destroy`, `alive`, `count` | Entity yaşam döngüsü | `modules/world/include/astral/world/world.hpp:131` |
| `add<T>`, `remove<T>`, `get<T>`, `has<T>` | Component erişimi; `add` varsa değiştirir; const olmayan `get` değişiklik damgalar | `modules/world/include/astral/world/world.hpp:141` |
| `each<Cs...>`, `parallel_each<Cs...>(jobs, fn, batch=1024)`, `count<Cs...>` | Archetype sütunları üzerinde yineleme | `modules/world/include/astral/world/world.hpp:165` |
| `QueryView`: `with`, `without`, `added`, `changed` | Filtreli sorgu: `world.query<A, const B>().without<C>().changed<B>().each(fn)` | `modules/world/include/astral/world/world.hpp:277` |
| `tick`, `advance_tick`, `set_change_baseline`, `clear_trackers`, `ScopedTick` | Değişiklik takip saati; worker thread için thread-local taban/yazma tick'i | `modules/world/include/astral/world/world.hpp:174` |
| `send<T>`, `update_events`, `EventReader<T>`, `Events<T>` | İki `update_events` boyunca yaşayan olay kuyruğu | `modules/world/include/astral/world/world.hpp:202` |
| `Commands` (`spawn/destroy/add/remove`), `commands()`, `apply_commands()` | Ertelenmiş yapısal değişiklik; kayıt thread-safe | `modules/world/include/astral/world/world.hpp:87` |
| `set_resource<T>`, `resource<T>` | Dünya başına tür başına tek değer | `modules/world/include/astral/world/world.hpp:215` |
| `add_raw/get_raw/has_raw/remove_raw`, `each_raw`, `keep_alive` | `ComponentId` ile tip-silinmiş erişim (script, araç, script tanımlı component) | `modules/world/include/astral/world/world.hpp:149` |
| `WorldId`, `WorldEntityRef`, `World::owns` | Dünyalar arası yanlış handle kullanımını yakalama (ADR-0034) | `modules/world/include/astral/world/world.hpp:35` |
| `ComponentId`, `component_id<T>()`, `component_name_hash<T>()`, `ComponentInfo` | Tip başına etiket adresi = kimlik; kalıcı veri için ayrı ad hash'i | `modules/world/include/astral/world/component.hpp:16` |

### Transform, origin

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Transform`, `Parent`, `WorldTransform` | Yerel TRS, ebeveyn bağı, hesaplanmış dünya matrisi | `modules/world/include/astral/world/transform.hpp:10` |
| `update_transforms(World&, JobSystem* = nullptr)` | `WorldTransform` hesaplar; kökler ve derinlik seviyeleri `JobSystem` ile paralel | `modules/world/include/astral/world/transform.hpp:31` |
| `FloatingOrigin`, `WorldPosition`, `OriginShifted`, `shift_roots` | Çift duyarlıklı dünya konumu; eşik 2000 m, 100 m'ye yuvarlanmış kayma, y kaymaz | `modules/world/include/astral/world/origin.hpp:27` |

### Serileştirme ve tip kaydı

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `TypeRegistry` (`add<T>`, `find`, `exclude<T>`, `add_copy`) | `ASTRAL_REFLECT`'li component'leri ada göre kaydeder (save/load/has/remove/transcode, alan tablosu) | `modules/world/include/astral/world/serialize.hpp:288` |
| `set_schema_version`, `prepare_schema`, `save_entry`, `load_entry` | Component başına şema sürümü ve migrasyon kancası (ADR-0034) | `modules/world/include/astral/world/serialize.hpp:341` |
| `TypeRegistry::add_dynamic`, `rollback_dynamic` | Çalışma anında şemadan component (düz veri, 256 bayt sınırı); başarısız script yüklemesini geri alma (ADR-0044) | `modules/world/include/astral/world/serialize.hpp:353` |
| `save_world`, `load_world`, `LoadResult`, `register_world_types` | Düz ECS anlık görüntüsü (`"astral.world"` v1); yalnız kayıtlı component'ler | `modules/world/include/astral/world/serialize.hpp:404` |
| `live_save_context`, `live_load_context`, `component_to_json`, `component_from_json` | Canlı entity referansları (`"#w<dünya>:<indeks>.<nesil>"`) ile araç/inspector yolu | `modules/world/include/astral/world/serialize.hpp:393` |
| `to_json` / `from_json` şablonları | bool, sayı, enum, string, Vec2/3/4, Quat, Mat4, optional, vector, std::array, iç içe reflected yapı | `modules/world/include/astral/world/serialize.hpp:186` |
| `FieldInfo`, `FieldKind`, `describe_fields<T>`, `read_number/write_number`, `field_to_json/from_json` | Bellek düzeni tabloları (script, inspector) | `modules/world/include/astral/world/fields.hpp:34` |
| `EventRegistry` (`add<T>`, `add_dynamic`, `rollback_dynamic`) | Adla olay gönder/oku; script'e özel olaylar `DynamicEvent` kuyruğunu paylaşır | `modules/world/include/astral/world/event_registry.hpp:27` |

### Sahne, prefab, harita

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Name`, `PrefabInstance`, `SceneId`, `PrefabPart`, `DocumentEntityId` | Sahne/prefab çalışma anı component'leri ve kalıcı adres (`"2/5"`) | `modules/world/include/astral/world/scene.hpp:46` |
| `PrefabLibrary` (`get`, `refresh`), `DocumentSource`, `PrefabTemplate` | Prefab'ı düzleştirir (iç içe + varyant), belge sürümüne göre önbellekler | `modules/world/include/astral/world/scene.hpp:120` |
| `load_scene`, `save_scene`, `save_entities` | `"astral.scene"` v1; kalıcı kimlik, örnekler yalnız farklarıyla | `modules/world/include/astral/world/scene.hpp:152` |
| `instantiate_prefab`, `sync_prefabs`, `find_by_name`, `find_part` | Kodla oluşturulan örnekleri açma, değişen prefab'ı yeniden açma, ada göre bulma | `modules/world/include/astral/world/scene.hpp:144` |
| `MapDesc`, `GridDesc`, `LayerDesc`, `parse_map`, `write_map` | `.world` belgesi (`"astral.map"` v1) | `modules/world/include/astral/world/map.hpp:30` |
| `MapEntity`, `read_map_source`, `make_entity_file`, `entity_file_name` | `.ent` dosyaları (`"astral.entity"` v1), kimlik 1…2^32-1 | `modules/world/include/astral/world/map.hpp:41` |
| `CellKey`, `assign_cells`, `cell_name`, `cell_distance`, `cook_map`, `read_cell` | Konuma göre hücre ataması; cook çıktısı (indeks + `"astral.cell"` belgeleri) | `modules/world/include/astral/world/map.hpp:71` |
| `CellStreamer`, `CellLoader`, `StreamingStats` | Hangi hücre yüklensin / bırakılsın kararı; iş `CellLoader` arayüzünde | `modules/world/include/astral/world/streaming.hpp:43` |
| `MapLayers`, `MapState`, `CellMember`, `CellDiff`, `MapEntityLookup` | Hücre spawn/despawn, veri katmanları, oyunun hücrede yaptığı değişikliği hatırlama | `modules/world/include/astral/world/map_state.hpp:65` |
| `LevelInstancePlacement`, `expand_level_instance`, `expand_map_level_instances`, `CrossCellRef`, `LevelInstanceMember` | `.scene`'i haritaya yerleştirme (yapay parenting yok), override, döngü/derinlik/entity sınırı (ADR-0036) | `modules/world/include/astral/world/level_instance.hpp:38` |

## Nasıl çalışır

**Depolama.** Aynı component kümesine sahip entity'ler tek `Archetype`'ta yaşar; her component tipi için bitişik
bir sütun (SoA) ve her satır için iki tick (eklenme, son değişim) tutulur (`modules/world/include/astral/world/detail/archetype.hpp:20`).
Sütun kapasitesi 64'ten başlar, dolunca iki katına çıkar ve değerler `move_construct` ile taşınır
(`modules/world/src/world.cpp:43`). Component'ler `nothrow` move-constructible olmak zorundadır
(`modules/world/include/astral/world/component.hpp:66`), çünkü bellekte yer değiştirirler.

**Kimlik.** `ComponentId`, tip başına bir `inline constexpr char` etiketinin adresidir
(`modules/world/include/astral/world/component.hpp:43`): kayıt ve global sayaç yok, farklı .cpp'lerdeki aynı adlı anonim
namespace tipleri çakışmaz (`modules/world/tests/review_test.cpp`). Kimlik çalıştırmalar arasında sabit değildir;
kalıcı veri için `component_name_hash<T>()` (FNV-1a, `modules/world/include/astral/world/component.hpp:49`) ve `TypeRegistry` adları kullanılır.
Entity = indeks + nesil; yok edilen slotun nesli artar, sıfıra sararsa 1'e çekilir (`modules/world/src/world.cpp:283`).

**Yapısal değişiklik.** Component ekle/çıkar entity'yi komşu archetype'a taşır; geçişler `add_edges`/`remove_edges` ile
önbelleklenir (`modules/world/src/world.cpp:303`, `modules/world/src/world.cpp:325`). Silme swap-remove'dur: son satır
deliğe taşınır (`modules/world/src/world.cpp:81`). `each`/`parallel_each` sırasında `create/destroy/add/remove`
`ASTRAL_ASSERT` ile durdurulur (`modules/world/src/world.cpp:224`); `ASTRAL_ASSERT` Release'te de açıktır
(`modules/core/include/astral/core/assert.hpp:7`). Yineleme içinde yapısal değişiklik gerekirse `commands()` kullanılır;
`Commands::apply` kuyruğu boşalana kadar döner (`modules/world/src/world.cpp:110`).

**Sorgu.** Her sorgu için eşleşen archetype listesi `queries_` içinde önbelleklenir ve yeni archetype'lar eklendikçe
yalnız yeni olanlar test edilir (`modules/world/src/world.cpp:345`). `with/without/added/changed` filtreleri archetype
düzeyinde (`with`, `without`) veya satır düzeyinde (`added`, `changed`) uygulanır (`modules/world/include/astral/world/world.hpp:461`).

**Değişiklik takibi.** Kural: const olmayan erişim = değişiklik. `each<T>` mutable satırlara `tick()` damgalar,
`get<T>` (const olmayan) damgalar, `get<const T>` damgalamaz (`modules/world/include/astral/world/world.hpp:413`,
`modules/world/include/astral/world/world.hpp:498`). `added/changed` satırın tick'ini `change_baseline()` ile
karşılaştırır. Kütüphane kullanımında taban `clear_trackers()` ile ilerler; App içinde her sistem kendi tabanını alır
(`modules/app/src/services/system_scheduler.cpp:29`). Paralel dalgada her sisteme kayıt sırasına göre `base_tick + i`
yazma tick'i ve kendi `last_run_` tabanı verilir; bunlar `ScopedTick` ile thread-local tutulur
(`modules/app/src/services/system_scheduler.cpp:38`, `modules/world/src/world.cpp:138`). `QueryView::parallel_each`
çağıranın tick durumunu worker'lara taşır (`modules/world/include/astral/world/world.hpp:514`).

**Zamanlayıcı (App'te).** `System::configure(SystemAccess&)` okuma/yazma `ComponentId` kümelerini bildirir; bildirmeyen
sistem `exclusive` sayılır. Çakışmayan, kayıt sırasında komşu sistemler dalga halinde `JobSystem`'de paralel koşar
(`modules/app/src/services/system_scheduler.cpp:108`, çakışma kuralı `:75`). Ayrıntı: [../decisions/0032-scalable-runtime-and-parallel-scheduler.md](../decisions/0032-scalable-runtime-and-parallel-scheduler.md).

**Olaylar.** `Events<T>` iki kuyruktur (`previous_`, `current_`); `update()` eskiyi atar
(`modules/world/include/astral/world/world.hpp:51`). Okuyucu imleç tutar; okuma sırasında aynı türden gönderilen olaylar
o okumaya girmez, bir sonrakine kalır (`modules/world/include/astral/world/world.hpp:58`).
`send` `event_mutex_` ile korunur (`modules/world/include/astral/world/world.hpp:449`).

**Transform hiyerarşisi.** `update_transforms` önce `Transform`'u olup `WorldTransform`'u olmayanlara bileşeni ekler,
kökleri hesaplar (eşik 1024 satır üstünde `parallel_each`), sonra çocukları ebeveyn zincirinden derinlik seviyelerine
ayırıp seviye başına `parallel_for` yapar. Döngü zincirin tepesinde kırılıp kök sayılır; ölü ebeveyn kök sayılır
(`modules/world/src/transform.cpp:28`, `modules/world/src/transform.cpp:63`, `modules/world/src/transform.cpp:109`).

**Serileştirme.** `ASTRAL_REFLECT` alan listesinden `to_json`/`from_json` üretilir. Yükleme hoşgörülüdür: bilinmeyen
alan yok sayılır, eksik alan varsayılanda kalır, hatalar `LoadResult::errors`'a yazılır
(`modules/world/include/astral/world/serialize.hpp:170`). Sayılar hedef tipe sığmalı ve tamsayı alanlarda tam olmalıdır;
64-bit tamsayılar kayıpsızdır (ADR-0041, `modules/world/include/astral/world/serialize.hpp:151`). `save_world` yalnız
kayıtlı component'i olan entity'leri yazar ve kimliği yazma sırasına göre verir
(`modules/world/src/serialize.cpp:90`). Canlı referans biçimi `"#w<dünya kimliği>:<indeks>.<nesil>"`'dir ve başka
dünyaya ait referans reddedilir (`modules/world/src/serialize.cpp:172`, test: `modules/world/tests/identity_schema_test.cpp`).

**Script tanımlı component'ler.** `add_dynamic` alanları 4 bayt sözcüklere dizer (Bool dahil 4 bayt yer tutar), en çok 64
sözcük = 256 bayt; boyuta göre 64 `Blob<N>` örneğinden biri seçilir ve kimlik `DynamicType::tag` adresidir
(`modules/world/src/dynamic.cpp:13`, `modules/world/src/dynamic.cpp:48`, `modules/world/src/dynamic.cpp:90`). Tip verisi
`World::keep_alive` ile dünya ömrünce tutulur (`modules/world/src/dynamic.cpp:126`).
`rollback_dynamic(entry_count)` girdileri `retired_` listesine taşır; `ComponentId` bir daha kullanılmaz
(`modules/world/src/dynamic.cpp:167`). Script host başarısız yüklemede bunu çağırır (`modules/script/src/host.cpp:163`,
karar: [../decisions/0044-staged-script-module-load.md](../decisions/0044-staged-script-module-load.md)).
Dinamik olaylar tek `Events<DynamicEvent>` kuyruğunu paylaşır; okuyucu ada göre süzer
(`modules/world/src/dynamic.cpp:139`).

**Sahne ve prefab.** Prefab = tek köklü sahne. `PrefabLibrary::build` iç içe prefab'ları ve varyantları düzleştirip
`PrefabTemplate` üretir, kaynak belge sürümlerini saklar; `refresh()` sürümü değişenleri düşürür
(`modules/world/src/prefab.cpp:369`). Kendini içeren prefab hata verir (`modules/world/src/prefab.cpp:354`).
`sync_prefabs` kodla yaratılmış örnekleri açar, değişen şablonları fark alıp yeniden açar
(`modules/world/src/scene.cpp:35`). `save_scene` örnekleri `prefab` + yalnız farklar (`components`, `removed`, `parts`,
`deleted`) olarak yazar; sürüm 1 dışındaki belge reddedilir (`modules/world/src/scene_internal.hpp:19`,
`modules/world/src/prefab.cpp:119`).

**Harita ve streaming.** `.world` + `.ent` dosyaları `read_map_source` ile doğrulanır; hücre = `floor(konum / cell_size)`
(yalnız yer düzlemi x,z). `cook_map` bir indeks ve hücre başına bir `"astral.cell"` belgesi üretir
(`modules/world/src/map.cpp:203`). `CellStreamer::update` her çağrıda her hücrenin en yakın kaynağa uzaklığını hesaplar;
`load_range` içinde yüklemeyi başlatır, `load_range × (1 + histerezis)` dışında bırakır, hazır hücreleri en yakından
başlayarak çağrı başına `max_attach` kadar ekler; başarısız `attach` 1, 2, 4 … 64 update bekleyip yeniden dener
(`modules/world/src/streaming.cpp:16`, `modules/world/src/streaming.cpp:68`). Gerçek G/Ç ve JobSystem kullanımı
App'tedir (`CellLoader` uygulaması). `MapState::despawn_cell` hücrede oyunun yok ettiği ve değiştirdiği entity'leri
`CellDiff` olarak dünya koordinatında hatırlar, hücre tekrar yüklenince uygular
(`modules/world/src/map_state.cpp:224`, `modules/world/src/map_state.cpp:176`).

**Level instance.** `.scene`, haritaya yerleştirme dönüşümüyle dünya uzayında açılır (parent zinciri kurulmaz); iç
entity kimlikleri `(instance_id, local_path)` üzerinden 31 bitlik FNV hash'inden türetilir, çakışmada artırılarak
çözülür (`modules/world/src/level_instance.cpp:79`, `modules/world/src/level_instance.cpp:196`). Varsayılan sınırlar:
derinlik 8, 10000 entity (`modules/world/include/astral/world/level_instance.hpp:51`).

**Floating origin.** `FloatingOrigin::update` yalnız yatay (x,z) uzaklık eşiği aşılınca kayma döndürür
(`modules/world/src/origin.cpp:12`). `shift_roots` yalnız `Parent`'ı olmayan `Transform`'ları kaydırır
(`modules/world/src/origin.cpp:30`). Fizik gövdelerinin ve render geçmişinin kaydırılması App katmanındadır;
`WorldBridge::begin_origin_shift` `shift_roots`'u çağırır ve `OriginShifted` olayını gönderir
(`modules/app/src/services/world_bridge.cpp:138`, `modules/app/src/services/world_bridge.cpp:148`). Fizik/render tarafı
bu belgede doğrulanmadı.

## Güçlü yanlar

- Küçük, bağımsız çekirdek: yalnız Core'a bağlı; sunucuda ve testlerde pencere/GPU olmadan çalışır
  (`modules/world/CMakeLists.txt:16`).
- Yanlış kullanım sessiz bellek bozulması yerine durdurulur: yineleme sırasında yapısal değişiklik, ölü entity'ye `add`
  (`modules/world/include/astral/world/world.hpp:388`) `ASTRAL_ASSERT` ile Release'te de yakalanır.
- Eski handle yeni entity'ye ulaşmaz (nesil) ve dünyalar arası handle karışması tespit edilebilir
  (`WorldEntityRef`, `modules/world/include/astral/world/world.hpp:38`; test `identity_schema_test.cpp`).
- Değişiklik takibi sistem başına tabanla çalışır ve paralel sistemlerde deterministik damgalama vardır
  (`modules/world/tests/msr_regression_test.cpp` "C01", `modules/world/tests/query_test.cpp`).
- Serileştirme sağlam kenar durumlara sahip: tamsayı aralık/kesir denetimi, 64-bit kayıpsızlık, şema sürümü + migrasyon,
  CRLF toleransı (`modules/world/tests/msr_regression_test.cpp`, `modules/world/tests/scene_world_crlf_test.cpp`).
- Sahne diff'i temiz: kalıcı kayıt kimlikleri, prefab örnekleri yalnız farkla yazılır; prefab hot-reload'da override'lar
  korunur (`modules/world/tests/scene_test.cpp`).
- Script/araç yolu için tip-silinmiş katman (ham erişim, alan tabloları, dinamik component, adlı olay) aynı sahne/kayıt
  yoluna bağlanır (`modules/world/tests/dynamic_test.cpp`).
- Başarısız hücre yüklemesi sonsuz döngüye girmez; üstel geri çekilme vardır
  (`modules/world/tests/streaming_test.cpp` "A09").

## Zayıf yanlar ve bilinen sınırlar

- **Dirty-flag yok:** `update_transforms` her çağrıda tüm hiyerarşiyi baştan hesaplar; değişmeyen alt ağaçlar atlanmaz,
  ve her çağrıda `ChildNode`/`LevelItem` vektörleri yeniden kurulur (`modules/world/src/transform.cpp:109`).
- **`changed<T>` satır başınadır**, alan başına değil; bir alanı yazmak tüm component'i değişmiş sayar
  (`modules/world/include/astral/world/world.hpp:424`). Tick 32 bit, sarma koruması yok
  (`modules/world/include/astral/world/world.hpp:32`).
- **Her `each` bir `find_query` kopyası alır:** `find_query` eşleşen archetype vektörünü değer olarak döndürür ve
  `query_mutex_` alır (`modules/world/src/world.cpp:345`); çok sayıda küçük sorgu/kare için ek maliyet. Ölçülmedi.
- **`parallel_each` iç thread-safety'si yalnız kendi satırı içindir:** sistem erişim bildirimi (`reads/writes`)
  doğrulanmaz; yanlış bildirilen sistem veri yarışı yaratır. Kodda bildirimi gerçek erişimle karşılaştıran bir denetim
  görmedim (doğrulanmadı) (`modules/app/src/services/system_scheduler.cpp:61`).
- **Olay okuma kilitsiz, gönderme kilitli:** `EventReader::read` `world.resource<Events<T>>()` çağırır
  (`modules/world/include/astral/world/world.hpp:334`) ama ilk `send` kaynağı kilit altında yaratır
  (`modules/world/include/astral/world/world.hpp:449`). Aynı dalgada paralel bir sistem ilk kez `send` ederken başkası
  okursa veri yarışı mümkün görünüyor; test yok, doğrulanmadı.
- **Olay kaçırma:** iki `update_events` boyunca okumayan okuyucu olay kaçırır (bilerek, sınırsız kuyruk yok)
  (`modules/world/include/astral/world/world.hpp:329`).
- **`commands().spawn()` entity döndürmez**, ertelenmiş referans yoktur (`modules/world/include/astral/world/world.hpp:89`).
  `add` ve `get<T>` ile dönen referans/işaretçi sonraki yapısal değişikliğe kadar geçerlidir (`modules/world/include/astral/world/world.hpp:140`).
- **Tip kimliği DLL sınırını aşmaz:** `ComponentId` adres tabanlıdır; motor ve oyun aynı ikili içinde statik
  linklenmeli (`modules/world/include/astral/world/component.hpp:12`).
- **Linear taramalar:** `TypeRegistry::find` ve `EventRegistry::find` doğrusal arama
  (`modules/world/src/serialize.cpp:12`, `modules/world/src/dynamic.cpp:160`); `find_by_name`/`find_part` tüm `Name`
  satırlarını tarar (`modules/world/src/scene.cpp:250`); `sync_prefabs` her çağrıda tüm `PrefabPart`/`PrefabInstance`
  satırlarını tarar (`modules/world/src/scene.cpp:39`, `modules/world/src/scene.cpp:58`); `MapState::lookup` her çağrıda
  tüm `CellMember`'ları gezer (`modules/world/src/map_state.cpp:125`); `CellStreamer::update` hücre × kaynak
  karmaşıklığındadır (`modules/world/src/streaming.cpp:21`).
- **Prefab şablonları JSON üzerinden:** ikili cook'lanmış şablon yok; her örnek JSON'dan açılır, binlerce örnekte
  yavaş olabilir. Ölçülmedi (`modules/world/include/astral/world/scene.hpp:108`).
- **Hot-reload'da parça handle'ları geçersizleşir:** parçalar yeniden oluşturulur; parçaya tutulan yerel `Entity`
  değişkenleri ölür, dışarıdan parçaya giden component referansları korunur (yol eşlemesiyle) (`modules/world/src/scene.cpp:35`).
- **Kodla parçalara eklenen yeni çocuklar** sahnede kendi kaydı olarak kalır; hot-reload'da ebeveyn parça yeniden
  oluşunca bu çocukların `Parent`'ı boşa düşer (legacy not; kod yolu burada tekrar doğrulanmadı).
- **Harita durumu yalnız bellekte:** `CellDiff` (`remembered_`) World modülünde diske yazılmaz; kayıt/yükleme için bir
  serileştirme API'si yok (`modules/world/include/astral/world/map_state.hpp:96`). Kalıcılık App/oyun tarafına bırakılmış;
  orada ne yapıldığı bu belgede doğrulanmadı.
- **Katman gizleme değişiklikleri korur** (`despawn_cell` gibi diff yazar, oyunun sildiği entity tekrar doğmaz:
  `modules/world/src/map_state.cpp:253`, test `modules/world/tests/map_state_test.cpp:120`). Ama oyunun hücrede **yeni yarattığı** entity'ler `CellDiff`'e girmez (yalnız
  `CellMember`'lı, yani haritadan gelen entity'ler izlenir) (`modules/world/src/map_state.cpp:224`).
- **Hücre ataması nesnenin konumuna göre**, sınır kutusuna göre değil: büyük nesne komşu hücreden görünse de kendi hücresi
  yüklenmeden gelmez (`modules/world/include/astral/world/map.hpp:88`). Hücreler yer düzleminde (x,z) kareler; yükseklik
  boyutu yok (`modules/world/include/astral/world/map.hpp:19`).
- **Level instance kimlik çakışması:** iç entity kimlikleri 31 bitlik hash'ten gelir; çakışma artırarak çözülür, bu
  çözümün kararlılığı (çakışan iki entity'nin hangi sırayla kimlik alacağı) kaynak sırasına bağlıdır; test kapsamı doğrulanmadı
  (`modules/world/src/level_instance.cpp:79`).
- **Dinamik component sınırları:** en çok 256 bayt, düz veri, alanlar sonradan değişmez; dize ve iç içe tip yok
  (`modules/world/src/dynamic.cpp:13`, `modules/world/src/dynamic.cpp:48`). `TypeRegistry::Entry*` işaretçisi sonraki
  `add`'e kadar geçerlidir (`modules/world/include/astral/world/serialize.hpp:353`).
- **`rollback_dynamic` önkoşulu çağırana bırakılmış:** geri alınan tiplerin hiçbir entity'de bulunmadığını çağıran
  garanti etmelidir; World doğrulamaz (`modules/world/include/astral/world/serialize.hpp:354`).
- **Luau hassasiyeti:** Luau sayıları `double` olduğundan script'e verilen 64-bit değerler 2^53 üstünde kayıplıdır
  (ADR-0041 kapsamı C++ tarafını kapsar) (`modules/world/include/astral/world/serialize.hpp:151`).
- `EventRegistry::rollback_dynamic` girdi adlarını (`owned_names_`) temizlemez, yalnız girdileri siler
  (`modules/world/src/dynamic.cpp:174`); küçük bir bellek birikimi, davranışsal etkisi görülmedi.

## Testler

`modules/world/tests/` altında 17 test dosyası (`modules/world/CMakeLists.txt:18`), doctest:

| Dosya | Kapsam |
|---|---|
| `world_test.cpp` | Kimlik, nesil, add/remove, archetype geçişleri, swap-remove, yapıcı/yıkıcı sayımı, `each`, komutlar, `parallel_each`, kaynaklar |
| `query_test.cpp` | `with/without/added/changed`, tick'lerin taşınması, sistem başına taban, filtreli paralel yineleme, olay ömrü |
| `review_test.cpp`, `msr_regression_test.cpp` | Anonim namespace çakışması, büyüme sırasında kopya ekleme, paralel komut kaydı, derin hiyerarşi, döngü; MSR C01–C06 (ScopedTick, olay okuma içinden gönderme, `$schema_version`, 64-bit) |
| `transform_test.cpp` | Kök/çocuk, rotasyon/ölçek, ölü ebeveyn, döngü, paralel = seri |
| `serialize_test.cpp`, `identity_schema_test.cpp` | Round-trip, hoşgörülü yükleme, format reddi, `exclude`; `WorldEntityRef`, `DocumentEntityId`, şema migrasyonu, `MapState` durumları, komut sırası |
| `dynamic_test.cpp` | Ham erişim, `each_raw`, alan tabloları, dinamik component ve olaylar |
| `scene_test.cpp`, `scene_world_crlf_test.cpp` | Kalıcı kimlik, override, iç içe/varyant, prefab hataları, `sync_prefabs`/hot-reload, CRLF |
| `map_test.cpp`, `streaming_test.cpp`, `map_state_test.cpp`, `level_instance_test.cpp`, `origin_test.cpp` | `.world`/`.ent`, hücre ataması, cook; streaming (histerezis, iptal, bütçe, geri çekilme); diff, katman; level instance (döngü, sınır, çok hücreli); origin |

Kapsanmayanlar: `TypeRegistry::rollback_dynamic` ve `EventRegistry::rollback_dynamic`'in World modülünde doğrudan testi
yok (kullanım yeri script host: `modules/script/src/host.cpp:163`; script testlerinde dolaylı kapsanabilir, bu belgede
doğrulanmadı). Paralel sistemde ilk `send` ile eşzamanlı okuma yarışı için test yok. TSan/ASan Windows'ta çalıştırılmadı.
Repo kökündeki `bench/main.cpp` ECS, transform ve streaming benchmark'ları içerir; ADR-0032 CTest'te bir duman testi
(`astral_benchmarks_smoke`) olduğunu söyler (CMake kaydı bu incelemede doğrulanmadı).

## Performans

Ölçülmedi (bu belge için). `bench/main.cpp` içinde ECS ölçeklenmesi (`bench_ecs`, satır 119), seri vs paralel
`update_transforms` (`bench_transforms`, satır 174) ve hücre streaming (`bench_streaming`, satır 323) ölçümleri var;
bu kaynak incelemesinde çalıştırılmadı ve belgeye kayıtlı sonuç yok.

## Kararlar

- [0005 — Kendi ECS](../decisions/0005-own-ecs.md)
- [0013 — ECS değişiklik takibi ve reflection](../decisions/0013-ecs-change-detection-reflection.md)
- [0015 — Sahneler ve prefab'lar](../decisions/0015-scenes-and-prefabs.md)
- [0021 — Dünya ve seviye sistemi](../decisions/0021-world-and-level-system.md)
- [0032 — Ölçeklenebilir çalışma zamanı ve paralel zamanlayıcı](../decisions/0032-scalable-runtime-and-parallel-scheduler.md)
- [0034 — Gameplay API ve fizik adım sözleşmesi](../decisions/0034-gameplay-api-and-physics-step-contract.md)
- [0036 — Dünya yazarlığı ve level instance'lar](../decisions/0036-world-authoring-and-level-instances.md)
- [0041 — Kayıpsız JSON tamsayıları](../decisions/0041-lossless-json-integers.md)
- [0044 — Aşamalı script modülü yükleme](../decisions/0044-staged-script-module-load.md)
