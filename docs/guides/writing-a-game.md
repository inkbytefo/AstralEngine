# Oyun nasıl yazılır

AstralEngine'de oyun üç parçadan oluşur: **component** (veri), **system** (mantık), **Game** (kurulum).
Döngüyü, pencereyi, GPU'yu ve render'ı `App` yönetir. Tam örnek: `examples/spinning_cubes/main.cpp`.

## 1. main

```cpp
#include <astral/astral.hpp>
using namespace astral;

int main() {
    App app({.title = "Sword Arena", .width = 1920, .height = 1080});
    return app.run<ArenaGame>();
}
```

## 2. Component'ler: sadece veri

```cpp
struct Health   { int value = 100; };
struct Player   { float speed = 6.0f; };
struct Enemy    {};                       // boş "etiket" component'i de olur
```

Kurallar: düz struct, taşınabilir (move). İçinde `std::string`, `std::vector`, `std::unique_ptr` olabilir.

## 3. System'ler: mantık

```cpp
class PlayerMovement final : public System {
public:
    void update(Context& ctx) override {
        const Vec2 move = ctx.input.wasd();
        ctx.world.each<Transform, const Player>([&](Transform& t, const Player& p) {
            t.position += Vec3{move.x, 0, -move.y} * p.speed * ctx.time.delta;
        });
    }
};
```

- `each<A, B>(fn)` A ve B'ye sahip **her** entity için çalışır. Sadece okuyacaksan `const` yaz.
- Ağır işler için `parallel_each<...>(ctx.jobs, fn)`: tüm çekirdeklerde çalışır; fn sadece kendi
  entity'sine dokunmalı.
- `each` içinde entity oluşturma/silme/component ekleme **yasak** → `ctx.world.commands()` kullan:

```cpp
ctx.world.each<const Health>([&](Entity e, const Health& h) {
    if (h.value <= 0) ctx.world.commands().destroy(e);
});
```

## 4. Game: sahneyi kur, system'leri kaydet

```cpp
class ArenaGame final : public Game {
public:
    void on_start(Context& ctx) override {
        ctx.world.create(Transform{.position = {0, 0, 0}},
                         MeshRenderer{ctx.meshes.cube, {0.8f, 0.2f, 0.2f, 1}},
                         Player{}, Health{});
        ctx.world.create(Transform{.position = {0, 3, 8}}, Camera{});

        ctx.add_system<PlayerMovement>(Phase::Update);
    }
};
```

## Kare sırası

`input → PreUpdate → Game::on_update → Update → PostUpdate → transform hiyerarşisi → render`
Her aşamadan sonra `commands()` uygulanır.

## Context'te neler var

| Alan | Ne |
|---|---|
| `world` | ECS (`create`, `each`, `get`, `add`, `commands`, `set_resource`…) |
| `input` | `down/pressed/released(Key)`, fare, `wasd()` |
| `time` | `delta`, `elapsed`, `frame` |
| `jobs` | `parallel_each` için iş havuzu |
| `log` | `ctx.log.info("skor: {}", skor)` |
| `meshes` | hazır `cube`, `sphere`, `plane` |
| `light` | yönlü ışık (yön, renk, ambient) |
| `renderer`, `window`, `device` | alt katman (headless'ta `nullptr`) |
| `quit()` | oyundan çık |

## Hiyerarşi

```cpp
Entity body  = ctx.world.create(Transform{}, Player{});
Entity sword = ctx.world.create(Transform{.position = {0.5f, 1, 0}}, Parent{body},
                                MeshRenderer{ctx.meshes.cube});
```
`sword` artık `body` ile birlikte hareket eder; dünya matrisi `WorldTransform`'da.

## Test

Mantığı pencere açmadan test et:

```cpp
App app({.headless = true, .fixed_timestep = 1.0f / 60});
MyGame game;
app.run_frames(game, 120);           // 2 saniye
CHECK(app.world().count<Enemy>() == 0);
```
