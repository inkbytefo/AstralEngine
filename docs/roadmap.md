# Yol Haritası

Durum: ✅ bitti · 🚧 sürüyor · 🔜 sıradaki · ⬜ planlı

| Faz | Kapsam | Durum |
|---|---|---|
| M0 | İskelet, `Astral::Core` (log, handle, clock, jobs), kurulum, CI | ✅ |
| M1 | `Astral::Platform` (SDL3 pencere/input), `Astral::Gpu` (Vulkan RHI), core math | 🚧 |
| M2 | `Astral::World` (kendi ECS'imiz), `Astral::App` (Game + System), basit mesh render | ⬜ |
| M3 | `Astral::Render`: render graph, deferred/hibrit mesh + SDF (eski v2'den taşıma) | ⬜ |
| M4 | `Astral::Physics` (Jolt) | ⬜ |
| M5 | `Astral::Assets`: glTF, PBR materyal, async yükleme, hot-reload | ⬜ |
| M6 | İlk oyun dikey dilimi (kılıç arenası prototipi) | ⬜ |

Oyun nasıl yazılır: [ADR-0006](decisions/0006-app-layer-for-games.md).

## M0 — İskelet ve Core ✅

**Sonuç:** Modüler CMake sistemi (`astral_add_module`), `Astral::Core` modülü, 25 test,
ThreadSanitizer temiz, `find_package(Astral COMPONENTS Core)` ile kullanılabilir,
GCC/Clang/MSVC + TSan CI. Bağımsız inceleme bir kilitlenme hatası buldu; düzeltildi.
Ayrıntı: [modules/core.md](modules/core.md).

**Devreden borç:** [core.md → Bilinen sınırlar](modules/core.md#bilinen-sınırlar).

## M1 — Platform ve GPU 🔜

**Bitiş kriterleri:** `examples/hello_triangle` pencerede üçgen çizer; GPU modülü
pencere olmadan (headless/offscreen) da çalışır ve testleri CI'da yazılımsal Vulkan
(lavapipe) ile koşar; public header'larda Vulkan/SDL tipi yok.

## M2 — Dünya ve App

**Bitiş kriterleri:** `examples/spinning_cubes`: oyun kodu yalnızca `Game` + component + system
yazar; ECS sorguları JobSystem ile paralel; system'ler headless test edilir.

## M3 — Render

**Bitiş kriterleri:** render graph pass ekleme/çıkarma mevcut pass'lere dokunmadan yapılır;
eski v2'deki deferred + SDF teknikleri bu mimariye temiz yeniden yazılmış olur.

## M4 — Fizik

**Bitiş kriterleri:** `physics_demo` (düşen kutular, raycast); fizik renderer olmadan test edilir.

## M5 — Asset'ler

**Bitiş kriterleri:** glTF sahnesi PBR ile yüklenir; asset'ler handle ile tutulur, diskte değişince
yeniden yüklenir.
