# AstralEngine Dokümantasyonu

Bu klasör motorun **bugünkü** durumunu anlatır: ne var, nasıl çalışır, nerede güçlü, nerede eksik. Planlar yalnız [roadmap.md](roadmap.md)'dedir. Her iddia kaynak koda (`path:line`) bağlanır; `docs_check` testi kırık link ve referansları yakalar. Eski dokümanlar [archive/](archive/README.md) altındadır ve güncel değildir.

**Doğrulama tabanı:** master `a113b55` (2026-11-26), RTX 3060, MSVC 19.51, Windows 11.

## Nereden başlamalı

| Kimsin | Oku |
|---|---|
| Oyun yazacaksın | [guides/writing-a-game.md](guides/writing-a-game.md) → [guides/game-architecture.md](guides/game-architecture.md) → [../templates/blank/](../templates/blank/README.md) |
| Motoru geliştireceksin | [architecture.md](architecture.md) → [process.md](process.md) → ilgili [modül dokümanı](#modüller) → [decisions/](decisions/README.md) |
| Durumu ve eksikleri merak ediyorsun | [status.md](status.md) → [roadmap.md](roadmap.md) |
| Yapay zekâ ajanısın | [../AGENTS.md](../AGENTS.md) (oyun yazma kuralları), [../CLAUDE.md](../CLAUDE.md) (motor geliştirme kuralları), [process.md](process.md) |

## Genel belgeler

| Belge | İçerik |
|---|---|
| [status.md](status.md) | Modül modül olgunluk, güçlü ve zayıf yanlar, test durumu, bilinen başarısızlıklar, ölçümler |
| [roadmap.md](roadmap.md) | Tek proje yol haritası: tamamlananlar, sıradaki işler, performans backlog'u, motor fazları, Studio/GDX, Technology Labs |
| [architecture.md](architecture.md) | Katmanlar, modül bağımlılıkları, değişmez kurallar ve denetimleri |
| [process.md](process.md) | Çalışma akışı, "bitti" tanımı, doküman kuralları, Windows derleme notları |
| [tools.md](tools.md) | Komut satırı araçları ve denetim betikleri, CMake oyun yardımcıları |
| [decisions/](decisions/README.md) | Mimari karar kayıtları (ADR-0001…0051) |

## Modüller

| Katman | Modül |
|---|---|
| 0 | [core](modules/core.md) |
| 1 | [shader](modules/shader.md) · [gpu](modules/gpu.md) · [platform](modules/platform.md) |
| 2 | [world](modules/world.md) · [physics](modules/physics.md) · [audio](modules/audio.md) · [script](modules/script.md) · [render](modules/render.md) · [assets](modules/assets.md) |
| 3–4 | [app](modules/app.md) · [editor](modules/editor.md) (`editor_core` + `editor`) |

## Rehberler

| Rehber | Konu |
|---|---|
| [writing-a-game.md](guides/writing-a-game.md) | Şablondan oyuna: C++ sistemleri, Luau, FeatureSet, başsız testler, paketleme |
| [game-architecture.md](guides/game-architecture.md) | World/Scene/Prefab/Entity/Component, fazlar, olaylar, fizik komut sözleşmesi, save/load |
| [scenes-and-worlds.md](guides/scenes-and-worlds.md) | `.scene`, `.prefab`, `.world`, `.ent`, `.cell` biçimleri ve streaming |
| [shaders-and-materials.md](guides/shaders-and-materials.md) | `.material` ve `.ashader` ile hızlı başlangıç |
| [ashader-language.md](guides/ashader-language.md) | `.ashader` dilinin tam başvuru kılavuzu |
