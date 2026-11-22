# Astral::Script — Luau scripting

Katman 2 (Core + World'e bağlı; App'e bağlı değil). Karar: [ADR-0017](../decisions/0017-gameplay-scripting.md),
uygulama kararları: [ADR-0026](../decisions/0026-script-host-and-features.md). Oyun kurallarını yazma rehberi:
[writing-a-game.md](../guides/writing-a-game.md#özellik-paketleri-ve-luau).

## API özeti

| Başlık | Ne |
|---|---|
| `script.hpp` | `ScriptHost` (modül yükle/kaldır, `run(faz, kare)`, `take_errors`), `ScriptEnv` (world, types, events, log, input), `ScriptInput` (eylem arayüzü), `ScriptLimits{budget_ms}`, `ScriptPhase`, `ScriptFrame`, `Script` component'i |
| `api.hpp` | `generate_script_api(types, events, owners)` → `astral.d.luau` + `API.md` |

App tarafı: `ctx.scripts`, `ctx.load_script(yol)` (hot reload'lu), `ctx.write_script_api(klasör)`,
`ctx.events`, `ctx.actions` ([app.md](app.md#oyun-çatısı-m11)).

## Script'ten ne yapılır

Tek global `astral` tablosu. Tam liste her oyun için üretilen `API.md`'dedir; özet:

```lua
astral.define_component("game::Health", { value = {type = "f32", default = 100}, max = "f32" })
astral.define_event("game::Died")

astral.system {                      -- her entity için
    phase = "FixedUpdate", query = {"game::Health"}, without = {"game::Dead"},
    update = function(ctx, entity, health)
        if health.value <= 0 then astral.send("game::Died", {who = entity}) end
    end,
}
astral.system { phase = "PostUpdate", run = function(ctx) ... end }   -- fazda bir kez
astral.on("game::Died", function(ctx, ev) ev.who:destroy() end)
astral.behaviour { on_start = function(self, ctx) end, on_update = function(self, ctx) end }
```

- **Component'ler** canlı vekillerdir: `c.field = v` doğrudan dünyaya yazar ve `changed<T>` filtresini işaretler.
  Değer türleri: sayı/bool/string, `vector` (Vec3), `{x,y}`/`{x,y,z,w}` tabloları (Vec2/Vec4/Quat), enum = ad
  (string), entity alanı = entity ya da `nil`.
- **Entity:** `get/has/add/remove/destroy/alive/id`. Ölü entity'ye `get/has/add/remove` hata verir.
- **Dünya:** `astral.spawn{}`, `astral.find(ad)`, `astral.query{"A","B"}` (`{ {entity, a, b}, ... }`).
- **Zaman:** `astral.after/every/cancel`. `ctx.delta`, `ctx.fixed_delta`, `ctx.time`.
- **Rastgele:** `astral.random()`/`astral.seed(n)`. Oyun kuralı için durumu component'te tutan kendi üretecini
  yaz (Pong: `Match.serve_rng`) — yeniden yüklemede ve kayıtta kaybolmaz.
- **Girdi:** `astral.input.down/pressed/released/value/axis(eylem)`.

## Çalışma kuralları

- Fazlar: `PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`; aynı fazda önce C++ sistemleri, sonra script
  modülleri (yüklenme sırasıyla; modülde önce dinleyiciler, sonra sistemler). Sıra önemliyse ayrı dosya yap
  ve doğru sırada yükle (`scoreboard.luau`, `rules.luau`'dan sonra).
- Durum component'lerde; script değişkenleri yalnızca sabitler ve yardımcı işlevler içindir.
- Hata: `dosya:satır: mesaj` loglanır, o modül durur, oyun sürer; dosya düzelince yeniden yüklenir.
  Sonsuz döngü zaman bütçesiyle kesilir. Bilinmeyen component/alan/olay adı, bilinen adları sayan hata verir.
- Sandbox: `os`, `io`, `require`, `loadstring` yok; kütüphane tabloları salt okunur.

## İçeride

`Impl` (host_impl.hpp) Luau durumunu ve modülleri tutar: her modül kendi global tablosuyla (`luaL_sandboxthread`)
çalışır, `astral.*` kayıtları o an yüklenen modüle yazılır. Entity ve component vekilleri etiketli
userdata'dır; vekil her erişimde `world.get_raw` ile çözülür (ölü entity → hata). Sistem çalıştırma önce
entity'leri toplar, sonra çağırır (script yapısal değişiklik yapabilir).

## Bilinen sınırlar

- Gamepad, tween, durum makinesi, tetik hacmi, kayıt yardımcıları, bytecode cook, luau-lsp entegrasyonu yok
  (ADR-0026).
- Sistem başına entity başına vekil oluşturulur: yüzlerce entity iyi, binlerce entity C++'a ait.
- `astral.d.luau` makine tarafından üretilir ama `luau-analyze` ile doğrulanmadı.
- Dinamik component: düz veri, en çok 256 bayt, alanları çalışırken değişmez; string/iç içe yapı yok.
- İç içe reflection (ör. `PrimitiveMesh.material`) script'ten yazılamaz.
- Olay yükü JSON üzerinden gider (sıcak döngüde pahalı; sık olaylar için C++ sistemi kullan).
- `Script` component'inin `path`'i değişirse yeni davranış `on_start` almaz, eski `started` kaydı kalır.
