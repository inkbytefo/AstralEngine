# AstralEngine — yapay zekâ ajanları için oyun yazma kuralları

Bu dosya, motoru kullanarak **oyun** yazan ajanlar içindir (motorun kendisini değiştirirken `CLAUDE.md` ve
`docs/architecture.md`). Kısa tut, tahmin etme, oku.

## İş akışı (kapalı döngü)

1. `API.md` ve `astral.d.luau` dosyalarını oku: `pong --script-api docs/api` gibi, oyunun kendi çalıştırılabilirinden
   üretilir (`ctx.script_service().write_script_api`). Her component, alan, enum ve olay orada; kodu tahmin etme.
2. Önce başarısız test yaz (başsız `App`, `fixed_timestep = 1/60`), sonra kuralı yaz, sonra testi geçir.
3. Derle ve koş: `cmake --preset debug && cmake --build --preset debug && xvfb-run -a ctest --preset debug`.
4. Sınır ve tip denetimi: `ctest -R "features|script_types"` (özellik sınırları; script'ler `astral.d.luau`'ya karşı `--!strict` tip denetimi) (`astral-features check`). Hata mesajı ne yapacağını söyler.
5. Script hataları `dosya:satır: mesaj` ile loglanır; o modül durur, oyun sürer. Dosyayı düzelt, kendiliğinden yeniden yüklenir.

## Bir özelliğin iskeleti

```
src/features/<ad>/                 C++ (yalnız gerekiyorsa): types.hpp, <ad>.cpp  → install_<ad>(FeatureContext&)
assets/features/<ad>/feature.json  ad, depends_on, components, events, exports.headers, scripts, actions, settings
assets/features/<ad>/<ad>.luau     kurallar
assets/features/<ad>/*.actions     girdi eşlemesi
```

Yeni özellik = yeni klasör + `game.add("<ad>", install_<ad>)`. Var olan özelliğin dosyalarına dokunmak yerine yeni
sistem/olay dinleyicisi ekle (açık/kapalı ilkesi).

## Kurallar (build denetler ya da test kırar)

- **Component = veri.** `ASTRAL_REFLECT`, pointer yok, mantık yok. Script'te tanımlanan component'ler (`astral.define_component`) düz veridir.
- **Sistem = tek iş.** Sistemler birbirini çağırmaz: **olay** (`astral.send/on`, `World::send`) ya da component üzerinden konuşur.
- **Durum component'te** (script/sistem içinde gizli durum yok; hot reload, kayıt ve tekrar oynatma bunu ister).
  Rastgelelik için tohumu component'te tutan üreteç kullan (Pong'daki `Match.serve_rng`).
- **Sayılar dosyada** (reflect edilmiş ayar component'i ya da `feature.json` `settings`), kodda sabit yok.
- **Özellik sınırı:** başka özelliğin yalnız `exports.headers` başlıklarını ve `depends_on`'daki özellikleri kullan. İç motor başlığı
  (`/detail/`, `/src/`) ve değişebilir global (`static` değişken) yasak.
- **Simülasyon deterministik:** oyun kuralı `FixedUpdate`'te (`ctx.fixed_delta`), aynı tohum aynı sonuç.
- **Önce script:** kural Luau'da; yüzlerce entity'den fazlasına dokunan sıcak döngü C++ sistemine taşınır.
- `manifest = API`: `install()` içinde kaydettiğin her component/olay `feature.json`'da listelenmeli.

## Luau özeti (tam liste: üretilen `API.md`)

```lua
astral.system { phase = "FixedUpdate", query = {"A", "B"}, without = {"C"}, update = function(ctx, entity, a, b) end }
astral.system { phase = "Update", run = function(ctx) end }          -- fazda bir kez
astral.on("event::Name", function(ctx, payload) end, "FixedUpdate")  -- sistemlerden önce, modülün sırasıyla
astral.send("event::Name", { field = value })
astral.query({"A", "B"})            -- { {entity, a, b}, ... }
astral.spawn { ["astral::Name"] = {value = "x"} }  astral.find("x")
entity:get("A")  :has  :add("A", {fields})  :remove  :destroy()  :alive()
astral.input.axis("move")  astral.input.pressed("fire")
```

Değerler: `vector` (Vec3), `{x=,y=}` (Vec2), `{x=,y=,z=,w=}` (Vec4/Quat), enum = ad (string), entity alanı = entity ya da `nil`.
Fazlar: `PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`. Aynı fazda önce C++ sistemleri, sonra script modülleri
(yükleme sırasıyla). Sıra önemliyse ayrı dosya yap (`feature.json` `scripts` sırası).

Yardımcılar: `astral.tween(s, step, {ease})`, `astral.state_machine{...}`, `astral.save(slot, {components})` / `astral.load(slot)`, `astral.quat.*`.
Ortak özellikler `features/` altında (`fly_camera`): oyuna kopyalanır (`python3 tools/sync_features.py`), kopyayı elle düzenleme.

Fizik: `RigidBody` C++'ta/sahnede oluşur; script `astral.physics.raycast/spherecast/overlap_*` ve `impulse/force/set_velocity/velocity` ile entity üstünde çalışır,
`astral.fields.wind/water` alanları okur, tetik ve temas olaylarını `astral.on("astral::TriggerEntered" | "astral::Contact", ...)` dinler. Gövdeye `AirDrag`/`Buoyant`/`Airfoil` ekleyince rüzgâr ve su kuvvetleri kendiliğinden uygulanır.

## Yapma

- `PrimitiveMesh.material` gibi iç içe reflect alanlarını script'ten yazmaya çalışma (görünmez): C++ sistemi yaz.
- Entity'yi kareler arası tutarken `entity:alive()` kontrol et (ölü entity'ye erişim hata verir).
- Script içinde `os`, `io`, `require` arama: sandbox'ta yok.
- Hata mesajını yutma: log satırı kural dosyasındaki yeri söyler.

Yeni oyun: `templates/blank` klasörünü kopyala (özellik, Luau, test, `--capture`, paketleme hazır). Örnekler aynı düzende: `examples/pong`
(`pong_rules` Luau kuralları, `pong_scorelights`, `pong_audio`; testler `tests/game_test.cpp`), `examples/spinning_cubes` (C++ paralel sistem + Luau),
`examples/hello_headless` (pencere/GPU yok). Düzen: `src/main.cpp` (standart kabuk), `src/game.cpp` (özellikleri kurar), `src/features/<ad>/`,
`assets/features/<ad>/`, `tests/game_test.cpp`, `CMakeLists.txt` (`astral_add_game`, `astral_check_features`).

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. When the answer is under a Markdown heading, `graphify section "<heading>"` prints that section's text, so the .md need not be opened. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.
