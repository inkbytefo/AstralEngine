# AstralEngine — yapay zekâ ajanları için oyun yazma kuralları

Bu dosya, motoru kullanarak **oyun** yazan ajanlar içindir. Motorun kendisini değiştiriyorsan `CLAUDE.md` ve
[docs/architecture.md](docs/architecture.md) geçerli. Kısa tut, tahmin etme, oku.

Ayrıntılı rehberler: [oyun yazmak](docs/guides/writing-a-game.md), [oyun mimarisi](docs/guides/game-architecture.md),
[sahne ve dünyalar](docs/guides/scenes-and-worlds.md), [`.ashader` dili](docs/guides/ashader-language.md),
[shader ve materyal iş akışı](docs/guides/shaders-and-materials.md). Motorun dürüst durumu: [docs/status.md](docs/status.md).

## İş akışı (kapalı döngü)

1. Oyunun ürettiği `API.md` ve `astral.d.luau` dosyalarını oku. Üretmek için: `<oyun> --script-api docs/api` (oyun açılınca yazar) ya da
   `<oyun> --script-api-only docs/api` (`modules/app/src/shell.cpp:76-80`). Her component, alan, enum ve olay orada. Kodu tahmin etme.
2. Önce başarısız test yaz (başsız `App`, `fixed_timestep = 1/60`), sonra kuralı yaz, sonra testi geçir. Girdi gereken testte
   `AppDesc::input_script` kullan: kare numarasıyla tuş basma/bırakma (`modules/app/include/astral/app/app.hpp:207-213`, [ADR-0046](docs/decisions/0046-headless-input-script.md)).
