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

## Görünüm: materyal, SDF, ışık

```cpp
// PBR materyal
ctx.world.create(Transform{}, MeshRenderer{ctx.meshes.sphere, {0.9f, 0.7f, 0.3f, 1}, /*roughness*/ 0.2f, /*metallic*/ 1.0f});

// SDF: matematiksel şekiller, birbirine eriyerek birleşir (order küçük olan önce)
ctx.world.create(Transform{.position = {0, 1, 0}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion,
                               .size = {0.8f, 0, 0, 0}, .blend = 0.5f, .color = {1, 0.3f, 0.2f, 1}}});

// Nokta ışık (entity'nin konumunda)
ctx.world.create(Transform{.position = {2, 3, 0}}, PointLight{.color = {1, 0.5f, 0.2f}, .intensity = 20, .radius = 8});

// Güneş ve ortam
ctx.sun = {.direction = {-0.5f, -1, -0.3f}, .intensity = 2.0f};
ctx.environment.ambient = 0.5f;
```

Tam örnek: `examples/sdf_scene/main.cpp`.

## Fizik

```cpp
// Zemin (statik) ve düşen bir top
ctx.world.create(Transform{.position = {0, -0.5f, 0}}, MeshRenderer{ctx.meshes.cube},
                 RigidBody{.desc = {.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static}});
ctx.world.create(Transform{.position = {0, 5, 0}}, MeshRenderer{ctx.meshes.sphere},
                 RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .restitution = 0.4f}});

// Oyuncu: her karede hareketi yaz, App sabit adımda yürütür
ctx.world.each<CharacterController>([&](CharacterController& c) {
    const Vec2 in = ctx.input.wasd();
    c.move = Vec3{in.x, 0, -in.y} * 6.0f;
    c.jump = ctx.input.pressed(Key::Space);
});

// Nişan alma
if (auto hit = ctx.physics->raycast(origin, forward, 100.0f)) {
    Entity target = entity_of(*hit);
}
```

Tam örnek: `examples/physics_demo/main.cpp`.

## Kare sırası

`input → PreUpdate → Game::on_update → Update → fizik (FixedUpdate + adım, sabit 60 Hz) → PostUpdate → transform hiyerarşisi → render`
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
| `sun`, `environment` | güneş (yön, renk, gölge) ve gök/yer ambient, pozlama |
| `renderer`, `window`, `device` | alt katman (headless'ta `nullptr`) |
| `physics`, `contacts()` | fizik dünyası (raycast, impulse…) ve bu karenin temasları |
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
