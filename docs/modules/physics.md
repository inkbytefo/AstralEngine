# Astral::Physics

Katman 2. Jolt Physics **v5.7.0** üzerine fizik; yalnızca Core'a bağlı (ECS'i bilmez, headless test edilir).
Public API'de Jolt tipi yok ([ADR-0010](../decisions/0010-jolt-physics.md), [ADR-0038](../decisions/0038-physics-api-v2.md)).

```cpp
astral::PhysicsWorld physics;
physics.register_material(1, {.friction = 0.02f, .friction_combine = PhysicsCombineMode::Min});
auto cooked = astral::cook_physics_shape(ShapeDesc::box({0.5f, 0.5f, 0.5f}));
auto shape = physics.create_shape_from_cooked(cooked);
auto ground = physics.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static,
                                   .position = {0, -0.5f, 0}});
auto ball = physics.create_body({.position = {0, 5, 0}, .surface = 1, .cooked_shape = shape});
physics.step(1.0f / 60.0f);
if (auto hit = physics.raycast(origin, direction, 100.0f)) { /* hit->body, position, normal, distance */ }
```

## Neler var

| | |
|---|---|
| Yetenekler (P0) | `capabilities()` / `supports(PhysicsCapability)`: backend özelliklerini sıcak yolda sanal çağrı olmadan bildirir |
| Şekiller | kutu, küre, kapsül, silindir, **convex hull**, **mesh** (statik/kinematik), **heightfield** (statik/kinematik), **compound**; **paylaşımlı & cooked şekiller (`ShapeHandle`)**: `create_shape`, `cook_shape` / `cook_physics_shape`, `inspect_cooked_shape`, `create_shape_from_cooked` (`ASSH` v1 sürümlü ikili başlık + backend ABI doğrulaması) |
| Materyal / Yüzey (P0) | `register_material(surface, PhysicsMaterialDesc)` (`friction`, `restitution`, `friction_combine`, `restitution_combine` — `Average`/`Min`/`Multiply`/`Max`, `density`); `RayHit`, `ShapeHit` ve temaslar `surface` taşır |
| Hareket & Body | `Static`, `Kinematic`, `Dynamic`; kütle (0 = hacim × materyal yoğunluğu), sürtünme, sekme, sönümleme, yerçekimi çarpanı, sensör, CCD (`continuous`) |
| Kontrol | `set_transform`, `move_kinematic`, `set_linear_velocity`, `set_angular_velocity`, `add_impulse`, `add_force`, `add_force_at`, `add_torque`, `wake`, `sleep`, `set_gravity`/`gravity()`, `shift_origin` (gövde + karakter + dünya çapalı eklemler) |
| Katmanlar | 16 çarpışma katmanı (`BodyDesc::layer`), simetrik matris (`set_layer_collision(a, b, false)`) |
| Sorgu & Batch (P0) | `raycast`, `shape_cast`, `overlap` + toplu **`raycast_batch`**, **`shape_cast_batch`**, **`overlap_batch`** (isteğe bağlı `JobSystem*` ile paralel çalışma, paylaşımlı `ShapeHandle` desteği); `QueryFilter{.layers, .ignore, .include_sensors}` |
| Eklemler (P0) | `create_joint`: `Fixed`, `Point`, `Hinge`, `Slider`, `Distance`, **`SixDOF`**, **`SwingTwist`**; açısal/doğrusal limitler (`JointLimitsDesc`), **`JointMotorDesc`** (`Off`/`Velocity`/`Position`, frekans, sönüm, max kuvvet/tork), çalışma zamanı setter'ları (`set_joint_motor_*`, `set_joint_limits`), tepki ölçümü (`joint_reaction`) ve kuvvet/tork kırılma eşikleri (`break_force`/`break_torque` → `broken_joints()`, `break_joint()`, `joint_broken()`) |
| Alanlar | `fields.hpp`: `FieldWorld` — rüzgâr ve su hacimleri; aynı formül `fields.glsl`'de (GPU eşitlik testi) |
| Olaylar | `contacts()` (Begin/End temas & sensör tetikleri), `broken_joints()` (`JointBrokenEvent`) |
| Karakter | kapsül `CharacterVirtual`: zemin/duvar/eğim, zıplama, iç kinematik gövde |

## App içinde

