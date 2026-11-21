# Astral::World

Katman 2. Kendi archetype ECS'imiz ([ADR-0005](../decisions/0005-own-ecs.md)). Yalnızca Core'a bağlı:
sunucuda ve testlerde pencere/GPU olmadan çalışır. Kullanım rehberi: [writing-a-game.md](../guides/writing-a-game.md).

## API özeti

| | |
|---|---|
| `create()`, `create(A{}, B{}...)`, `spawn()` | entity oluştur (`spawn` → akıcı `EntityRef`) |
| `destroy(e)`, `alive(e)`, `count()` | |
| `add(e, T{})`, `remove<T>(e)`, `get<T>(e)`, `has<T>(e)` | `add` varsa değiştirir |
| `each<A, const B>(fn)` | fn(A&, const B&) veya fn(Entity, A&, const B&) |
| `parallel_each<...>(jobs, fn, batch)` | JobSystem üzerinde |
| `count<A, B>()` | A ve B'ye sahip entity sayısı |
| `commands()` + `apply_commands()` | ertelenmiş yapısal değişiklik |
| `set_resource(T)`, `resource<T>()` | dünya başına tekil değerler |
| `Transform`, `Parent`, `WorldTransform`, `update_transforms()` | hiyerarşi |
| `query<A, const B>().with<C>().without<D>().added<A>().changed<B>()` | filtreli sorgu: `.each`, `.parallel_each`, `.count` |
| `tick()`, `advance_tick()`, `set_change_baseline(t)`, `clear_trackers()` | değişiklik takibi saati |
| `send(T)`, `EventReader<T>::read(world, fn)`, `update_events()` | olaylar |
| `TypeRegistry`, `save_world`, `load_world` (`astral/world/serialize.hpp`) | reflection ile JSON kaydet/yükle |

## Değişiklik takibi

Her component satırı iki tick taşır: **eklendiği** ve **son değiştiği**. Kural basit: **const olmayan erişim =
değişiklik** (`each<T>`, `get<T>`, var olan değeri `add` ile değiştirmek). Sadece okuyorsan `const T` yaz.
`added<T>()`/`changed<T>()` filtreleri bu tick'leri dünyanın *değişiklik referansıyla* karşılaştırır:

- Kütüphane olarak: `clear_trackers()` → "son clear_trackers'tan beri".
- App içinde: her `System` kendi referansını alır → "bu sistem son çalıştığından beri". Sistemin kendi
  yazdıkları bir sonraki çalışmasında görünmez.

## Olaylar

```cpp
world.send(Damage{target, 30});                     // her yerden
EventReader<Damage> reader;                         // okuyan sistemin üyesi
reader.read(world, [&](const Damage& d) { ... });   // her olayı bir kez görür
```

Olaylar iki `update_events()` boyunca yaşar (App kare başında çağırır): bir karede gönderilen olayı ondan
önce *veya* sonra çalışan her sistem görür. İki kareden uzun okumayan okuyucu olay kaçırır.

## Serileştirme

```cpp
TypeRegistry types;
register_world_types(types);   // astral::Name, astral::Transform, astral::Parent
types.add<Health>();           // ASTRAL_REFLECT'li her component
Json doc = save_world(world, types);
LoadResult r = load_world(other, types, doc);   // ekler; r.entities belge sırasında, r.errors sorunlar
```

