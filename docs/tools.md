# Araçlar, CMake yardımcıları, test ortamı ve SDK kurulumu

> **Olgunluk:** Kullanılabilir (araçların çoğu küçük ve testli; denetimlerin bazı boşlukları aşağıda) ·
> **Bağımlılıklar:** `astral-cook` → Astral::Assets, Script; `astral-features` → Astral::App; `astral-luau-check` → Luau.Analysis (`tools/CMakeLists.txt:4`, `tools/CMakeLists.txt:13`, `tools/CMakeLists.txt:37`); denetim betikleri Python 3 ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

Bu belge bir modül değil, deponun komut satırı araçlarını ve bunları çalıştıran CMake/CTest bağlantısını anlatır. Her araç için: ne yapar, nasıl çağrılır, hangi `ctest` testi çalıştırır.
Python araçları için ortamda `python` (3.x) gerekir; CMake `find_package(Python3 COMPONENTS Interpreter)` bulamazsa Python tabanlı denetimler **sessizce kaydedilmez**
(`../CMakeLists.txt:72-100`: yalnızca bir `STATUS` iletisi yazılır).

## Özet tablo

| Araç | Tür | Ne yapar | CTest testi |
|---|---|---|---|
| `astral-cook` | C++ yürütülebilir | Kaynak asset klasörünü pişirir, `.apak` paketler | `astral_cook_cli`, `astral_cook_cli_usage` (`tools/CMakeLists.txt:47-52`); oyunlarda `<oyun>_package` hedefi |
| `astral-features check` | C++ yürütülebilir | Oyunun `FeatureSet` paketlerinin sınırlarını denetler (ADR-0023) | oyun başına `<oyun>_features_check` (`astral_check_features`) |
| `astral-luau-check` | C++ yürütülebilir | Luau betiklerini üretilmiş `astral.d.luau`'ya karşı tip denetler | oyun başına `<oyun>_script_types` (`astral_check_scripts`) |
| `tools/check_sizes.py` | Python | Dosya (600), işlev (80), yapı üyesi (25) boyut bütçesi | `code_size_check`, `check_sizes_self_test` |
| `tools/check_layers.py` | Python | Modül bağımlılık katmanlarını izin listesine karşı denetler | `layer_check`, `layer_check_self_test` |
| `tools/check_editor_isolation.py` | Python | Çalışma zamanı modüllerinin editöre bağlanmadığını denetler | `editor_isolation_check` |
| `tools/sync_features.py` | Python | `features/` ortak paketlerini oyunlardaki kopyalarla eşitler/denetler | `shared_features_in_sync` |
| `tools/check_docs.py` | Python | Dokümandaki bağlantı ve `yol:satır` başvurularını denetler | `docs_check`, `docs_check_self_test` |
| `tools/make_test_model.py`, `tools/make_test_world.py` | Python | Elle test verisi üretir (crate.glb; 4×4 km `.world`) | test değil; derleme hiçbirini çalıştırmaz |
| `cmake/CheckPublicHeaders.cmake` | CMake betiği | Public başlıklarda üçüncü parti adı arar | `astral_public_headers_clean` |
| `cmake/CheckJobPools.cmake` | CMake betiği | Ninja `heavy_compile` havuzunu denetler | `heavy_compile_pool_check` |
| `cmake/CheckTestEnvironment.cmake` | CMake betiği | Her testte `ASTRAL_SHADER_CACHE` ayarlı mı | `test_environment_check` |
| `cmake/CheckScripts.cmake` | CMake betiği | `astral_check_scripts` testinin gövdesi | `<oyun>_script_types` |

Kök `CMakeLists.txt` testleri `../CMakeLists.txt:70-112` aralığında kaydeder; `ASTRAL_BUILD_TESTS` kapalıysa hiçbiri yoktur.

## `astral-cook`

