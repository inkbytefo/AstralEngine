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
| Şekiller | kutu, küre, kapsül, silindir, **convex hull** (`convex_hull(points)`), **mesh** (`mesh(vertices, indices)`, yalnız statik/kinematik), **heightfield** (`height_field(heights, samples, cell_size)`, gövdeye ortalı, yalnız statik/kinematik), **compound** (`compound({{konum, dönüş, şekil}...})`) |
| Hareket | `Static` (zemin/duvar), `Kinematic` (kodla hareket, iter), `Dynamic` (simüle) |
| Body | kütle (0 = hacimden), sürtünme, sekme, sönümleme, yerçekimi çarpanı, sensör, CCD (`continuous`) |
| Kontrol | `set_transform`, `move_kinematic`, `set_linear_velocity`, `set_angular_velocity`, `add_impulse`, `add_force`, `add_force_at`, `add_torque`, `set_gravity`/`gravity()` |
| Katmanlar | 16 çarpışma katmanı (`BodyDesc::layer`), simetrik matris (`set_layer_collision(a, b, false)`; varsayılan hepsi çarpışır); iki statik gövde hiç test edilmez |
| Sorgu | `raycast`, **`shape_cast`** (süpürülen şekil: küre, kutu...), **`overlap`** (şekille temas eden gövdeler); hepsi `QueryFilter{.layers = layer_bit(a) \| layer_bit(b), .ignore, .include_sensors}` alır (sensörler varsayılan görünmez) |
| Yüzey | `BodyDesc::surface` (senin tablonun kimliği): `RayHit`, `ShapeHit` ve temaslar `surface` taşır → ses/efekt eşlemesi |
| Eklemler | `create_joint`: Fixed, Point (top eklem), Hinge (menteşe, limit/sürtünme), Slider, Distance (çubuk/halat/yay: `spring_frequency`, `spring_damping`); dünyaya ya da iki gövdeye; eklemli gövdeler birbiriyle çarpışmaz (`collide_connected` ile açılır); gövde silinince eklemleri de gider |
| Alanlar | `fields.hpp`: `FieldWorld` — rüzgâr (küresel + kutu hacimleri, gust, türbülans, kenar sönümü) ve su hacimleri (yüzey, dalga, akış, yoğunluk); konum ve zamanın saf işlevi; aynı hash/gürültü/formül `fields.glsl`'de (GPU eşitlik testi) |
| Olaylar | `contacts()`: son adımdaki Begin/End temasları; `ContactEvent::sensor` = tetik girişi/çıkışı, `surface_a/b` |
| Karakter | kapsül karakter: zeminde durur, duvarda kayar, eğim sınırı, zıplama, dinamik body'leri iter; iç body sayesinde raycast ve sensörler onu görür, karakterler birbirinin içinden geçmez |
| Hatalı girdi | sıfır/negatif/NaN boyutlar reddedilir (geçersiz handle + log), adım taşmaları loglanır |

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
- Dinamik body'ler `Transform`'u yazar; kinematik body'ler `Transform`'u takip eder (hedef, karedeki
  adımlara bölünür: düşük FPS'te sarsıntı yok).
- `ctx.contacts()`: FixedUpdate içinde bir önceki adımın, diğer fazlarda bu karenin tüm temasları.
- Bir component'i kopyalamak (şablondan spawn) yeni entity'ye kendi body'sini verir.
- Karakterin konumu **ayak**; görsel mesh'i çocuk entity olarak yukarı kaydır (`examples/physics_demo`).

## Alanlar ve kuvvetler (M12)

Karar: [ADR-0029](../decisions/0029-physics-fields-and-joints.md). `WindZone` ve `WaterBody` component'leri (App) her sabit adımın başında `ctx.fields`'e
(`FieldWorld`) yazılır; kuvvetleri üç component uygular (`RigidBody` gerekir):

| Component | Kuvvet |
|---|---|
| `AirDrag{area, drag}` | `F = ½ ρ Cd A \|v_rel\| v_rel`, `v_rel = rüzgâr − hız` (suya batmışsa azalır) |
| `Buoyant{volume, height, area, drag, angular_drag}` | Arşimet: `ρ_su g V f` (f = suya batan oran: gövdenin altındaki noktada su derinliği / `height`), su sürüklemesi (akıma göre) ve dönüş sönümü; `volume` 0 ise `kütle / 500` (yarı batarak yüzer) |
| `Airfoil{area, lift_slope, stall_angle, forward, up}` | Kanat: `½ ρ v² A · slope · α` akışa dik, `α` hücum açısı, `stall_angle` ötesinde söner |

Sensör (`BodyDesc::sensor`) giriş/çıkışları ve çarpışmalar World olayı olur: `TriggerEntered{trigger, other}`, `TriggerExited`, `Contact{kind, a, b, point, normal, surface_a, surface_b}`;
`Joint{type, a, b, anchor, axis, ...}` component'i iki entity'nin `RigidBody`'sini bağlar. `AppDesc::physics_interpolation`: dinamik gövdeler son iki simüle poz arasında çizilir
(`physics_rate` kare hızından düşükse akıcı hareket; Transform simülasyonun bir adım kadar gerisinde). `ctx.debug_show.fields`: rüzgâr ve su kutuları; `debug_show.physics` yeni şekilleri de çizer.

## Bilinen sınırlar

- `RigidBody` yalnızca kök entity'lerde (Parent'sız); `desc` yalnızca oluşturulurken okunur; statik
  body'nin `Transform`'u sonradan değişirse body taşınmaz (`set_transform` kullan).
- Karakterler birbirini iter (kütleye göre değil, eşit).
- Mesh ve heightfield dinamik olamaz ve sorgularda süpürülemez (convex hull / compound kullan). Mesh/heightfield kinematik gövdede dönüş maliyetlidir.
- Eklemler: 6 serbestlik dereceli (SixDOF), motorlu eklemler, halat/zincir yardımcısı, kırılan eklem yok. Katman adları yok (sayılarla; oyun sabitleri tanımlar).
- Su: kutu hacmi + dalga formülü; gerçek dalga/ıslaklık/akış simülasyonu M16/M18. Yüzdürme kuvveti gövdenin tek noktasında; eğilen gövdelerde yaklaşık.
- Alan GLSL'i hazır ve CPU ile birebir doğrulanıyor, ama henüz bir shader tüketmiyor (bitki, partikül, bulut M14/M17).
- Jolt kendi iş parçacığı havuzunu kullanır (motorun JobSystem'iyle paylaşılmıyor; `Jolt::JobSystem` bağlama ertelendi, ADR-0029).
- Jolt'un tip kaydı süreç genelinde (ilk `PhysicsWorld` kurar, sonuncusu kaldırır).
- Görsel interpolasyon isteğe bağlı (`AppDesc::physics_interpolation`, varsayılan kapalı): kapalıyken 60 Hz'den yüksek ekranlarda hareket adım adım görünebilir.