- Yalnız **kayıtlı** component'ler kaydedilir; hiç kayıtlı component'i olmayan entity atlanır.
- Component içindeki `Entity` alanları belge kimliğine çevrilir, yüklemede yeni entity'lere eşlenir
  (belgede olmayan entity'ye referans → null + hata kaydı).
- Desteklenen alan tipleri: bool, sayılar, enum, `std::string`, `Vec2/3/4`, `Quat`, `Mat4`, `Entity`,
  `std::vector`, `std::array`, `std::optional`, iç içe ASTRAL_REFLECT'li yapılar. Çalışma anı handle'ları
  (`MeshHandle` vb.) null yazılır. Dosyaya yazılabilen karşılıkları: `ModelRef`, `PrimitiveMesh`.
- `ASTRAL_REFLECT_ENUM`'lu enum'lar isimle yazılır (`"Torus"`); okumada isim de sayı da kabul edilir.
  Float'lar en kısa geri dönüşen ondalıkla yazılır (`0.6`, `0.6000000238…` değil).
- `save_world` **düz anlık görüntü** üretir (kayıt oyunu, araçlar): kimlikler kayıt sırasına göre verilir,
  prefab örnekleri düz entity olarak yazılır. Sahne/prefab için aşağıdaki `save_scene`.
- Yükleme hoşgörülü: bilinmeyen alanlar yok sayılır, eksik alanlar varsayılanında kalır, bilinmeyen
  component ve yanlış tipler `errors`'a yazılıp atlanır. Biçim/sürüm uyuşmazsa hiçbir şey oluşturulmaz.

## Sahneler ve prefab'lar

`astral/world/scene.hpp` · karar: [ADR-0015](../decisions/0015-scenes-and-prefabs.md)

```cpp
PrefabLibrary prefabs(types, source);          // source: path -> {const Json*, version}
LoadResult r = load_scene(world, prefabs, doc); // ekler; entity'ler SceneId alır
Json out = save_scene(world, prefabs);          // aynı kimlikler; örnekler yalnız farklarıyla
world.create(Transform{...}, PrefabInstance{"props/lamp.prefab"});
sync_prefabs(world, prefabs);                   // kodla oluşturulan örnekleri açar, değişenleri yeniler
Entity bulb = find_part(world, lamp, "bulb");   // Name ile
```

```json
{"format": "astral.scene", "version": 1, "entities": [
  {"id": 1, "components": {"astral::Name": {"value": "floor"}, "astral::Transform": {"scale": [10, 1, 10]}}},
  {"id": 2, "prefab": "props/lamp.prefab",
   "components": {"astral::Transform": {"position": [3, 0, 0]}},
   "parts": {"2": {"components": {"astral::PointLight": {"intensity": 25}}}},
   "removed": ["..."], "deleted": ["3"]},
  {"id": 3, "components": {"astral::Parent": {"entity": "2/2"}}}]}
```

- **Kalıcı kimlikler:** kayıt `id`'si çalışma anında `SceneId`; yeni entity'ler en büyük kimliğin devamını
  alır, silinen kayıt diğer satırlara dokunmaz → temiz diff. Aynı sahne iki kez yüklenirse çakışan kimlikler
  kayıtta yenilenir.
- **Prefab = tek köklü sahne.** Örnek kaydı `prefab` + yalnız farklar: `components` (kök için alan bazlı
  yama), `removed` (kökten kaldırılan component'ler), `parts` (prefab içi yol → yama), `deleted` (yok edilen
  parçalar; çocukları onlarla gider).
- **Override = kayıt anında fark.** Oyun/editör bir parçayı değiştirdiyse kayıt bunu kendiliğinden yazar;
  değişmeyen örnek tek satırdır (`{"id": 2, "prefab": "..."}`).
- **Referanslar:** kayıt kimliği (`4`) veya örnek içi yol (`"2/5"`, iç içe `"2/4/3"`). Prefab içindeki bir
  parçaya parent'lanmak mümkün.
- **İç içe ve varyant:** prefab örnek içerebilir; prefab'ın kökü başka bir prefab'ın örneği olabilir
  (varyant — parçalar temelin yollarını korur). Kendini içeren prefab hata verir.
- **Çalışma anı:** kökte `PrefabInstance`, parçalarda `PrefabPart{instance, path}`. Parçalar örnekle yok
  edilir, ayrı kayıt olmaz. Kökün kendi component'leri prefab'ınkine üstün gelir.
- **Hot-reload:** `PrefabLibrary::refresh()` belgesi değişen şablonları düşürür; `sync_prefabs` o örnekleri
  eski şablona göre fark alıp yeni şablonla yeniden açar — override'lar ve dışarıya referanslar korunur.
- World dosya sistemini bilmez: belgeler `DocumentSource` ile gelir (App'te `AssetManager::load_document`).

## İçeride nasıl çalışıyor

- **Component kimliği** tip başına benzersiz bir etiketin adresi: kayıt yok, global sayaç yok ve farklı
  .cpp dosyalarındaki aynı isimli (anonim namespace) tipler asla çakışmaz. Kaydedilecek/ağa gidecek
  veriler için çalıştırmalar arası sabit `component_name_hash<T>()` var.
- **Archetype**: aynı component kümesine sahip entity'ler; her component tipi için sıkı paketlenmiş
  bir sütun (SoA). `each` sütunlar üzerinde doğrusal yürür → cache dostu, kolay paralel.
- Component ekle/çıkar entity'yi komşu archetype'a taşır; geçişler önbellekli (archetype grafiği).
- Silme **swap-remove**: son satır deliğe taşınır, O(1).
- Sorgular önbellekli; yeni archetype'lar oluştukça artımlı güncellenir.
- Entity = indeks + nesil: silinen entity'nin eski handle'ı asla yeni entity'ye ulaşmaz.
- `each` sırasında yapısal değişiklik `ASTRAL_ASSERT` ile durdurulur (sessiz bellek bozulması yerine).
- `commands()` thread-safe: `parallel_each` içinden de kayıt yapılabilir.
- `add`/`create` değeri önce kopyalar: dünyanın içindeki bir component'i kopyalayarak eklemek güvenli.

## Bilinen sınırlar

- `update_transforms` tek thread'de, her entity'yi bir kez hesaplar (derinlik sınırı yok, döngüler kök
  sayılır). İleride derinliğe göre sıralı, paralel yayılım.
- Component kimlikleri paylaşılan kütüphaneler (DLL) arasında farklı olabilir: motor ve oyun aynı
  ikili içinde statik linklenmeli (varsayılan).
- Tick'ler 32 bit: ~4 milyar sistem çalışmasından sonra sarar (pratikte sorun değil, ileride wrap koruması).
- `changed<T>` satır başına, alan başına değil: bir alanı yazmak tüm component'i değişmiş sayar.
- Olay okuyucusu iki kareden uzun okumazsa olay kaçırır (bilerek: sınırsız kuyruk yok).
- `commands().spawn()` oluşturulan entity'yi döndürmüyor (ertelenmiş referans yok).
- `get<T>` dönen pointer bir sonraki yapısal değişikliğe kadar geçerli.
- Sahneler: yalnız JSON (ikili sahne M6 cook'ta). Prefab şablonları her örnekte JSON üzerinden
  deserialize edilir — binlerce örnekte yavaş (cook edilmiş ikili şablon ileride).
- Hot-reload'da parçalar yeniden oluşturulur: parçalara tutulan `Entity` handle'ları geçersizleşir
  (dışarıdan parçaya giden component referansları korunur, yerel değişkenler korunmaz).
- Kodla parçalara eklenen yeni çocuk entity'ler sahnede kendi kayıtları olarak kalır; hot-reload'da
  ebeveyn parça yeniden oluşturulduğundan bu çocukların Parent'ı boşa düşer.
- `deleted` yolu artık olmayan bir parçayı gösterirse uyarı verilir, kayıt yine yüklenir.
