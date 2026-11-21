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

- **Component kimliği** derleme zamanında tip adından hash'lenir (FNV-1a): kayıt yok, global sayaç yok.
- **Archetype**: aynı component kümesine sahip entity'ler; her component tipi için sıkı paketlenmiş
  bir sütun (SoA). `each` sütunlar üzerinde doğrusal yürür → cache dostu, kolay paralel.
- Component ekle/çıkar entity'yi komşu archetype'a taşır; geçişler önbellekli (archetype grafiği).
- Silme **swap-remove**: son satır deliğe taşınır, O(1).
- Sorgular önbellekli; yeni archetype'lar oluştukça artımlı güncellenir.
- Entity = indeks + nesil: silinen entity'nin eski handle'ı asla yeni entity'ye ulaşmaz.
- `each` sırasında yapısal değişiklik `ASTRAL_ASSERT` ile durdurulur (sessiz bellek bozulması yerine).

## Bilinen sınırlar

- `update_transforms` çocukları her karede üst zincirden yeniden hesaplar (derin hiyerarşilerde O(derinlik)).
  İleride derinliğe göre sıralı, paralel yayılım.
- Değişiklik takibi (changed/added filtreleri), olaylar, serileştirme yok.
- `get<T>` dönen pointer bir sonraki yapısal değişikliğe kadar geçerli.
