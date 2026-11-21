# Astral::App

Katman 3. Oyunların yazıldığı katman ([ADR-0006](../decisions/0006-app-layer-for-games.md)).
Rehber: [writing-a-game.md](../guides/writing-a-game.md).

`App` şunları kurar ve sahiplenir: Logger (konsol), JobSystem, World, Input, Clock, Window, Device,
Swapchain, Renderer, hazır mesh'ler. `Game` + `System`'ler bunlara `Context` üzerinden erişir.

## AppDesc

| Alan | Varsayılan | |
|---|---|---|
| `title`, `width`, `height` | "Astral", 1280×720 | pencere |
| `vsync` | true | |
| `headless` | false | pencere/GPU yok — sunucu ve testler |
| `validation` | false | Vulkan validation (yavaş, hata ayıklama) |
| `fixed_timestep` | 0 | >0 ise her kare tam bu kadar ilerler (deterministik testler) |
| `worker_threads` | 0 | 0 = çekirdek sayısı − 1 |

## Kare

`poll → PreUpdate → on_update → Update → PostUpdate → update_transforms → render (MeshRenderer + Camera)`

Kamera: ilk aktif `Camera` component'li entity'nin `WorldTransform`'u (−Z'ye bakar); yoksa varsayılan.

## Bilinen sınırlar

- Pencere yeniden boyutlanınca swapchain kendini yeniler; olay callback'i yok.
- Sabit zaman adımlı fizik döngüsü (fixed update) M4'te fizikle gelecek.
