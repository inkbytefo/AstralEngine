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
| `physics_rate` | 60 | fizik adımı / saniye |
| `max_physics_steps` | 4 | kare başına en fazla adım |

## Kare

`poll → PreUpdate → on_update → Update → fizik (FixedUpdate system'leri + adım, 0..4 kez) → PostUpdate → update_transforms → render`

Fizik component'leri: `RigidBody`, `CharacterController` ([physics.md](physics.md)).

Kamera: ilk aktif `Camera` component'li entity'nin `WorldTransform`'u (−Z'ye bakar); yoksa varsayılan.

Render component'leri: `MeshRenderer` (mesh + PBR materyal), `Sdf` (SDF şekli + sıra; konum/dönüş
entity'den), `PointLight` (konum entity'den). Güneş ve ortam: `ctx.sun`, `ctx.environment`.

## Yaşam süresi

`run()`/`run_frames()` App başına bir kez. Kapanışta önce system'ler yok edilir (destructor'larında
renderer/GPU kaynağı bırakabilirler), sonra renderer, swapchain, device ve pencere. Küçültülmüş
pencerede döngü işlemciyi yormaz.

## Bilinen sınırlar

- Pencere yeniden boyutlanınca swapchain kendini yeniler; olay callback'i yok.
