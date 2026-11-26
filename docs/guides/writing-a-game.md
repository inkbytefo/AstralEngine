# Oyun nasıl yazılır

> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi. Kod örnekleri `templates/blank` ve motor başlıklarındaki
> imzalarla karşılaştırıldı; bu belgedeki parçalar bu oturumda derlenip çalıştırılmadı (yalnız `templates/blank`
> dosyaları motorun kendi testlerinde derleniyor). Mimari model için [game-architecture.md](game-architecture.md),
> sahne dosyaları için [scenes-and-worlds.md](scenes-and-worlds.md).

Bu rehber "yeni bir oyunu sıfırdan nasıl ayağa kaldırırım" sorusunu cevaplar. Motor kütüphanedir, döngünün sahibi
`App`'tir; oyun `Game` sınıfı + özellik paketleri (feature) + asset'lerden oluşur.

## 1. Başlangıç: `templates/blank`

Yeni oyun = `templates/blank` klasörünü kopyalamak. İçinde derlenen bir oyun, başsız test, özellik sınırı denetimi,
Luau tip denetimi ve paketleme hedefi hazırdır.

| Dosya | Görevi | Kaynak |
|---|---|---|
| `CMakeLists.txt` | Oyun kütüphanesi (`blank_game_lib`) + exe (`astral_add_game`) + testler + denetimler | `templates/blank/CMakeLists.txt:14` |
| `src/main.cpp` | Standart `main`: `parse_shell_args` + `run_game` | `templates/blank/src/main.cpp:9` |
| `src/game.cpp` | `FeatureSet` kurar, ilk sahneyi açar; kural yok | `templates/blank/src/game.cpp:10` |
| `src/features/<ad>/` | Özelliğin C++ tarafı (component tipleri, ağır sistemler, `install_<ad>`) | `templates/blank/src/features/spin/spin.hpp:10` |
| `assets/features/<ad>/` | `feature.json` + Luau kuralları + `.actions` | `templates/blank/assets/features/spin/feature.json:1` |
| `assets/main.scene` | İlk sahne (kamera, zemin, küp) | `templates/blank/assets/main.scene` |
| `tests/game_test.cpp` | Başsız, deterministik testler | `templates/blank/tests/game_test.cpp:11` |