- **Ne yapar:** Kaynak klasörü artımlı olarak pişirir: `.gltf/.glb` → pişmiş model (içerdiği buffer/görüntü içine gömülür), `.world` + `<ad>.entities/` → dizin + `<ad>.cells/*.cell`, `.scene/.prefab/.json` → ikili JSON,
  `.luau` → bayt kodu (söz dizimi hatası `dosya:satır: ileti` ile bildirilir ve derlemeyi düşürür), diğerleri kopyalanır; `.` ile başlayan dosya/klasörler atlanır. Çıktı klasöründe içerik özeti önbelleği
  tutulur, yalnız değişenler yeniden pişer (`modules/assets/include/astral/assets/cook.hpp:70-74`; `tools/astral_cook/main.cpp:88-92`). Dokular BC7 (renk/veri) ya da BC5 (normal) olur
  (`modules/assets/include/astral/assets/cook.hpp:25-30`).
- **Kullanım** (`tools/astral_cook/main.cpp:1-34`):
  ```
  astral-cook <kaynak-klasör> -o <pişmiş-klasör> [--pack oyun.apak] [--uncompressed] [--quality 0..4] [--level 1..19] [--jobs N] [-q]
  ```
  `--pack` pişmiş dosyaları zstd sıkıştırmalı tek `.apak`'a yazar (varsayılan seviye 9, BC7 kalitesi 1). Argümansız çağrı kullanım iletisiyle 2 döner.
- **Nerede çalışır:** `astral_add_game` her oyun için `<oyun>_package` hedefini kurar (aşağıda). Duman testi: `astral_cook_cli` `examples/asset_viewer/assets/prefabs` klasörünü pişirip paketler; `astral_cook_cli_usage` argümansız çağrının
  hata döndüğünü (`WILL_FAIL`) doğrular (`tools/CMakeLists.txt:44-52`).
- **Sınırlar:** Sayısal seçenekler `atoi` ile okunur, doğrulanmaz (`--quality abc` sessizce 0 olur: `tools/astral_cook/main.cpp:60`, `tools/astral_cook/main.cpp:64`, `tools/astral_cook/main.cpp:68`). `tools/CMakeLists.txt:45` yorumu "sonra yine pişir (yapacak iş yok)" der ama test tek çağrıdır; artımlı
  davranış bu testte sınanmaz (assets modül testlerinde olabilir; burada doğrulanmadı).

## `astral-features`

- **Ne yapar:** `astral-features check <özellik-kaynak-klasörü> <özellik-manifest-klasörü>`: her özellik klasörü için `feature.json` olmasını, manifest adının klasör adıyla eşleşmesini, bağımlılık döngüsü/eksik bağımlılık olmamasını,
  başka özelliğin başlığının yalnızca `depends_on` + `exports.headers` ile içerilmesini, motorun `/detail/` ya da `/src/` iç başlıklarının kullanılmamasını ve değiştirilebilir global (`static` değişken) durum olmamasını denetler.
  İhlal başına bir satır yazar, 1 döner; sorun yoksa `features ok` (`modules/app/src/feature_check.cpp:44-119`, `tools/astral_features/main.cpp:8-18`).
- **Sınırlar:** Denetim **satır başına düzenli ifade**dir, C++ ayrıştırıcısı değildir: `static` yerel değişkenler de "global" sayılabilir ve `static` olmayan ad alanı değişkenleri yakalanmaz; istisna için satıra
  `// astral-allow-global: <neden>` yazılır (`modules/app/src/feature_check.cpp:75-77`, `modules/app/src/feature_check.cpp:112`). Yalnızca `include` satırlarına bakar; çalışma zamanı kayıtlarını (ör. `ctx.types.add<T>()` ile manifeste yazmadan kayıt) denetlemez.
- **CTest:** `astral_check_features(<ad> SOURCES <klasör> MANIFESTS <klasör>)` `<ad>_features_check` testini ekler (`cmake/AstralGame.cmake:63-78`; örnek: `templates/blank/CMakeLists.txt:44`).

## `astral-luau-check`

- **Ne yapar:** `astral-luau-check <astral.d.luau> <betik.luau>...`: Luau'nun kendi analizörüyle (stok `luau-analyze` tanım dosyası yükleyemediği için ayrı küçük sürücü) betikleri tip denetler; tanımlar yüklenemezse ya da tip hatası varsa 1 döner
  (`tools/astral_luau_check/main.cpp:1-4`, `tools/astral_luau_check/main.cpp:40-79`). Çözücü yeni (`SolverMode::New`), mod **Nonstrict** (`--!strict` yazan dosya sıkı denetlenir) (`tools/astral_luau_check/main.cpp:47-48`).
