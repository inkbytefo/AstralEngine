# Astral::Physics

Katman 2. Jolt Physics üzerine fizik; yalnızca Core'a bağlı (ECS'i bilmez, headless test edilir).
Public API'de Jolt tipi yok ([ADR-0010](../decisions/0010-jolt-physics.md)).

```cpp
astral::PhysicsWorld physics;
auto ground = physics.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static,
                                   .position = {0, -0.5f, 0}});
auto ball = physics.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 5, 0}, .restitution = 0.4f});
physics.step(1.0f / 60.0f);
if (auto hit = physics.raycast(origin, direction, 100.0f)) { /* hit->body, position, normal, distance */ }
```

## Neler var

| | |
|---|---|
| Şekiller | kutu, küre, kapsül, silindir (`ShapeDesc::box/sphere/capsule/cylinder`) |
| Hareket | `Static` (zemin/duvar), `Kinematic` (kodla hareket, iter), `Dynamic` (simüle) |
| Body | kütle (0 = hacimden), sürtünme, sekme, sönümleme, yerçekimi çarpanı, sensör, CCD (`continuous`) |
| Kontrol | `set_transform`, `move_kinematic`, `set_linear_velocity`, `add_impulse`, `add_force`, `set_gravity` |
| Sorgu | `raycast` (sensörleri atlar, bir body'yi hariç tutabilir) |
| Olaylar | `contacts()`: son adımdaki Begin/End temasları (sensör girişleri dahil) |
| Karakter | kapsül karakter: zeminde durur, duvarda kayar, eğim sınırı, zıplama, dinamik body'leri iter |

## App içinde

```cpp
ctx.world.create(Transform{.position = {0, 5, 0}}, MeshRenderer{ctx.meshes.sphere},
                 RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
ctx.world.create(Transform{}, CharacterController{});   // move/jump alanlarını system'den yaz
if (auto hit = ctx.physics->raycast(o, d, 100)) { Entity e = entity_of(*hit); }
for (const ContactEvent& c : ctx.contacts()) { ... }
```

- Body, component eklendiği ilk karede `Transform`'dan oluşturulur; entity silinince veya component
  kaldırılınca body de silinir.
- Fizik **sabit 60 Hz** adımla koşar (`AppDesc::physics_rate`), kare başına en fazla 4 adım; yavaş
  karelerde zaman atılır (spiral of death yok). `Phase::FixedUpdate` system'leri her adımdan önce,
  `ctx.time.fixed_delta` ile çalışır.
- Dinamik body'ler `Transform`'u yazar; kinematik body'ler `Transform`'u takip eder.
- Karakterin konumu **ayak**; görsel mesh'i çocuk entity olarak yukarı kaydır (`examples/physics_demo`).

## Bilinen sınırlar

- `RigidBody` yalnızca kök entity'lerde (Parent'sız); statik body'nin `Transform`'u sonradan değişirse
  body taşınmaz (`set_transform` kullan).
- Mesh/convex hull/heightfield çarpışma şekilleri, eklemler (constraint), shape cast/overlap sorguları,
  çarpışma katmanları/filtreleri yok.
- Jolt kendi iş parçacığı havuzunu kullanır (motorun JobSystem'iyle paylaşılmıyor).
- Jolt'un tip kaydı süreç genelinde (ilk `PhysicsWorld` kurar, sonuncusu kaldırır).
- Görsel interpolasyon yok: 60 Hz'den yüksek ekranlarda hareket adım adım görünebilir.
