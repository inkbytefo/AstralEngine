# AstralEngine — yapay zekâ ajanları için oyun yazma kuralları

Bu dosya, motoru kullanarak **oyun** yazan ajanlar içindir (motorun kendisini değiştirirken `CLAUDE.md` ve
`docs/architecture.md`). Kısa tut, tahmin etme, oku.

## İş akışı (kapalı döngü)

1. `API.md` ve `astral.d.luau` dosyalarını oku: `pong --script-api docs/api` gibi, oyunun kendi çalıştırılabilirinden
   üretilir (`ctx.write_script_api`). Her component, alan, enum ve olay orada; kodu tahmin etme.
2. Önce başarısız test yaz (başsız `App`, `fixed_timestep = 1/60`), sonra kuralı yaz, sonra testi geçir.
3. Derle ve koş: `cmake --preset debug && cmake --build --preset debug && xvfb-run -a ctest --preset debug`.
4. Sınır denetimi: `ctest -R features` (`astral-features check`). Hata mesajı ne yapacağını söyler.
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

## Yapma

- `PrimitiveMesh.material` gibi iç içe reflect alanlarını script'ten yazmaya çalışma (görünmez): C++ sistemi yaz.
- Entity'yi kareler arası tutarken `entity:alive()` kontrol et (ölü entity'ye erişim hata verir).
- Script içinde `os`, `io`, `require` arama: sandbox'ta yok.
- Hata mesajını yutma: log satırı kural dosyasındaki yeri söyler.

Yeni oyun: `templates/blank` klasörünü kopyala (özellik, Luau, test, `--capture`, paketleme hazır). Örnekler aynı düzende: `examples/pong`
(`pong_rules` Luau kuralları, `pong_scorelights`, `pong_audio`; testler `tests/game_test.cpp`), `examples/spinning_cubes` (C++ paralel sistem + Luau),
`examples/hello_headless` (pencere/GPU yok). Düzen: `src/main.cpp` (standart kabuk), `src/game.cpp` (özellikleri kurar), `src/features/<ad>/`,
`assets/features/<ad>/`, `tests/game_test.cpp`, `CMakeLists.txt` (`astral_add_game`, `astral_check_features`).