Derleme ve çalıştırma (`templates/blank/README.md`'deki akış):

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<motorun kurulum klasörü>   # motor reposunun içindeysen gerekmez
cmake --build build && ctest --test-dir build
build/blank                         # oyna
build/blank --capture out.ppm       # tek görüntü (GPU gerekir)
build/blank --script-api docs/api   # astral.d.luau + API.md
cmake --build build --target blank_package   # dağıtılabilir klasör: build/packages/blank/
```

- Motor reposunun içinde `templates/blank` `ASTRAL_BUILD_EXAMPLES` açıkken kendiliğinden eklenir
  (`CMakeLists.txt:114-115`); dışarıdan `find_package(Astral 0.17 ...)` ile bulunur (`templates/blank/CMakeLists.txt:10`).
  Sürüm numarasını kopyaladığın motorun sürümüyle eşle (`project(Astral VERSION 0.17.0 ...)`, `CMakeLists.txt:2`).
- CI, şablonu kurulu motora karşı ayrıca derleyip test ediyor (`.github/workflows/ci.yml:48-53`).

## 2. Oyunun üç parçası

### 2.1 `main`

```cpp
#include "game.hpp"
#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    blank::BlankGame game;
    return astral::run_game(options,
                            {.title = "Blank game", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK},
                            game);
}
```

`run_game` (`modules/app/include/astral/app/shell.hpp:49`) komut satırına göre pencerede çalıştırır, tek bir çevrimdışı
görüntü alır veya yalnız script API belgesini yazıp çıkar. Bayraklar:

| Bayrak | Etki | Kaynak |
|---|---|---|
| (yok) | Pencerede oyna; Esc çıkar | `modules/app/src/shell.cpp:99` |
| `--capture out.ppm [--frames N] [--width W] [--height H]` | Offscreen, sabit 60 Hz adım, son kare `.ppm`; GPU yoksa çıkış kodu 1 | `modules/app/src/shell.cpp:162-168` |
| `--script-api <klasör>` | Oyun başladıktan sonra `astral.d.luau` + `API.md` yaz, oyun sürer | `modules/app/src/shell.cpp:96` |
| `--script-api-only <klasör>` | Aynısı, başsız tek kare, sonra çık (CI/araç için) | `modules/app/src/shell.cpp:150-156` |
| diğerleri | Oyuna kalır: `options.has("--night")`, `options.value("--view", "0")`, `options.positional()` | `modules/app/include/astral/app/shell.hpp:26` |

`--script-api` çıktısı yalnız `on_start` sırasında kurulmuş component/olayları içerir: özellikleri `on_start`'ta
yüklemelisin (`templates/blank/src/game.cpp:15-21`).

### 2.2 `Game`

`Game`'in dört kancası var: `on_start`, `on_update`, `on_shutdown`, `on_debug_ui` (`modules/app/include/astral/app/app.hpp:50-58`).
Blank oyunda `on_start` yalnız özellikleri kurar ve sahneyi açar:

```cpp
void BlankGame::on_start(Context& ctx) {
    if (!install_features(ctx)) { ctx.quit(); return; }
    ctx.scenes().load_scene("main.scene");
}
```

`ctx.scenes().load_scene(yol)` asset kökünden okur, mevcut dünyaya **ekler** (eski sahneyi silmez)
(`modules/app/include/astral/app/scene_service.hpp:17-19`). Hata listesi `LoadResult::errors`'ta ve logda.

`Context` her şeye uzanan tek yüz: `world`, `input`, `time`, `jobs`, `physics`, `assets`, `audio`, `types`, `events`,
`actions`, `scripts`, `hud`, `debug`… (`modules/app/include/astral/app/app.hpp:68-131`). Pencere, renderer ve cihaz başsız
modda `nullptr`'dır (`modules/app/include/astral/app/app.hpp:75-77`). Hizmetler gruplu erişimcilerle gelir: `scenes()`, `maps()`, `materials()`,
`audio_service()`, `script_service()`, `physics_service()` (`modules/app/include/astral/app/app.hpp:173-186`).

### 2.3 Özellik paketi (feature)

Bir özellik küçük bir klasördür: C++ tarafı `src/features/<ad>/`, veri/kural tarafı `assets/features/<ad>/`.
`feature.json` özelliğin **API'sidir** (`modules/app/include/astral/app/feature.hpp:1-17`):

| Alan | Anlamı | Doğrulama |
|---|---|---|
| `format`, `version`, `name` | `format` `"astral.feature"` olmalı; `name` klasör adıyla aynı olmalı | `modules/app/src/feature.cpp:35` ve `modules/app/src/feature.cpp:147` |
| `depends_on` | Önce kurulacak özellikler; yoksa/döngüyse kurulum durur | `modules/app/src/feature.cpp:97` ve `modules/app/src/feature.cpp:113` |
| `components`, `events` | `install()` içinde kaydedilenler burada **listelenmeli** | `modules/app/src/feature.cpp:58-64` |
| `exports.headers` | Başka özelliğin `#include` edebileceği başlıklar | `modules/app/src/feature_check.cpp:106` |
| `scripts`, `actions` | Yüklenecek `.luau` / `.actions` dosyaları (asset yolu) | `modules/app/src/feature.cpp:161-167` |
| `settings` | Serbest nesne; `FeatureContext::number/text/flag` ile okunur | `modules/app/include/astral/app/feature.hpp:71-73` |

Kurulum:

```cpp
FeatureSet game;
game.add("spin", install_spin);   // manifest assets/features/spin/feature.json'dan okunur, sonra install_spin çalışır
if (!game.install(ctx)) ctx.quit();
```

- Saf Luau özellik için install işlevi gerekmez: `game.add("ad")` (`modules/app/include/astral/app/feature.hpp:7`, `modules/app/include/astral/app/feature.hpp:88`).
- `install` önce **bütün** manifestleri okur ve bağımlılık grafiğini doğrular; yanlışsa hiçbir özellik kurulmaz
  (`modules/app/include/astral/app/feature.hpp:93-95`). Grafik doğruysa özellikler bağımlılık sırasıyla (yoksa ekleme sırasıyla) kurulur ve **ilk hatada
  durur**: o ana kadar kurulmuş özellikler dünyada kalır (`modules/app/src/feature.cpp:154-168`).
