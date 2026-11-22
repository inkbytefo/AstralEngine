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
- **Dünya:** `astral.spawn{}`, `astral.find(ad)`, `astral.query{"A","B"}` (`{ {entity, a, b}, ... }`),
  `astral.each({"A","B"}, function(entity, a, b) ... end)` (ara tablo ayırmadan eşleşen entity'leri akış halinde dolaşır — sıcak sorgular için tercih edilir, M13.6 P4).
- **Zaman:** `astral.after/every/cancel`. `ctx.delta`, `ctx.fixed_delta`, `ctx.time`.
- **Fizik ve alanlar** (`ScriptEnv::physics`; App sağlar): `astral.physics.raycast(origin, direction, max, {layers, ignore})` ve `.spherecast(origin, radius, displacement, opts)` → `{entity, position, normal, distance, surface}`
  ya da nil; `.overlap_sphere(center, radius, opts)`, `.overlap_box(center, half, rotation?, opts)` → entity listesi; `.impulse/.force/.set_velocity(entity, v)`, `.velocity(entity)` (entity'nin `RigidBody`'si üstünde);
  `astral.fields.wind(position)`, `astral.fields.water(position)` → `{inside, surface, depth, flow, density}`. `layers`: 0..15 sayılarının listesi ya da maske. Gövdeler ilk fizik adımında oluşur: ilk karede sorgu nil dönebilir.
  Olaylar: `astral.on("astral::TriggerEntered", fn)` (`ev.trigger`, `ev.other`), `TriggerExited`, `astral::Contact` (`ev.kind` = "Begin"/"End", `ev.a`, `ev.b`, `ev.point`, `ev.normal`, `ev.surface_a/b`) — tetik hacmi kuralı script'te yazılır (`examples/physics_playground`).
- **Kuaterniyon:** `astral.quat.axis_angle(eksen, radyan)`, `.mul(a, b)` (b önce uygulanır, C++ gibi), `.rotate(q, v)`, `.normalize(q)`;
  Quat alanları `{x,y,z,w}` tablolarıdır. Kamera/hareket kuralları için yeter.
- **Rastgele:** `astral.random()`/`astral.seed(n)`. Oyun kuralı için durumu component'te tutan kendi üretecini
  yaz (Pong: `Match.serve_rng`) — yeniden yüklemede ve kayıtta kaybolmaz.
- **Girdi:** `astral.input.down/pressed/released/value/axis(eylem)` (klavye, fare ve gamepad `.actions` dosyasından gelir).
- **Animasyon:** `astral.tween(saniye, step, {ease, on_done})` (her karede `step(t)`, t 0..1 yumuşatılmış), `astral.ease(ad, t)`, `astral.lerp(a, b, t)`.
  Eğriler: linear, in_quad, out_quad, in_out_quad, in_cubic, out_cubic, in_out_cubic, out_back. İlerleme closure'da durur (yeniden yüklemede
  kaybolur): kayda girmesi gereken animasyonu component + sistemle yaz.
- **Durum makinesi:** `astral.state_machine{initial, states = {Ad = {enter, update, exit, on = {olay = "Sonraki"}}}, store = {get, set}}` →
  `m:send(olay)`, `m:go(ad)`, `m:update(dt)`, `m:state()`. `store` durumu bir component alanında tutar (yeniden yükleme ve kayda dayanıklı).
- **Kayıt:** `astral.save(yuva, {"Component", ...})` listedeki component'lere sahip her entity'yi yazar; `astral.load(yuva)` bu component'lere
  sahip entity'leri yok edip kaydedilenleri geri getirir; `astral.has_save(yuva)`. Dosya: `<AppDesc::save_folder>/<yuva>.json`.

## Fizik ve alanlar

Bkz. yukarıdaki **Fizik ve alanlar** maddesi ve [physics.md](physics.md). Script'ten `RigidBody` oluşturulamaz (iç içe `BodyDesc` script'ten yazılmaz): gövdeyi C++ ya da sahne üretir, script kural koyar.

## Çalışma kuralları

