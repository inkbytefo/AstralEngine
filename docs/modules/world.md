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
- Değişiklik takibi (changed/added filtreleri), `without<T>` filtresi, olaylar, serileştirme yok.
- `commands().spawn()` oluşturulan entity'yi döndürmüyor (ertelenmiş referans yok).
- `get<T>` dönen pointer bir sonraki yapısal değişikliğe kadar geçerli.