- `PhysicsService` (`ctx.physics_service()`): F1 sabit-adım komut kuyruğu (`add_impulse`, `add_force`, `set_linear_velocity`, `set_angular_velocity`, `set_body_transform`, `wake_body`, `sleep_body`, `set_joint_motor_mode`/`_velocity`/`_position`/`_angular_velocity`/`_rotation`, `break_joint`) ve tamamlanmış adım sorguları (`raycast`, `raycast_batch`, `spherecast`, `overlap_*`, `capabilities`).
- ECS component ve olayları: `RigidBody`, `CharacterController`, `Joint` (7 eklem tipi + limit + kırılma), `JointMotor` (Inspector ve Luau'dan canlı düzenlenebilir), `Contact`, `TriggerEntered`, `TriggerExited`, **`JointBroken`**, **`JointDetached`**.
- **Streaming detach/attach & origin shift:** Bir eklemin bağlı gövdesi hücre unload/silme ile gittiğinde eklem entity'si hayattaysa `JointDetached` olayı gönderilir (`Joint::broken` false kalır); gövde geri geldiğinde eklem otomatik yeniden bağlanır. `shift_origin()` dünya çapalı eklemleri ve ECS `Joint::anchor` konumlarını tutarlı kaydırır.
- **Debug Inspector:** `ctx.debug_show.physics_joints` (veya `physics`) eklem çapalarını, eksenlerini, SwingTwist limit konilerini, motor durumunu ve kırık işaretini çizer.
- **Luau (`astral.physics.*`):** `raycast`, `raycast_batch`, `spherecast`, `overlap_sphere`, `overlap_box`, `impulse`, `force`, `set_velocity`, `velocity`, `teleport`, `wake`, `sleep`, `set_joint_motor_mode`, `set_joint_motor_velocity`, `set_joint_motor_position`, `break_joint`.
- Kabul örnekleri: `examples/physics_playground` (M12/F1 regresyonu) ve `examples/physics_lab` (P0 kabul sahnesi: motorlu eklemler, kırılma, materyaller, cooked shape, batched sorgu, 120 gövde/60 eklem stres ve fixed-step replay determinizmi).

## Bilinen sınırlar

- Sahne dosyası `RigidBody.desc` ile collider, hareket tipi, kütle, sensör ve yüzey ayarlarını,
  `CharacterController.desc` ile kapsül geometrisi, eğim sınırı ve kütleyi tanımlar. Başlangıç konumu entity'nin
  `Transform` bileşeninden okunur. Native handle, karakter komutu ve simülasyon durumu kayda yazılmaz.
  Koddan verilmiş bir `cooked_shape` handle'ı sahneye taşınmaz. Kaydedilecek gövde için `desc.shape` de tanımlanmalıdır.
- Luau'da `astral.physics.move_character(entity, velocity, jump?)` yatay hızı m/s cinsinden ayarlar.
  Zıplama isteği sıradaki sabit adımda bir kez tüketilir. `astral.physics.character_state(entity)` son tamamlanan
  adımın hızını ve zemin temasını verir.
- `JointMotor` modu, hedefleri, yay frekansı (Hz), sönüm oranı ve kuvvet/tork limitleri bir sonraki sabit adımda
  mevcut ekleme uygulanır. Eklemin geometrisi ve bağlantıları oluşturma ayarlarıdır.
- `RigidBody` yalnızca kök entity'lerde (Parent'sız); `desc` oluşturulurken okunur; statik gövde taşımak için `set_body_transform` / `set_transform` kullan.
- Mesh ve heightfield dinamik olamaz ve sorgularda süpürülemez (convex hull / compound kullan).
- Su: kutu hacmi + dalga formülü; gerçek dalga/akış simülasyonu M16/M18.
- Jolt kendi `JobSystemThreadPool` havuzunu kullanır (`astral-bench` ile worker çekişmesi ölçülür; ADR-0038).
- Jolt'un tip kaydı süreç genelinde referans sayımlıdır (`acquire_jolt_registry` / `release_jolt_registry`).

## Dosya düzeni (P0)

- `include/astral/physics/types.hpp`: P0 değer tipleri (yetenekler, materyaller, cooked şekil bilgisi, eklem limit/motor/tepki/kırılma).
- `include/astral/physics/physics.hpp`: `PhysicsWorld`, `ShapeDesc`, `BodyDesc`, `JointDesc`, sorgu ve standalone cooking API'si.
- `src/physics_internal.hpp`: modül içi paylaşılan Jolt katman filtreleri, materyal birleştirme ve `PhysicsWorld::Impl`.
- `src/physics.cpp`: Jolt yaşam döngüsü, yetenekler, materyal tablosu, gövdeler, `step` ve `shift_origin`.
- `src/physics_shapes.cpp`: şekil doğrulama/üretimi (`make_jolt_shape`), `ShapeHandle` havuzu ve sürümlü `ASSH` v1 shape cooking.
- `src/physics_joints.cpp`: 7 eklem tipi (`Fixed`..`SwingTwist`), motor/limit setter'ları, tepki/kırılma ve dünya çapalı origin shift.
- `src/physics_queries_joints.cpp`: tekil ve paralel toplu dar faz sorguları (`*_batch`) ve sanal karakter kontrolcüsü.
- `src/fields.cpp`: rüzgâr ve su hacmi (`FieldWorld`) değerlendirmesi.