- Fazlar: `PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`; aynı fazda önce C++ sistemleri, sonra script
  modülleri (yüklenme sırasıyla; modülde önce dinleyiciler, sonra sistemler). Sıra önemliyse ayrı dosya yap
  ve doğru sırada yükle (`scoreboard.luau`, `rules.luau`'dan sonra).
- Durum component'lerde; script değişkenleri yalnızca sabitler ve yardımcı işlevler içindir.
- Hata: `dosya:satır: mesaj` loglanır, o modül durur, oyun sürer; dosya düzelince yeniden yüklenir.
  Sonsuz döngü zaman bütçesiyle kesilir. Bilinmeyen component/alan/olay adı, bilinen adları sayan hata verir.
- Sandbox: `os`, `io`, `require`, `loadstring` yok; kütüphane tabloları salt okunur.

## Tip denetimi ve bytecode

- `astral_check_scripts(<ad> GAME <exe> SCRIPTS <klasör>)` (AstralGame.cmake): oyun `--script-api-only` ile API'yi yazar, `astral-luau-check` her script'i
  `--!strict` ile denetler. `astral.d.luau` component/olay adlarını `keyof`/`index` tip işlevleriyle indeksler: `e:get("pong::Paddle").speeed` tip hatasıdır.
- `astral-cook` `.luau` dosyalarını bytecode'a derler (`"ALBC"` + Luau bytecode; `compile_script`): sözdizimi hatası paketleme sırasında `dosya:satır` ile
  bulunur, paketli oyunda kaynak yoktur. `ScriptHost::load` kaynak ve bytecode'u ayırt eder; geliştirmede kaynak, hot reload'la.

## İçeride

`Impl` (`host_impl.hpp`) Luau durumunu ve modülleri tutar: her modül kendi global tablosuyla (`luaL_sandboxthread`)
çalışır, `astral.*` kayıtları o an yüklenen modüle yazılır. Bağlamalar iki dosyaya ayrılmıştır:
`bindings.cpp` (entity, component vekili, sistem/olay/davranış kaydı, zamanlayıcılar ve log) ve
`bindings_services.cpp` (`astral.save/load/has_save`, `astral.physics`, `astral.fields`, `astral.quat`,
`astral.input`). `api.cpp` hem `astral.d.luau` tip tanımlarını hem de `API.md` referansını üretir. Entity ve
component vekilleri etiketli userdata'dır; vekil her erişimde `world.get_raw` ile çözülür (ölü entity → hata).
Sistem çalıştırma önce entity'leri toplar, sonra çağırır (script yapısal değişiklik yapabilir).

## Bilinen sınırlar

- luau-lsp için hazır proje ayarı yok (tip tanımı üretiliyor, bkz. aşağıdaki denetim).
- Tween/durum makinesi Luau'da yazılmış yardımcılar (`src/prelude.hpp`): durumları closure'da; sıcak döngüde değil, oyun mantığında kullan.
- Kayıt yalnız listelenen component'leri saklar; entity referansları kayıt içinde yeniden eşlenir, kayıt dışındaki entity'ye giden referans `nil` olur.
- Sistem başına entity başına vekil oluşturulur: yüzlerce entity iyi, binlerce entity C++'a ait.
- `astral.d.luau` Luau'nun kendi çözümleyicisiyle doğrulanır: `astral-luau-check` aracı (`tools/astral_luau_check`) tanımı yükler ve oyunun bütün `.luau` dosyalarını denetler (`astral_check_scripts` → CTest `<oyun>_script_types`). Component/olay/alan adı yazım hatası ve yanlış tip build hatasıdır.
- Dinamik component: düz veri, en çok 256 bayt, alanları çalışırken değişmez; string/iç içe yapı yok.
- İç içe reflection (ör. `PrimitiveMesh.material`) script'ten yazılamaz.
- Olay yükü JSON üzerinden gider (sıcak döngüde pahalı; sık olaylar için C++ sistemi kullan).
- `Script` component'inin `path`'i değişirse yeni davranış `on_start` almaz, eski `started` kaydı kalır.