- Tersi denetlenmez: manifestte listelenip `install()`'da kaydedilmeyen component hata vermez
  (`modules/app/src/feature.cpp:58-64` yalnız "kaydedildi ama listelenmedi" yönünü yakalar).
- Dosya yolları `folder` (varsayılan `"features"`) altından aranır: `assets/features/<ad>/feature.json`.

## 3. C++ sistemi mi, Luau mu?

Önce Luau; ayrıca bir gerekçe varsa C++ (proje kuralı, `AGENTS.md`). Karar tablosu:

| Durum | Seçim | Neden (kaynak) |
|---|---|---|
| Oyun kuralı, girdi, sayaç, durum makinesi | Luau | Hot reload (0,5 sn'de bir içerik özeti karşılaştırılır, `modules/app/src/services/script_bridge.cpp:227`); hata `dosya:satır` ile loglanır, bozuk dosya eski sürümü bırakır (`modules/script/include/astral/script/script.hpp:167-170`) |
| Binlerce entity üstünde sıcak döngü | C++ | Luau fazı toplamda 100 ms bütçesiyle sınırlı ve aşılınca durdurulur (`modules/script/include/astral/script/script.hpp:155-158`); bütçe `AppDesc`'ten ayarlanamıyor (`modules/app/src/services/script_bridge.cpp:113`) |
| Paralel çalışması gereken iş | C++ | C++ sistemleri erişimini bildirirse çakışmayanlar paralel dalgalarda koşar (`modules/app/include/astral/app/system.hpp:22-23`); Luau tek iş parçacığında, C++ sistemlerinden sonra koşar (`modules/app/include/astral/app/app.hpp:127-128`) |
| İç içe reflect alanına (`PrimitiveMesh.material`) yazmak | C++ | `FieldKind::Other` alanlar script'e görünmez (`modules/world/include/astral/world/fields.hpp:28`, `modules/script/src/api.cpp:156`) |
| Motor servisi (renderer, ses, `PhysicsWorld`) | C++ | Luau yalnız `astral` tablosuna uzanır (`modules/script/include/astral/script/script.hpp:2-4`) |

### 3.1 Luau kuralı (blank'tan)

```lua
--!strict
astral.system({
    name = "spin",
    phase = "Update",
    query = { "astral::Transform", "blank::Spin" },
    update = function(ctx, _, transform, spin)
        local turn = astral.quat.axis_angle(spin.axis, spin.speed * ctx.delta)
        transform.rotation = astral.quat.normalize(astral.quat.mul(turn, transform.rotation))
    end,
})
```

Kaynak: `templates/blank/assets/features/spin/spin.luau:4`. Tam `astral.*` yüzeyi için oyunun kendi çıktısını üret:
`oyun --script-api docs/api` → `API.md` + `astral.d.luau` (komut katalogu `modules/script/src/api.cpp:9`). Luau ayrıntıları
[game-architecture.md](game-architecture.md#6-luau-ile-gameplay).

### 3.2 C++ sistemi

Bir component'i C++'ta tanımla, yansıt (reflect), özellikte kaydet:

```cpp
// spin.hpp
struct Wobble { float speed = 2.0f; float height = 0.25f; };
ASTRAL_REFLECT(blank::Wobble, speed, height)     // global kapsamda, tipten sonra
```

```cpp
void install_wobble(astral::FeatureContext& feature) {
    feature.component<Wobble>();   // feature.json "components" listesinde olmalı
    feature.system(astral::Phase::Update, "wobble",
        [](astral::Context& ctx) {
            ctx.world.each<astral::Transform, const Wobble>([&](astral::Transform& t, const Wobble& w) {
                t.position.y = std::sin(static_cast<float>(ctx.time.elapsed) * w.speed) * w.height;
            });
        },
        astral::SystemAccess{}.query<astral::Transform, const Wobble>());
}
```

İmzalar: `FeatureContext::component<T>()` / `system(Phase, name, fn, SystemAccess)` (`modules/app/include/astral/app/feature.hpp:52`, `modules/app/include/astral/app/feature.hpp:66`),
`World::each` (`modules/world/include/astral/world/world.hpp:165`), `SystemAccess::query` (`modules/app/include/astral/app/system.hpp:35`).
Sınıf biçimi: `class X final : public System { void update(Context&) override; }` + `feature.system<X>(Phase::Update)`
(`modules/app/include/astral/app/system.hpp:61-68`, `modules/app/include/astral/app/feature.hpp:62`). Notlar:

- Component'ler **nothrow move-constructible** olmalı; derleme zamanı denetlenir (`modules/world/include/astral/world/component.hpp:66-67`).
- `ASTRAL_REFLECT` en çok 32 alan alır (`modules/core/include/astral/core/reflect.hpp:157`); enum alanları için
  `ASTRAL_REFLECT_ENUM` (`modules/core/include/astral/core/reflect.hpp:174`). Yansıtılmayan component çalışır ama sahne dosyası, inspector ve Luau'ya görünmez.
- `each` içinde `create/destroy/add/remove` yasaktır ve **her yapılandırmada** süreci durdurur (`ASTRAL_ASSERT` her zaman açık:
  `modules/world/src/world.cpp:226`, `modules/core/include/astral/core/assert.hpp:8`); yerine `ctx.world.commands()`
  kullan (`modules/world/include/astral/world/world.hpp:211`). Komutlar her faz sonunda uygulanır (`modules/app/src/services/frame_loop.cpp:118-123`).
- Erişim bildirmeyen (veya `mark_exclusive()` diyen) sistem tek başına ana iş parçacığında koşar (`modules/app/include/astral/app/system.hpp:46`).
  Bildirilen erişim **doğrulanmaz**: yanlış bildirim yarış durumu yaratır (kodda erişimi denetleyen bir mekanizma
  bulunamadı).

## 4. Girdi: `.actions`

Tuş kodu yerine eylem kullan; `.actions` dosyası tuşları/fare/gamepad'i eylemlere eşler (`modules/app/include/astral/app/input_actions.hpp:1-19`):

```json
{"format": "astral.actions", "version": 1, "actions": {
  "fire":  {"type": "button", "keys": ["Space"], "mouse": ["Left"], "gamepad": ["A"]},
  "move":  {"type": "axis2", "x": {"negative": ["A"], "positive": ["D"]}, "y": {"negative": ["S"], "positive": ["W"]},
            "stick": "left", "dpad": true},
  "look":  {"type": "axis2", "mouse_delta": [0.012, -0.012]}
}}
```

- Yükleme: manifestin `actions` listesi veya `ctx.script_service().load_actions(yol)` (`modules/app/include/astral/app/script_service.hpp:25`). Dosya değişince
  yeniden yüklenir (`modules/app/src/services/script_bridge.cpp:225-240`).
- Birden çok `.actions` dosyası **ad bazında birleştirilir**; aynı ada sahip eylemde sonraki dosya kazanır
  (`modules/app/src/services/script_bridge.cpp:159-174`). `InputActions::load` tek belge içindir ve tümünü değiştirir (`modules/app/include/astral/app/input_actions.hpp:34`) —
  eski belgelerdeki "ikinci dosya ilkini ezer" bilgisi artık geçerli değil.
- C++'ta `ctx.actions.pressed("fire")`, Luau'da `astral.input.pressed("fire")`. Gamepad: ilk bağlı cihaz (`modules/app/include/astral/app/input_actions.hpp:13`).

## 5. Başsız testler

Mantığı pencere ve GPU olmadan test et. Şablonun testi:

```cpp
App make_app() {
    return App({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
}

TEST_CASE("the cube spins: the Luau rule turns every Spin") {
    App app = make_app();
    blank::BlankGame game;
    app.run_frames(game, 60);                       // bir saniye
    const Entity cube = find_by_name(app.world(), "cube");
    REQUIRE(cube.valid());
    const Transform* t = app.world().get<Transform>(cube);
    CHECK(t->rotation.w < 0.9f);
    CHECK(app.context().scripts->take_errors().empty());   // script çalışma hataları
}
```

Kaynak: `templates/blank/tests/game_test.cpp:11-37`.

- `AppDesc::headless`: pencere yok, GPU yok; ses sessiz karışır (`modules/app/include/astral/app/app.hpp:220`, `modules/app/include/astral/app/app.hpp:242-243`). `fixed_timestep > 0`: her kare tam o
  kadar ilerler (`modules/app/include/astral/app/app.hpp:222`); fizik hızı varsayılan 60 Hz olduğundan `1/60` her karede tam bir sabit adım demektir
  (`modules/app/src/app.cpp:296`, `modules/app/src/services/physics_bridge.cpp:388`).
- `run_frames(game, n)` başlatma ve kapanışla birlikte tam `n` kare koşar (`modules/app/include/astral/app/app.hpp:280-281`). Bir `App` **bir kez**
  çalıştırılabilir: ikinci `run`/`run_frames` hata loglar ve dönmez (`modules/app/src/services/frame_loop.cpp:44-48`).
- Başsız modda `ctx.materials().create_material` geçersiz handle döner (çizim yok, `modules/app/include/astral/app/material_service.hpp:18`);
  `read_frame` yalnız offscreen modda çalışır (`modules/app/include/astral/app/app.hpp:286`).
- Sahne dosyası testte yüklenecekse `.asset_root`'u ver (`ASTRAL_GAME_ASSETS`), `hot_reload = false` yap.

### 5.1 Betikli girdi: `AppDesc::input_script`

Başsız/offscreen çalışmada tuş basma/bırakma zamanlaması için (`modules/app/include/astral/app/app.hpp:207-213,255-257`):

```cpp
const InputScriptEvent script[] = {
    {.frame = 2, .key = Key::R, .down = true},     // sırası karışık olabilir, App kareye göre sıralar
    {.frame = 4, .key = Key::R, .down = false},
};
App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false,
         .input_script = script});
app.run_frames(game, 8);
```

- Olay `frame` karesinin başında uygulanır (`Time::frame`, 0 = ilk kare). Pencere yoksa App her kare kendisi yeni girdi
  karesi başlatır, yani `pressed`/`released` gerçek kenarlardır (`modules/app/include/astral/app/app.hpp:255-256`, `modules/app/src/app.cpp:199-206`).
- App listeyi **kopyalar** (`modules/app/src/app.cpp:297`) ve kareye göre sabit sıralar (`modules/app/src/app.cpp:298`); span'in sahibi olmak gerekmez.
- Test örneği: `modules/app/tests/input_script_test.cpp:28-73` (R basılı kareler `{2,3,6,7}`, `pressed` kareleri `{2,6}`).
- Yalnız tuş verir; fare/gamepad betiği yok (`InputScriptEvent` yalnız `Key` taşır).

### 5.2 Test yazarken tuzaklar

- **Çıkarım (çalıştırılarak doğrulanmadı):** girdi karede bir kez güncellenir (`modules/app/src/services/frame_loop.cpp:67`, `modules/app/src/services/frame_loop.cpp:93-96`), `FixedUpdate` ise
  karede 0..N kez koşar (`modules/app/src/services/physics_bridge.cpp:388`); `pressed` `FixedUpdate`'te adım sayısı kadar "true" okunabilir veya
  hiç okunmayabilir. Tek seferlik istekleri `Update`'te component'e yaz, `FixedUpdate`'te tüket.
- Paralel `ctest` yükü altında Luau 100 ms bütçesi aşılabilir: bu makinede `pong_tests` ve `spinning_cubes_tests` paralel
  koşuda kırıldı, seri koşuda geçti (ana ajanın ölçümü; kök neden incelenmedi).
- GPU gerektiren görüntü testleri (`golden_hw_*`) bu makinede 7 örnekte (ton farkı) kalıyor; oyun mantığı testleriyle ilgili değil.

## 6. Bir fizik/sahne/HUD kesiti

Görünüm ve fizik API'leri ayrıntısıyla [game-architecture.md](game-architecture.md) ve [shaders-and-materials.md](shaders-and-materials.md)'de.
Burada yalnız doğrulanmış çağrı biçimleri:

```cpp
// Materyal (paylaşılan kaynak; başsız modda geçersiz handle)
MaterialHandle gold = ctx.materials().create_material({.base_color = {0.9f, 0.7f, 0.3f, 1}, .roughness = 0.2f, .metallic = 1.0f});
ctx.world.create(Transform{}, MeshRenderer{ctx.meshes.sphere, gold});

// Fizik: zemin (statik) + düşen top
ctx.world.create(Transform{.position = {0, -0.5f, 0}}, MeshRenderer{ctx.meshes.cube},
                 RigidBody{.desc = {.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static}});

// Ses ve HUD
ctx.audio->play(ctx.audio_service().sound("sfx/hit.wav"), {.pitch = 1.2f, .spatial = true, .position = p});
ctx.hud.text(*ctx.font, "Skor 3", {16, 16}, {.size = 48, .shadow = true});   // piksel, sol üst orijin; her kare temizlenir
```

Kaynaklar: `MaterialDesc` `modules/render/include/astral/render/renderer.hpp:98`, `MeshRenderer` `modules/app/include/astral/app/components.hpp:17`,
`RigidBody` `modules/app/include/astral/app/components.hpp:88`, `ShapeDesc::box` `modules/physics/include/astral/physics/physics.hpp:47`,
`PlayDesc` `modules/audio/include/astral/audio/audio.hpp:42`, `UiCanvas::text` `modules/render/include/astral/render/text.hpp:106`.
F1 debug arayüzü `AppDesc::debug_ui` açıkken kullanılabilir (`modules/app/include/astral/app/app.hpp:237`); kendi paneli `Game::on_debug_ui` içinde Dear ImGui ile çiz.

## 7. Paketleme

```cmake
astral_add_game(blank SOURCES src/main.cpp ASSETS assets)   # templates/blank/CMakeLists.txt:24
```

`astral_add_game(<ad> SOURCES ... ASSETS <klasör>)` (`cmake/AstralGame.cmake:15`):

- `<ad>` exe'sini `Astral::App`'e bağlar ve iki tanım verir: `ASTRAL_GAME_ASSETS` (kaynak asset klasörü, mutlak) ve
  `ASTRAL_GAME_PACK` (`"<ad>.apak"`) (`cmake/AstralGame.cmake:33`).
- `<ad>_package` hedefi asset'leri pişirir ve `<build>/packages/<ad>/` altında exe + `<ad>.apak` hazırlar
  (`cmake/AstralGame.cmake:38-55`). `ASTRAL_PACKAGE_GAMES` açıksa `ALL`'a girer; varsayılan **kapalı** (`cmake/AstralGame.cmake:11`),
  yani paketi `--target <ad>_package` ile istemelisin.
- **Geliştirme:** yanında `.apak` yoksa `asset_root`'tan gevşek dosyalar okunur, hot reload açıktır. **Dağıtım:**
  paket exe'nin yanında bulunursa onu kullanır (`AppDesc::pack`, `modules/app/include/astral/app/app.hpp:228-231`;
  `modules/app/src/app.cpp:78-87`).
- Cook artımlıdır (içerik özeti). Doku → BC7 (renk/veri) veya BC5 (normal); glTF → ikili model; `.scene/.prefab/.json` →
  ikili JSON; `.world` + `.entities/` → indeks + `.cells/`; `.luau` → bytecode (derlenemezse hata); gerisi kopyalanır
  (`modules/assets/include/astral/assets/cook.hpp:70-74`). Elle: `astral-cook <kaynak> -o <çıktı> --pack oyun.apak`
  (`tools/astral_cook/main.cpp:1-12`).
- `.ashader`/`.material` dosyaları paketlenir ama **SPIR-V olarak önceden derlenmez**; çalışma zamanında glslang derler.
  Cook yalnız derlenip derlenmediğini doğrular (`modules/assets/src/cook.cpp:284-286`). Başlangıç maliyetini azaltan
  makine başına SPIR-V disk önbelleği `%LOCALAPPDATA%/AstralEngine/shader-cache/<AppDesc::title>` benzeri bir klasörde tutulur;
  `ASTRAL_SHADER_CACHE=<klasör>` taşır, `off` veya `0` kapatır (`modules/app/src/shader_cache_location.cpp:47-52`).
- Dağıtılan oyunda `save_scene` ve `astral.save` yazma yolları farklıdır (bkz. [game-architecture.md](game-architecture.md#8-kaydetme-ve-yükleme)).

## 8. Denetimler: `ctest -R "features|script_types"`

- `astral_check_features(<ad> SOURCES src/features MANIFESTS assets/features)` → test `<ad>_features_check`
  (`cmake/AstralGame.cmake:63-77`). `astral-features check` satır satır tarar (`modules/app/src/feature_check.cpp:44-119`):
  1. `/detail/` veya `/src/` içeren iç motor başlığı `#include`'u (`modules/app/src/feature_check.cpp:96`).
  2. Başka özelliğin başlığını `depends_on` olmadan ya da `exports.headers`'ta olmayan bir başlığı almak (`modules/app/src/feature_check.cpp:100-109`).
  3. Değişebilir `static` değişken (`modules/app/src/feature_check.cpp:77`); gerekçeli istisna için satıra `// astral-allow-global: <neden>` yaz.
  - Regex tabanlıdır: `static` olmayan global, `thread_local`, makroyla gizlenmiş tanımlar yakalanmaz; Luau dosyalarına bakmaz.
- `astral_check_scripts(<ad> GAME <hedef> SCRIPTS assets/features)` → test `<ad>_script_types` (`cmake/AstralGame.cmake:85-102`).
  Oyun `--script-api-only` ile `astral.d.luau` yazar, `astral-luau-check` her `.luau`'yu buna karşı tip denetler
  (`cmake/CheckScripts.cmake:1-15`). `astral-luau-check` yoksa test hiç eklenmez (`cmake/AstralGame.cmake:93`). Denetleyicinin
  varsayılan modu `Nonstrict`'tir (`tools/astral_luau_check/main.cpp:46`); dosya başına `--!strict` yazmayan script
  daha gevşek denetlenir (Luau'nun dosya başı yönergesini onurlandırması Luau davranışıdır, bu repoda ayrıca sınanmadı).

## Bilinen sınırlar

- **Özellik kurulumu kısmi kalabilir:** `install` ilk hatada durur; önceki özellikler kayıtlı kalır, oyun `ctx.quit()` demeli
  (`modules/app/src/feature.cpp:154-168`).
- **Manifest tek yönlü denetlenir:** listelenip kaydedilmeyen component fark edilmez (`modules/app/src/feature.cpp:58-64`).
- **`astral-features check` regex tabanlı:** yalnız dört kural, yalnız C++ kaynaklar; Luau tarafı sınırsız.
- **Luau bütçesi sabit 100 ms ve ayarlanamıyor** (`modules/app/src/services/script_bridge.cpp:113`); yoğun test makinelerinde kırılganlık kaynağı.
- **`App` tek kullanımlık:** `run`/`run_frames` ikinci kez çağrılamaz (`modules/app/src/services/frame_loop.cpp:44-48`); her test yeni `App` kurar.
- **`input_script` yalnız klavye:** fare ve gamepad betiklenemez (`modules/app/include/astral/app/app.hpp:207-213`).
- **`AppDesc` 25 üyede, bütçe sınırında** (`modules/app/include/astral/app/app.hpp:215-258`; 25 üye sınırı ADR-0030; `Context` için ayrı istisna `tools/size_budget.json:1-8`); yeni ayar eklemek önce yeniden gruplamayı gerektirir.
- **Shader'lar paketlenirken derlenmiyor:** ilk çalıştırmada derleme + disk önbelleği (`modules/assets/src/cook.cpp:284`); soğuk başlangıç Debug'da onlarca saniye
  sürebilir (ana ajanın ölçümü: the_architect Debug soğuk 11,2 sn, Release soğuk 2,5 sn).
- **Doğrulanmadı:** `find_package(Astral)` ile dış kullanım akışı bu oturumda çalıştırılmadı; yalnız CI tanımı okundu.