- **Derleme:** Luau Analysis kütüphanesini de derler (kaynak yorumuna göre yaklaşık bir dakika); `ASTRAL_LUAU_CHECK` seçeneği, testler ya da kurulum kuralları açıksa varsayılan AÇIK, kurulumda `bin/` altına `astral-luau-check` olarak yüklenir
  (`tools/CMakeLists.txt:19-42`).
- **CTest:** `astral_check_scripts(<ad> GAME <oyun-hedefi> SCRIPTS <klasör>)`, `<ad>_script_types` testini ekler; test `CheckScripts.cmake` ile önce oyunu `--script-api-only <klasör>` ile çalıştırıp (başsız, GPU'suz) `astral.d.luau` yazdırır,
  sonra klasördeki her `.luau` dosyasını denetler. Betik bulunamazsa test düşer (`cmake/AstralGame.cmake:85-104`, `cmake/CheckScripts.cmake:1-15`). Örnek: `templates/blank/CMakeLists.txt:45`; `templates/blank` ve 9 örnek kullanır (`astral_check_scripts` araması).
- **Ne yakaladığı (gözlem, 2026-11-26):** Yerel `build/debug` aracıyla, blank şablonunun ürettiği tanımlara karşı: yanlış faz sabiti, `entity:get("astral::Nope")` ve `entity:get(...)` sonucunda yanlış alan adı **yakalandı**;
  `astral.system{ update = function(ctx, _, transform, spin) ... }` içindeki bileşen parametrelerinin alan adı yanlışları, `query = {"Olmayan"}` dizgeleri ve `astral.on(...)` olay alanı yanlışı **yakalanmadı**
  (neden: üretilen tanım `update: ((ctx, entity, ...any) -> ())` verir, `modules/script/src/api.cpp:223`; bkz. [modules/script.md](modules/script.md)). `cmake/AstralGame.cmake:80-84` yorumundaki "yanlış bileşen, alan ya da olay adı ... derlemeyi düşürür" ifadesi bu yüzden
  yalnızca tipli yollar için doğrudur.
- **Araç yoksa uyarı:** `astral_check_scripts` aracı bulamazsa (`ASTRAL_LUAU_CHECK=OFF` ve SDK'da yok) `message(WARNING)` verir ve test kaydetmez (`cmake/AstralGame.cmake:90-98`).

## `tools/check_sizes.py` ve boyut bütçesi (ADR-0030)

- **Ne yapar:** `modules/*/src`, `modules/*/include`, `tools/`, `examples/*/src`, `templates/*/src` altındaki C++ dosyalarını tarar (testler, `_deps`, `build` hariç) ve dosya ≤ 600 satır, işlev gövdesi ≤ 80 satır, yapı/sınıf veri üyesi ≤ 25
  sınırlarını denetler; istisnalar `tools/size_budget.json`'da tutulur ve `max` ile tavanlanır, eşleşmeyen (bayat) istisnalar `--strict` ile hata olur. Sınırın %90'ına gelenleri `[NEAR]` listeler (hata değildir)
  (`tools/check_sizes.py:1-19`, `tools/check_sizes.py:31-34`).
- **Kullanım:** `python tools/check_sizes.py [--root <dizin>] [--budget <yol>] [--summary] [--strict] [--near <oran>]` (`tools/check_sizes.py:1176-1208`). `--strict` fazlalık/bayat istisnada 1 döner.
- **CTest:** `code_size_check` (`ASTRAL_SIZE_BUDGET_STRICT` AÇIKSA `--strict` ile: `../CMakeLists.txt:12`, `../CMakeLists.txt:76-81`) ve `check_sizes_self_test` (9 birim test: `tools/CMakeLists.txt:54-60`, `tools/tests/test_check_sizes.py`).
- **Güncel durum** (bu ağaçta çalıştırıldı): 0 aşım, 1 istisna (`Context` 31 üye, tavan 31: `tools/size_budget.json:3-7`), 23 `[NEAR]` (örn. `AppDesc` 25/25, `App::Impl` 25/25, `bindings.cpp` 590/600).
- **Sınırlar:** Çözümleyici gerçek bir C++ ayrıştırıcısı değil, elle yazılmış bir tokenizer + kapsam tarayıcısıdır (`tools/check_sizes.py:118-1040`); şablonlar/makrolar ve alışılmadık biçimlerde üye/işlev sayımı yanılabilir (kendi testi 9 durumu kapsar).
  Python dosyaları taranmaz: bu betik 1212 satırdır ve C++ için koyduğu 600 satır sınırını aşar.

## `tools/check_layers.py` ve `module_layers.json`

- **Ne yapar:** CMake'in yapılandırma zamanında yazdığı modül grafiğini (`build/<preset>/astral_module_graph.json`, `astral_export_module_graph`: `cmake/AstralModule.cmake:89-226`) `tools/module_layers.json` izin listesine karşı doğrular:
  her modül listede olmalı (ve tersi), her iç bağımlılık açıkça izinli olmalı, kenarlar katman sırasına uymalı (kaynak katmanı ≥ hedef), grafik döngüsüz olmalı, bilinmeyen `Astral::`/`astral_*` takma adları reddedilir; `Astral::ImGui` gibi sarmalayıcılar
  `third_party_aliases` ile yalnızca izinli modüllerden (şu an `app`, `editor`) bağlanabilir (`tools/check_layers.py:1-17`, `tools/module_layers.json:67-72`).
- **Kullanım:** `python tools/check_layers.py --manifest <graf.json> [--allowlist <yol>]` (`tools/check_layers.py:358-394`). Çıktı `layer_check: ok (13 modules, 32 directed edges)` (bu ağaçtaki yerel `build/debug` grafiğiyle denendi) ya da hata listesi.
- **CTest:** `layer_check` (`../CMakeLists.txt:82-85`) ve `layer_check_self_test` (11 birim test: `../CMakeLists.txt:86-88`, `tools/tests/test_check_layers.py`).
- **Katmanlar** (`tools/module_layers.json`): core 0; shader, gpu, platform 1; world, physics, audio, script, render, assets 2; app, editor_core 3; editor 4.

## `tools/check_editor_isolation.py`

`--manifest <graf.json>`: editör olmayan hiçbir modülün `editor_core`/`editor`'e bağlanmadığını ve `editor_core`'un **doğrudan** `gpu`, `platform`, `render`, `app`, `Astral::ImGui` bağımlılığı taşımadığını denetler
(`tools/check_editor_isolation.py:12-33`). CTest: `editor_isolation_check` (`../CMakeLists.txt:95-97`). **Sınır:** yalnızca doğrudan kenarlara bakar; `editor_core` → Assets → Render geçişli bağımlılığı bu denetimden geçer
(`modules/assets/CMakeLists.txt:53-56`). `tests/consumer` ayrıca `Astral::App`'in ve tüketici oyununun editör hedefine bağlanmadığını CMake'te doğrular (`tests/consumer/CMakeLists.txt:26-38`).

## `tools/sync_features.py`

`features/<ad>/src` → `<oyun>/src/features/<ad>/` ve `features/<ad>/assets` → `<oyun>/assets/features/<ad>/` kopyalar; bir oyun, `src/features/<ad>/` klasörünü oluşturarak özelliği "kullanır". `--check` kopyaları değiştirmez,
farkı listeler ve 1 döner (`tools/sync_features.py:1-6`, `tools/sync_features.py:32-57`). CTest: `shared_features_in_sync` (`../CMakeLists.txt:75`). Şu an ortak paket tek: `features/fly_camera` (`features/README.md:1-24`).
**Sınır:** Yalnızca ortak kaynakta bulunan dosyaları karşılaştırır; oyun kopyasındaki **fazladan** ya da silinmiş dosyalar fark sayılmaz ve silme kopyalanmaz (`tools/sync_features.py:45-54`).

## `tools/check_docs.py`

Git'te izlenen (`git ls-files *.md`) kök `README.md`, `AGENTS.md` ve `docs/` altındaki (`docs/archive/` hariç) Markdown dosyalarında: göreli bağlantıların var olan dosyaya gittiğini ve `` `yol:satır` `` / `` `yol:satır-satır` `` başvurularında dosyanın
var olduğunu ve en az o kadar satırı bulunduğunu denetler (`tools/check_docs.py:1-12`, `tools/check_docs.py:26-34`, `tools/check_docs.py:59-83`). Çit içindeki kod blokları başvuru sayılmaz. `yol` depo kökünden
(`modules/`, `tools/`, `cmake/`, `examples/`, `features/`, `templates/`, `tests/`, `docs/`, `bench/` önekleri) ya da Markdown dosyasına göreli olmalıdır; yalnızca dosya adıyla yazılmış başvuru (örn. `app.cpp`, satır 10) bulunamaz ve hata sayılır.
Kullanım: `python tools/check_docs.py --root .`. CTest: `docs_check`, `docs_check_self_test` (5 birim test) (`../CMakeLists.txt:89-94`, `tools/tests/test_check_docs.py`).
**Sınırlar:** (1) Yalnızca **var olma ve satır aralığını** denetler; satırın başvuruyla ilgili olup olmadığına bakmaz. (2) `git ls-files` kullandığı için henüz izlenmeyen (eklenmemiş) belgeler denetlenmez ve git olmayan bir kaynak
arşivinde `check=True` yüzünden hata ile çöker (`tools/check_docs.py:27`). (3) `.github/` gibi önek listesinde olmayan yollar yalnızca belgeye göreli yazılırsa (`../.github/...`) çözülür.

## `tools/make_test_model.py`, `tools/make_test_world.py`

- `make_test_model.py <çıktı.glb>`: prosedürel dokulu "crate.glb" üretir (numpy ve Pillow gerekir; üçüncü parti sanat yok) (`tools/make_test_model.py:1-10`). Elle kullanılır.
- `make_test_world.py <çıktı-dizini> [--size 4000] [--spacing 55] [--seed 42]`: sabit tohumlu 4×4 km `.world` + her varlık için `.ent` üretir; çıktı depoya değil derleme klasörüne gider (`tools/make_test_world.py:1-14`).
  Derleme bunu çalıştırmaz; `examples/world_showcase` haritası depoya eklenmiş dosyalardır (`examples/world_showcase/make_world.py` üretir).

## CMake yardımcıları

| Fonksiyon / dosya | Ne yapar | Kaynak |
|---|---|---|
| `astral_add_module(NAME ...)`, `astral_add_tests(MODULE ...)` | Statik kütüphane `astral_<ad>` + takma ad `Astral::<Ad>` (ilk harf büyür: `editor_core` → `Astral::Editor_core`); testler `astral_<ad>_tests` (doctest, zaman aşımı 120 sn) | `cmake/AstralModule.cmake:7-54` |
| `astral_export_module_graph` | Modül bağımlılık grafiğini JSON yazar (layer_check girdisi) | `cmake/AstralModule.cmake:94-226` |
| `astral_add_game(<ad> SOURCES ... ASSETS <klasör>)` | `Astral::App`'e bağlı yürütülebilir; `ASTRAL_GAME_ASSETS` (kaynak asset klasörü) ve `ASTRAL_GAME_PACK` (`<ad>.apak`) tanımlarını verir; `<ad>_package` hedefi `astral-cook` ile `.apak` üretir ve `build/packages/<ad>/` altında çalıştırılabilir klasör hazırlar; `ASTRAL_PACKAGE_GAMES=ON` ise `ALL` ile | `cmake/AstralGame.cmake:11-56` |
| `astral_check_features`, `astral_check_scripts` | Yukarıdaki `*_features_check`, `*_script_types` testleri | `cmake/AstralGame.cmake:63-104` |
| `astral_add_shaders(<hedef> SOURCES ...)` | `glslc` ile GLSL → SPIR-V, gömülü `astral::shaders::<ad>`; `glslc` yoksa yapılandırma `FATAL_ERROR` | `cmake/AstralShaders.cmake:1-30` |
| `astral_embed_file` | Dosyayı `astral::embedded::<sembol>` olarak ikiliye gömer (varsayılan font) | `cmake/AstralEmbed.cmake:1-19` |
| `astral_set_warnings` | MSVC `/W4 /permissive- /utf-8`, diğerleri `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`; `ASTRAL_WARNINGS_AS_ERRORS` ile hata | `cmake/AstralWarnings.cmake:1-15` |
| Ninja `heavy_compile` havuzu | En ağır çeviri birimleri (Jolt) çekirdek/2 işte derlenir (MSVC C1060 bellek hatasını önlemek için); `ASTRAL_HEAVY_COMPILE_JOBS` ile ayarlanır | `../CMakeLists.txt:14-29`, `cmake/CheckJobPools.cmake:1-23` |
| Ön ayarlar | `debug`, `release`, `tsan` (`-fsanitize=thread`), `asan` (`address,undefined`; `lsan.supp`/`ubsan.supp` ile) | `../CMakePresets.json:3-45`, `../CMakePresets.json:93-94` |

Önemli seçenekler: `ASTRAL_BUILD_TESTS`, `ASTRAL_BUILD_EXAMPLES`, `ASTRAL_INSTALL` (üst düzey projede varsayılan AÇIK), `ASTRAL_WARNINGS_AS_ERRORS` (varsayılan KAPALI; CI açar: `../.github/workflows/ci.yml:30`),
`ASTRAL_SIZE_BUDGET_STRICT`, `ASTRAL_LUAU_CHECK`, `ASTRAL_PACKAGE_GAMES` (`../CMakeLists.txt:8-12`).
**Sınırlar:** `astral_add_game` varlık listesini `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` ile çıkarır (`cmake/AstralGame.cmake:40`): her derlemede yeniden taranır, çok dosyalı klasörlerde yavaş olabilir (ölçülmedi); `astral_check_features/scripts` yalnızca
`enable_testing()` sonrası çağrılmalıdır (yorum: `cmake/AstralGame.cmake:61`).

## Test ortamı: `ASTRAL_SHADER_CACHE`

Çalışma zamanında App, derlenmiş SPIR-V'yi kullanıcı profilindeki bir önbellekte tutar (bkz. [modules/app.md](modules/app.md)). Testlerin kullanıcı profiline yazmaması için kök `CMakeLists.txt` **son satırlarda**, tüm alt klasörlerdeki her teste
`ENVIRONMENT_MODIFICATION ASTRAL_SHADER_CACHE=set:<derleme>/test-shader-cache` ekler (`../CMakeLists.txt:127-143`; derleme klasörü `CMAKE_BINARY_DIR`). Önbellek içerik adresli ve yazma atomik olduğu için paralel testler (`ctest -j`) aynı klasörü güvenle paylaşır
(`modules/shader/include/astral/shader/spirv_cache.hpp:7-9`, `modules/shader/include/astral/shader/spirv_cache.hpp:49-51`). `test_environment_check` (`cmake/CheckTestEnvironment.cmake:1-43`) `ctest --show-only=json-v1` ile **kayıtlı her testin** bu ortam değişkenini içerdiğini doğrular; eksik olan testleri adlarıyla listeleyip düşer.
**Sınır:** `set_property(TEST ... DIRECTORY ...)` CMake ≥ 3.28 ister; proje alt sınırı 3.24 olduğu halde (`../CMakeLists.txt:1`) eski CMake'te hem ortam atanması hem `test_environment_check` **sessizce atlanır** (`../CMakeLists.txt:107`, `../CMakeLists.txt:130`), yani testler
kullanıcı önbelleğine yazar. Ayrıca bu yalnızca ana depo derlemesindeki testler içindir; `tests/consumer` ve `templates/blank` bağımsız proje olarak derlendiğinde bu ayar uygulanmaz (CI'daki tüketici testleri başsız çalıştığından önbellek kullanmaz).

## SDK kurulumu ve tüketici testi

- **Kurulum:** `cmake --install` tüm modül kütüphanelerini, public başlıkları, `astral_imgui` + `imgui.h`/`imconfig.h`, GPU/fizik shader include klasörlerini (`share/astral/shaders`), araçları (`astral-cook`, `astral-features`,
  `astral-luau-check`) ve `AstralConfig.cmake` + `AstralConfigVersion.cmake` + `AstralShaders.cmake` + `AstralGame.cmake` + `CheckScripts.cmake` dosyalarını kurar (`cmake/AstralInstall.cmake:1-29`, `modules/app/CMakeLists.txt:15-16`,
  `modules/gpu/CMakeLists.txt:32`, `modules/physics/CMakeLists.txt:47`, `tools/CMakeLists.txt:7`, `tools/CMakeLists.txt:16`, `tools/CMakeLists.txt:40`). Sürüm uyumu `SameMinorVersion` (0.17.x) (`cmake/AstralInstall.cmake:21-22`); `AstralConfig.cmake` Vulkan ≥ 1.3, SDL3, Jolt, glslang ve Threads bulur
  (`cmake/AstralConfig.cmake.in:1-11`).
- **Tüketici testi:** `tests/consumer` bağımsız bir CMake projesidir: `find_package(Astral 0.17 CONFIG REQUIRED COMPONENTS Core Gpu Platform Physics World Render Assets App)`, `astral_add_shaders`, `astral_add_game` (+ `consumer_game_package`),
  `astral_check_scripts` (kurulu `Astral::astral_luau_check` yoksa yapılandırma hata verir) ve editör izolasyon denetimleri (`tests/consumer/CMakeLists.txt:1-38`). **CI'da yalnızca Linux işinde** koşar: kurulum → tüketici yapılandır/derle (`consumer_game_package` dahil) →
  `ctest` → üç yürütülebilir; ardından `templates/blank` kurulu motorla derlenip test edilir (`../.github/workflows/ci.yml:38-53`). Windows işleri (`../.github/workflows/ci.yml:55-91`) kurulumu ve tüketiciyi denemez.
- **Şablon:** `templates/blank` aynı yardımcıları (`astral_add_game`, `astral_check_features`, `astral_check_scripts`) kullanır; depo içinde `add_subdirectory` ile ya da tek başına `find_package(Astral 0.17 ...)` ile derlenir
  (`templates/blank/CMakeLists.txt:1-46`).
- **Sınır:** `find_package(Astral 0.17 ...)` sürümü hem `tests/consumer/CMakeLists.txt:5` hem `templates/blank/CMakeLists.txt:10` içinde sabittir; kök proje sürümü (`../CMakeLists.txt:2`, şu an 0.17.0) yükseldiğinde ikisi elle güncellenmelidir (`SameMinorVersion`).

## CI özeti (`.github/workflows/ci.yml`)

Linux gcc-13/clang-18 (Release, uyarılar hata, lavapipe + xvfb, `ctest`, boyut özeti, kurulum + tüketici + şablon), Windows MSVC Release ve Debug (Debug'da `-LE golden`; GPU testleri cihaz yoksa atlanır), Linux TSan
(core, world, physics, script testlerinin tamamı ve app testlerinin dört dosyası: `../.github/workflows/ci.yml:93-108`) ve clang include-hygiene (`continue-on-error`: `../.github/workflows/ci.yml:110-130`). Bu PC'de (Windows) tsan/asan koşturulmadı.

## Güçlü yanlar

- **Kurallar makineyle denetlenir:** boyut bütçesi, katman izin listesi, public başlıklarda üçüncü parti adı, editör izolasyonu, özellik sınırları, ortak özellik kopyaları, dokümandaki yol/satır başvuruları ve test ortamı ayrı CTest testleridir; Python denetimlerinin kendi birim testleri var (25 test).
- **Araçlar küçük ve tek amaçlı:** çıktıları satır başına "ne yanlış, ne yapılmalı" biçiminde, çıkış kodları açık.
- **Artımlı pişirme ve tek komutla paket:** `<oyun>_package` hedefi pişirme + `.apak` + çalıştırılabilir klasörü hazırlar.
- **SDK uçtan uca CI'da sınanır (Linux):** kurulu paketten oyun, betik tip denetimi ve şablon derlenir.
- **Testler kullanıcı profilini kirletmez** (CMake ≥ 3.28'de): paylaşılan, atomik SPIR-V önbelleği derleme klasöründe.

## Zayıf yanlar ve bilinen sınırlar

- **Denetimlerin bir kısmı sezgiseldir ve kolay atlanır:** `astral-features` satır regex'idir (`modules/app/src/feature_check.cpp:75-77`); `check_sizes.py` elle yazılmış tokenizer'dır; `check_editor_isolation.py` geçişli bağımlılığa bakmaz; `check_docs.py` yalnızca var olma/aralık denetler.
- **`astral-luau-check` beklenenden az yakalar** (yukarıdaki gözlem) ve kendi testi yoktur; araç yoksa `astral_check_scripts` uyarı verip hiçbir şey kaydetmez (`cmake/AstralGame.cmake:94-98`).
- **Python yoksa denetimler sessizce kaybolur** (`../CMakeLists.txt:99`); CMake < 3.28 ise test ortamı yalıtımı ve `test_environment_check` sessizce kaybolur.
- **SDK kurulumu Windows'ta CI'da denenmez** (`../.github/workflows/ci.yml:55-91`); sürüm numarası iki yerde elle sabit.
- **`astral-cook` sayısal argümanlarını doğrulamaz** (`tools/astral_cook/main.cpp:60-68`).
- **`sync_features.py --check` fazladan dosyayı görmez** (`tools/sync_features.py:45-54`).
- **`check_docs.py` izlenmeyen dosyaları atlar ve git gerektirir** (`tools/check_docs.py:27`).

## Testler

| Test | Ne sınar |
|---|---|
| `astral_cook_cli`, `astral_cook_cli_usage` | `astral-cook` küçük bir klasörü pişirip paketler; argümansız çağrı hata verir (`tools/CMakeLists.txt:47-52`) |
| `check_sizes_self_test` (9), `layer_check_self_test` (11), `docs_check_self_test` (5) | ilgili Python araçlarının birim testleri (`tools/tests/`) |
| `code_size_check`, `layer_check`, `editor_isolation_check`, `docs_check`, `shared_features_in_sync` | depoya uygulanan denetimler (`../CMakeLists.txt:75-97`) |
| `astral_public_headers_clean`, `heavy_compile_pool_check`, `test_environment_check` | CMake betikleri (`../CMakeLists.txt:101-111`) |
| `<oyun>_features_check`, `<oyun>_script_types` | oyun başına; `templates/blank` ve 9 örnek (ikisi için de) |

**Kapsanmayanlar:** `astral-features` ve `astral-luau-check` için ayrı birim test yok (yalnızca oyunlar üzerinden dolaylı); `astral-cook` komut satırı seçenekleri (`--quality`, `--level`, `--uncompressed`, `--jobs`); `astral_add_game` paketleme hedefi
yalnızca tüketici/şablon CI'ında ve `ASTRAL_PACKAGE_GAMES` ile denenir. Golden görüntü testleri (`tests/golden`: `golden` lavapipe, `golden-hw` donanım) ve `tests/integration` bu belgenin kapsamı dışındadır
(`tests/golden/CMakeLists.txt:1-3`, `tests/integration/CMakeLists.txt:1-2`).

## Performans

Ölçülmedi. (Kaynak yorumu: `astral-luau-check` derlemesi yaklaşık bir dakika, `tools/CMakeLists.txt:19-21`; ölçülmedi.)

## Kararlar

- [ADR-0016 Paketleme](decisions/0016-packaging.md)
- [ADR-0023 Modüler oyun kodu](decisions/0023-modular-game-code.md)
- [ADR-0027 Boş şablon ve oyun kabuğu](decisions/0027-blank-template-and-game-shell.md)
- [ADR-0028 Betik araçları ve paylaşılan özellikler](decisions/0028-script-tooling-and-shared-features.md)
- [ADR-0030 Refaktör kuralları ve boyut bütçesi](decisions/0030-refactor-rules-and-size-budget.md)
- [ADR-0035 Bağımsız editör çekirdeği](decisions/0035-standalone-ai-assisted-editor-core.md) (editör izolasyonu)
- [ADR-0047](decisions/0047-spirv-disk-cache.md), [ADR-0048](decisions/0048-startup-responsiveness-and-mips.md)