3. Derle ve koş: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`
   (Linux'ta pencereli testler için `xvfb-run -a ctest --preset debug`).
4. Sınır ve tip denetimi: `ctest --preset debug -R "features_check|script_types"`. `<oyun>_features_check` `astral-features check` çalıştırır.
   `<oyun>_script_types` ise script'leri `astral.d.luau`'ya karşı `--!strict` denetler (`cmake/AstralGame.cmake:63-102`).
   Hata mesajı ne yapacağını söyler.
5. Script hataları `dosya:satır: mesaj` ile loglanır. O modül durur, oyun sürer. Dosyayı düzelt, kendiliğinden yeniden yüklenir.

## Bir özelliğin iskeleti

```
src/features/<ad>/                 C++ (yalnız gerekiyorsa): types.hpp, <ad>.cpp  → install_<ad>(FeatureContext&)
assets/features/<ad>/feature.json  ad, depends_on, components, events, exports.headers, scripts, actions, settings
assets/features/<ad>/<ad>.luau     kurallar
assets/features/<ad>/*.actions     girdi eşlemesi
```

Yeni özellik için yeni klasör aç ve `game.add("<ad>", install_<ad>)` ile ekle. Var olan özelliğin dosyalarına dokunma; yeni sistem ya da olay
dinleyicisi ekle (açık/kapalı ilkesi).

## Kurallar (build denetler ya da test kırar)

- **Component = veri.** `ASTRAL_REFLECT` ile tanımlanır. İçinde pointer ve mantık olmaz. Bir struct en çok 32 alan reflect edebilir; fazlası
  derlemede `static_assert` verir. Script'te tanımlanan component'ler (`astral.define_component`) düz veridir.
- **Sistem = tek iş.** Sistemler birbirini çağırmaz. **Olay** (`astral.send/on`, `World::send`) ya da component üzerinden konuşur.
- **Durum component'te.** Script ya da sistem içinde gizli durum tutma; hot reload, kayıt ve tekrar oynatma bunu ister. Rastgelelik için tohumu
  component'te tutan üreteç kullan (Pong'daki `Match.serve_rng`). `astral.random()`/`astral.seed()` script host'unun tek üretecidir
  (`modules/script/src/bindings.cpp:505-514`): kayda girmez, oyun kuralında kullanma.
- **Sayılar dosyada.** Reflect edilmiş ayar component'ine ya da `feature.json` `settings`'e yaz. Kodda sabit bırakma.
- **Özellik sınırı.** Başka bir özellikten yalnız `exports.headers` başlıklarını ve `depends_on`'daki özellikleri kullan. İç motor başlığı
  (`/detail/`, `/src/`) ve değişebilir global (`static` değişken) yasak.
- **Simülasyon deterministik.** Oyun kuralı `FixedUpdate`'te çalışır (`ctx.fixed_delta`). Aynı tohum aynı sonucu vermeli.
- **Önce script.** Kurallar Luau'da yazılır. Yüzlerce entity'den fazlasına dokunan sıcak döngü C++ sistemine taşınır. Script zaman
  bütçesi varsayılan 100 ms'dir; aşan modül durur (`modules/script/include/astral/script/script.hpp:157`).
- **`manifest = API`.** `install()` içinde kaydettiğin her component ve olay `feature.json`'da listelenmeli.

## Luau özeti (tam liste: üretilen `API.md`)

```lua
astral.system { phase = "FixedUpdate", query = {"A", "B"}, without = {"C"}, update = function(ctx, entity, a, b) end }
astral.system { phase = "Update", run = function(ctx) end }          -- fazda bir kez
astral.on("event::Name", function(ctx, payload) end, "FixedUpdate")  -- sistemlerden önce, modülün sırasıyla
astral.send("event::Name", { field = value })
astral.query({"A", "B"})            -- { {entity, a, b}, ... }
astral.each({"A", "B"}, function(entity, a, b) end) -- ara tablo ayırmadan toplu sorgu
astral.spawn { ["astral::Name"] = {value = "x"} }  astral.find("x")
entity:get("A")  :has  :add("A", {fields})  :remove  :destroy()  :alive()  :id()
astral.input.axis("move")  .pressed("fire")  .released  .down  .value
astral.after(s, fn)  astral.every(s, fn)  astral.cancel(id)   -- zamanlayıcı
```

Değerler: `vector` (Vec3), `{x=,y=}` (Vec2), `{x=,y=,z=,w=}` (Vec4/Quat). Enum adıyla (string) yazılır. Entity alanı entity ya da `nil` olur.
Fazlar: `PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`. Aynı fazda önce C++ sistemleri, sonra script modülleri çalışır (yükleme sırasıyla).
Sıra önemliyse ayrı dosya yap (`feature.json` `scripts` sırası).

Yardımcılar: `astral.tween(s, step, {ease})`, `astral.ease`, `astral.lerp`, `astral.state_machine{...}`, `astral.save(slot, {components})` /
`astral.load(slot)` / `astral.has_save(slot)`, `astral.quat.axis_angle/mul/normalize/rotate`.
Ortak özellikler `features/` altındadır (`fly_camera`). Oyuna `python tools/sync_features.py` ile kopyalanır; kopyayı elle düzenleme
(`--check` farkları listeler).

**Fizik** (`modules/script/src/bindings_services.cpp:468-492`):
- `RigidBody.desc` ve `CharacterController.desc` C++'tan ya da sahne dosyasından gelir.
- Karakter: sabit adımda `astral.physics.move_character(entity, velocity, jump?)` çağır, önceki adımı `character_state(entity)` ile oku.
- Sorgular: `raycast`, `raycast_batch`, `spherecast`, `overlap_sphere`, `overlap_box`. Seçenek tablosu `{ignore = {e1, e2}, sensors = true}`
  ([ADR-0045](docs/decisions/0045-query-filter-ignore-list.md)).
- Gövdeler: `impulse`, `force`, `force_at`, `torque`, `angular_impulse`, `set_velocity`, `velocity`, `set_angular_velocity`, `angular_velocity`,
  `teleport`, `wake`, `sleep`.
- Eklemler: `set_joint_motor_mode/velocity/position`, `break_joint`.
- Alanlar: `astral.fields.wind(p)` ve `astral.fields.water(p)` okunur. Gövdeye `AirDrag`, `Buoyant` ya da `Airfoil` eklersen rüzgâr ve su
  kuvvetleri kendiliğinden uygulanır.
- Olaylar: `astral.on("astral::TriggerEntered" | "astral::Contact", ...)`.

**Efekt:** `astral.send("astral::PlayVfx", {asset = "vfx/x.vfx", position = p, tag = "t"})`, durdurmak için
`astral.send("astral::StopVfx", {tag = "t"})` (`modules/app/include/astral/app/vfx.hpp:14-21`).

**Görünüm:** materyal ve shader `.material` / `.ashader` dosyalarıdır. Sahnede `astral::PrimitiveMesh.material_file` ya da
`ModelRef.material_overrides` ile bağlanır. Luau'dan materyal API'si yok
([shaders-and-materials.md](docs/guides/shaders-and-materials.md)).

## Yapma

- `PrimitiveMesh.material` gibi iç içe reflect alanlarını script'ten yazmaya çalışma (görünmez). C++ sistemi yaz.
- Entity'yi kareler arasında tutarken `entity:alive()` kontrol et. Ölü entity'ye erişim hata verir.
- Script içinde `os`, `io` ya da `require` arama: sandbox'ta yok.
- Hata mesajını yutma. Log satırı kural dosyasındaki yeri söyler.
- Entity başına materyal ezmesi bekleme (`MaterialOverride` yok). Farklı görünüm için ayrı `.material` dosyası kullan.

Yeni oyun için `templates/blank` klasörünü kopyala: özellik, Luau, test, `--capture` ve paketleme hazır gelir. Örnekler aynı düzende:
- `examples/pong`: `pong_rules` Luau kuralları, `pong_scorelights`, `pong_audio`; testler `tests/game_test.cpp`.
- `examples/spinning_cubes`: C++ paralel sistem + Luau.
- `examples/hello_headless`: pencere ve GPU yok.

Dosya düzeni: `src/main.cpp` (standart kabuk), `src/game.cpp` (özellikleri kurar), `src/features/<ad>/`, `assets/features/<ad>/`,
`tests/game_test.cpp`, `CMakeLists.txt` (`astral_add_game`, `astral_check_features`, `astral_check_scripts`).

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
