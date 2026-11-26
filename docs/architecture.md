# Mimari

**Doğrulama:** master `a113b55` (2026-11-26) — katman tablosu `modules/*/CMakeLists.txt` ve `tools/module_layers.json` ile karşılaştırıldı.

## Temel fikir: kütüphane, framework değil

Motor bir kütüphane setidir; döngünün sahibi onu kullanan koddur ([ADR-0001](decisions/0001-library-not-framework.md)). Alt modüller (core, gpu, platform, world, render, physics…) tek başına kullanılabilir. Oyunlar için `Astral::App` katmanı standart bir kabuk sağlar: döngüye App sahip olur, oyun `Game` + component + system (C++) veya Luau script'leri yazar ([ADR-0006](decisions/0006-app-layer-for-games.md)). Kare sırası `modules/app/src/services/frame_loop.cpp:63` (`FrameLoop::run_frame`) içindedir; ayrıntı [modules/app.md](modules/app.md).

## Katmanlar

```
Katman 4  │ editor (masaüstü düzenleme uygulaması kütüphanesi, Astral::ImGui) — oyun ve sunucu linklemez
Katman 3  │ app (Game + System + Context + servisler; Astral::ImGui) · editor_core (GPU'suz belge/transaction/AI)
Katman 2  │ world (ECS) ← script (Luau) · physics (Jolt) · audio (miniaudio) · render · assets (→ world, render)
Katman 1  │ shader (.ashader, glslang, SPIR-V önbelleği) · gpu (Vulkan 1.3 RHI) ← platform (SDL3 pencere/girdi)
Katman 0  │ core (log, handle, saat, iş sistemi, matematik, JSON, reflection, profiler)
```

| Modül | Katman | Doğrudan bağımlılıklar | Doküman |
|---|---|---|---|
| `core` | 0 | — | [core](modules/core.md) |
| `shader` | 1 | core | [shader](modules/shader.md) |
| `gpu` | 1 | core | [gpu](modules/gpu.md) |
| `platform` | 1 | core, gpu | [platform](modules/platform.md) |
| `world` | 2 | core | [world](modules/world.md) |
| `physics` | 2 | core (Jolt private) | [physics](modules/physics.md) |
| `audio` | 2 | core (miniaudio private) | [audio](modules/audio.md) |
| `script` | 2 | core, world (Luau private) | [script](modules/script.md) |
| `render` | 2 | core, gpu, shader | [render](modules/render.md) |
| `assets` | 2 | core, world, render | [assets](modules/assets.md) |
| `app` | 3 | core, platform, gpu, world, script, render, assets, physics, audio (+ `Astral::ImGui`) | [app](modules/app.md) |
| `editor_core` | 3 | core, world, assets | [editor](modules/editor.md) |
| `editor` | 4 | core, world, assets, app, editor_core (+ `Astral::ImGui`) | [editor](modules/editor.md) |

**Denetim:** her modül `astral_add_module(...)` ile kaydedilir (`cmake/AstralModule.cmake:7`). Configure anında bağımlılık grafiği `astral_module_graph.json` olarak yazılır; ctest `layer_check` (`tools/check_layers.py`) bunu `tools/module_layers.json` izin listesiyle karşılaştırır: izinsiz kenar, üst katmana bağımlılık veya döngü testi kırar. `editor_isolation_check`, oyun ve sunucu hedeflerinin editörü linklemediğini doğrular.

**Bir bağımlılık eklemek:** döngü ve katman yönünü kontrol et → modülün `DEPS`/`PRIVATE_DEPS` alanını güncelle → aynı değişiklikte `tools/module_layers.json` ve bu tabloyu güncelle → yeni bir sınır açılıyorsa ADR yaz → `ctest -R layer_check`.

## Değişmez kurallar

