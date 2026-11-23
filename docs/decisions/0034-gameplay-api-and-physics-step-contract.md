# ADR-0034: Gameplay API Keşif Yüzeyi, Kimlik Sözleşmesi ve Fizik Adım Kuyruğu (F1)

**Tarih:** 2026-11-23 · **Durum:** Kabul edildi · **Faz:** F1

## Bağlam

M11–M12 ile `FeatureSet`, `TypeRegistry`, `EventRegistry`, Luau `ScriptHost`, `PhysicsBridge` ve açık dünya akışı çalışır hale geldi. Ancak yol haritası F1'de üç mimari boşluk öne çıktı:

1. **Keşif ve şema sapması:** `TypeRegistry`/`EventRegistry` veri alanlarını tutarken `modules/script/src/api.cpp` çağrılabilir `astral.*` ve `Entity:*` metotlarını elle çoğaltılmış string'lerden üretiyordu; component şema sürümü ve migration kancası yoktu.
2. **Kimlik katmanlarının ayrışmaması:** Runtime `Entity` (index + generation) `World` aidiyeti taşımıyordu; sahne/prefab belge kimliği ve harita entity referansının (`Alive` / `Unloaded` / `Destroyed` / reloaded) ayrı sözleşmesi tanımlı değildi.
3. **Fizik komut zamanlaması:** `AppScriptPhysics::impulse/force/set_velocity` doğrudan `PhysicsWorld`'e yazıyordu. Aynı tick'te gecikmiş spawn veya `RigidBody` ekleme sonrası verilen komutlar `sync_bodies()` öncesinde sessizce düşebiliyor, paralel C++ sistemleri de deterministik sırada fizik komutu kaydedemiyordu.

## Karar

### 1. Tek Keşif Yüzeyi ve Şema Sürümü
- `TypeRegistry` ve `EventRegistry` veri alanları, tipleri, enum'ları ve şema sürümünün (`schema_version`, varsayılan `1`) tek kaynağıdır.
- `modules/script` içinde `ScriptApiCatalog` (`script_api_catalog()`) tüm çağrılabilir `astral.*` ve `Entity:*` bağlamalarının tek metadata kaynağı olur; `generate_script_api()` (`astral.d.luau` ve `API.md`) ve sapma testleri bu katalogdan üretilir.
- `TypeRegistry::set_schema_version(name, version, migrate)` isteğe bağlı migration kancası tanımlar. Sürüm `1` belgelerde `"$schema_version"` alanı yazılmaz (mevcut tüm `.scene`/`.world`/`.ent` dosyaları bayt-bayt uyumlu kalır); `schema_version > 1` component'ler `"$schema_version"` yazar. Daha yeni şema sürümü hata teşhisiyle atlanır, daha eski şema kayıtlı `migrate` ile yükseltilir.

### 2. Üç Katmanlı Kimlik Sözleşmesi
- **Runtime kimlik:** `World::id()` (`WorldId`, her `World` örneği için benzersiz), `WorldEntityRef { WorldId, Entity }` ve `World::owns(WorldEntityRef)`. Canlı JSON referansları (`live_save_context` / `live_load_context`) `#w<world_id>:<index>.<gen>` biçimini üretir ve eski `#<index>.<gen>` biçimini de kabul eder; farklı bir `World`'e ait referans reddedilir.
- **Sahne/Prefab belge kimliği:** `DocumentEntityId { record_id, part_path }` (`"2"`, `"2/5"`), E0 editör komutlarının kalıcı belge hedefi olarak kullanılır.
- **Harita referansı:** `MapEntityLookup { MapEntityStatus, Entity, CellKey, load_count }` (`Unknown`, `Unloaded`, `Alive`, `Destroyed`) ile `MapState::lookup()` ve `MapService::resolve_map_entity()` yüklenmemiş, yok edilmiş ve yeniden yüklenmiş durumları açıkça ayırır.

### 3. Dar `PhysicsService` ve Sabit Adım Sözleşmesi
- `PhysicsService` komut (`add_impulse`, `add_force`, `set_linear_velocity`, `set_angular_velocity`), sorgu (`raycast`, `spherecast`, `overlap_sphere`, `overlap_box`, `linear_velocity`, `query_state`) ve temas yüzlerini (`contacts`) dar bir sözleşmede birleştirir.
- Tüm komutlar mutex korumalı bir kuyruğa `(order_key, sequence, cmd)` olarak kaydedilir; `FixedUpdate` sonrası `world_.apply_commands()` ve `sync_bodies()` tamamlandığında sabit sırada (`order_key`, ardından `sequence`) `PhysicsWorld`'e uygulanır.
- Sıfır alt adımlı karelerde (`delta < fixed_delta`) biriken komutlar kaybolmaz, sonraki sabit adıma taşınır; ölü veya `RigidBody`'siz entity komutları güvenle atlanır.
- Sorgular her zaman son tamamlanmış `physics_->step()` anlık görüntüsünden okur (`PhysicsQueryState::step_index`). `FixedUpdate` içindeki `contacts()` önceki tamamlanmış adıma, `PostUpdate` içindeki `contacts()` ise o karenin tüm adımlarına aittir.
- Doğrudan `PhysicsWorld` kullanan alt seviye C++ tüketicilerinin adım kontrolü korunur.

### 4. Eski `Context` Düz Sarmalayıcıları
- Depodaki tüm dahili örnek, şablon ve testler M13.5'te gruplu servislere (`scenes()`, `maps()`, `materials()`, `audio_service()`, `script_service()`, `physics_service()`) geçmişti; yalnızca uyumluluk testi eski `[[deprecated]]` düz metotları çağırıyordu. Bu fazda eski düz forward'lar kaldırılarak `Context` yüzü sadeleştirilir.

## Sonuçlar
- Inspector, `API.md`, `astral.d.luau` ve Luau runtime arasındaki sapmalar otomatik birim testiyle yakalanır.
- Paralel C++ sistemleri ve Luau script'leri `PhysicsService` komut kuyruğuyla deterministik (seri = paralel) fizik çıktısı üretir.
- E0 editör çekirdeği, F1'in `DocumentEntityId`, `TypeRegistry` şema sürümü ve `ApiDiscoverySummary` sözleşmesini doğrudan tüketebilir.
