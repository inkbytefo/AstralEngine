# Yol Haritası

Durum: ✅ bitti · 🚧 sürüyor · 🔜 sıradaki · ⬜ planlı

| Faz | Kapsam | Durum |
|---|---|---|
| M0 | İskelet, `Astral::Core` (log, handle, clock, jobs), kurulum, CI | ✅ |
| M1 | `Astral::Platform` (SDL3 pencere/input), `Astral::Gpu` (Vulkan RHI) | 🔜 |
| M2 | `Astral::Render`: render graph, kamera, temel mesh çizimi, SDF pass | ⬜ |
| M3 | `Astral::World` (kendi archetype ECS'imiz, transform hiyerarşisi), `Astral::Physics` (Jolt) | ⬜ |
| M4 | `Astral::Assets`: glTF, PBR materyal, async yükleme, hot-reload | ⬜ |
| M5 | İlk oyun dikey dilimi (kılıç arenası prototipi) | ⬜ |

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

## M2 — Render 

**Bitiş kriterleri:** render graph pass ekleme/çıkarma mevcut pass'lere dokunmadan
yapılır; `hello_cube` ve SDF Playground'dan taşınan SDF pass çalışır.

## M3 — Dünya ve Fizik

**Bitiş kriterleri:** `physics_demo` (düşen kutular, raycast) 1080p'de 144+ FPS;
fizik modülü renderer olmadan test edilir.

## M4 — Asset'ler

**Bitiş kriterleri:** glTF sahnesi PBR ile yüklenir; asset'ler handle ile tutulur,
diskte değişince yeniden yüklenir.