| Kural | Denetim | Bugünkü durum |
|---|---|---|
| Değiştirilebilir global durum yok; bağımlılıklar constructor veya servis arayüzüyle verilir | İnceleme | İki bilinçli `thread_local` istisnası, ikisi de `src/` içinde: fizik komut üreticisi kimliği (`modules/app/src/services/physics_commands.cpp:13`, [ADR-0043](decisions/0043-deterministic-physics-command-producers.md)) ve world tick durumu (`modules/world/src/world.cpp:133`) |
| Public header'larda üçüncü parti tip yok (Vulkan, SDL, Jolt, Luau, miniaudio `src/` içinde) | ctest `astral_public_headers_clean` (`cmake/CheckPublicHeaders.cmake`) | Geçiyor |
| Kendi ECS'imiz ([ADR-0005](decisions/0005-own-ecs.md)) | — | `modules/world` |
| Kaynaklara `Handle<T>` ile erişim | İnceleme | `modules/core/include/astral/core/handle.hpp` |
| Boyut bütçesi: dosya ≤ 600 satır, fonksiyon ≤ 80 satır, yapı ≤ 25 veri üyesi ([ADR-0030](decisions/0030-refactor-rules-and-size-budget.md)) | ctest `code_size_check` (`tools/check_sizes.py`, `tools/size_budget.json`) | Geçiyor; `Context` için 31 üyelik istisna; `AppDesc` ve `App::Impl` 25/25 (sınırda) |
| Oyun kodu FeatureSet paketleridir; paketler birbirini çağırmaz, olay ve component ile konuşur ([ADR-0023](decisions/0023-modular-game-code.md)) | `astral-features check` (ctest `*_features_check`) | Örneklerde geçiyor |
| Luau script'leri kalıcı durumu component'lerde tutar ([ADR-0017](decisions/0017-gameplay-scripting.md), [ADR-0026](decisions/0026-script-host-and-features.md)) | İnceleme | — |
| Her modül tek başına derlenir ve test edilir; renderer'sız (headless) çalışma desteklenir | Modül test hedefleri | `AppDesc::headless` |
| Ortak `features/` ile oyunlardaki kopyaları aynıdır | ctest `shared_features_in_sync` (`tools/sync_features.py`) | Geçiyor |

## Tasarım ilkeleri (koddaki karşılıkları)

| İlke | Örnek |
|---|---|
| Küçük arayüzler | `ILogSink` (`modules/core/include/astral/core/log.hpp:27`), `ITimeSource` (`modules/core/include/astral/core/clock.hpp:9`), `PhysicsService`, `ScriptPhysics` |
| Yetenek sorgusu | `gpu::Device::supports(Feature)` (`modules/gpu/include/astral/gpu/device.hpp:49`), `PhysicsWorld::capabilities()` |
| Ödünç verilen servisler, global yok | `ShaderCompileServices { SpirvCache*, JobSystem* }` (`modules/shader/include/astral/shader/compile.hpp`) |
| Sıcak veri data-oriented | ECS sütunları, toplu sorgular (`raycast_batch`) |

`Window` (`modules/platform/include/astral/platform/window.hpp:36`) somut bir sınıftır; pencere için soyut arayüz yoktur.

## Klasör yapısı

```
modules/<ad>/include/astral/<ad>/   public API
modules/<ad>/src/                   private uygulama
modules/<ad>/tests/                 birim testleri (doctest)
features/                           ortak FeatureSet paketleri (oyunlara kopyalanır, tek kaynak)
templates/blank/                    boş oyun şablonu
examples/                           örnek oyunlar ve lab'ler (yenilenecek: roadmap §4.1)
tests/consumer/                     kurulu SDK'ya karşı find_package tüketici testi
tests/integration/                  modüller arası testler
tests/golden/                       görüntü testleri (ctest -L golden / golden-hw)
bench/                              astral-bench
tools/                              komut satırı araçları ve denetim betikleri (bkz. tools.md)
cmake/                              derleme yardımcıları (AstralModule, AstralGame, AstralInstall…)
docs/                               bu dokümanlar; docs/archive/ tarihçe
```

## İsimlendirme

- Namespace `astral::`; GPU tipleri `astral::gpu::`.
- Dosyalar `snake_case.hpp/.cpp`.
- CMake hedefi `astral_<ad>`, alias `Astral::<Ad>`.
- Kod ve yorumlar İngilizce; `docs/` Türkçe.
