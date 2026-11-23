# ADR-0038: Physics API v2 (P0)

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-23
- **İlişkili ADR'ler:** [ADR-0007](0007-physics-jolt.md), [ADR-0023](0023-modular-game-code.md), [ADR-0034](0034-gameplay-contract-and-physics-pipeline.md), [ADR-0036](0036-world-authoring-and-level-instances.md)
- **Hedef faz:** P0 — Physics API v2

## Bağlam

`Astral::Physics` (M4/M12) Jolt `v5.3.0` üzerinde sabit/temel gövdeler, 5 temel eklem (`Fixed`, `Point`, `Hinge`, `Slider`, `Distance`), tekil sorgular ve `FieldWorld` sunar. Yol haritasındaki **M16 (su örnekleme)**, **M19 (SDF çarpışma)**, **M21/S1 (pasif & aktif ragdoll)**, **M23/V0 (rigid vehicle)**, **M23/V1 (node-beam)** ve **M23/S2 (cloth/soft body)** fazları şu eksikleri kapatmadan ilerleyemez:

1. **Eklem ve motor yetenekleri:** `SixDOF` ve `SwingTwist` eklemleri, açısal limit koni/piramit/bükülme sınırları, hız ve pozisyon motor/drive hedefleri, kuvvet/tork eşiğine bağlı kırılma ve debug görselleştirmesi.
2. **Sürümlü şekil kaynakları (shape cooking):** `Mesh`, `ConvexHull`, `HeightField` ve `Compound` şekillerinin her `create_body` veya sorguda sıfırdan pişirilmesi yerine paylaşılabilir `ShapeHandle` ve sürümlü ikili serileştirme.
3. **Fizik materyal/yüzey eşlemesi:** Ham `uint16_t surface` alanı ile gövde başı sürtünme/sekme değerlerinin birleşik `PhysicsMaterialDesc` + combine mode tablosuna bağlanması.
4. **Batched sorgular ve genişletilmiş F1 komut hattı:** Çoklu raycast/shape_cast/overlap sorgularının sıcak yolda sanal çağrı olmadan toplu (ve isteğe bağlı paralel) yürütülmesi; eklem motoru, kırılma, teleport ve sleep/wake komutlarının F1 sabit-adım kuyruğuna eklenmesi.
5. **Streaming detach/attach & floating origin:** Hücre unload/reload ve `shift_origin()` sırasında gövde ve eklem sahipliğinin kopmaması ve sabit adımlı replay determinizminin korunması.

## Kararlar

### 1. Jolt Sürümü ve Backend Yetenek Sorgusu

- JoltPhysics pini en güncel kararlı sürüm olan **`v5.7.0`**'a yükseltilir (`modules/physics/CMakeLists.txt`).
- `Astral::Physics` Layer 2 olarak yalnızca `Astral::Core`'a bağlı kalır; public header'larda Jolt tipi yer almaz.
- Backend yetenekleri `PhysicsCapability` bitmask'i üzerinden `PhysicsWorld::capabilities()` ve `PhysicsWorld::supports(PhysicsCapability)` ile dar bir değer sorgusu olarak sunulur.
- Sıcak yolda (body/joint/query/step) gövde başına virtual dispatch eklenmez.
- Jolt'un kendi `JPH::JobSystemThreadPool` havuzu korunur; `astral-bench` ile Astral `JobSystem` eşzamanlılığında worker çekişmesi ölçülür. Darboğaz ölçülmedikçe özel Jolt job adaptörüne geçilmez.

### 2. Eklemler, Motor/Drive ve Kırılma Sözleşmesi

- `JointType` enum'ına `SixDOF` ve `SwingTwist` eklenir (`Fixed`, `Point`, `Hinge`, `Slider`, `Distance`, `SixDOF`, `SwingTwist`).
- `JointMotorDesc` (`JointMotorMode::Off | Velocity | Position`, `frequency_hz`, `damping`, `max_force`, `max_torque`) ve `JointLimitsDesc` (6-DOF eksen limitleri + SwingTwist normal/plane/twist koni limitleri) `JointDesc` içinde tutulur.
- Çalışma zamanı denetimi:
  - `PhysicsWorld::set_joint_motor_mode`, `set_joint_motor_velocity`, `set_joint_motor_position`, `set_joint_motor_rotation`, `set_joint_limits`, `break_joint`, `joint_broken`, `joint_reaction`.
- **Kırılma politikası:** `JointDesc::break_force > 0` veya `break_torque > 0` verildiğinde `PhysicsWorld::step(dt)` sonrası Jolt kısıtının son adım toplam lineer/açısal lambda kuvvet/tork karşılığı eşikle karşılaştırılır (`|lambda| / dt >= break_force` veya `break_torque`). Eşik aşıldığında:
  1. Kısıt Jolt'tan çıkarılır ve `pair_filter` kilidi bırakılır (bağlı gövdeler çarpışmaya geri döner).
  2. `JointRecord::broken = true` işaretlenir; `PhysicsWorld::broken_joints()` o adımda kırılan eklemler için `JointBrokenEvent` üretir.
  3. App `PhysicsBridge`, kırılan eklemin ECS `Joint::broken = true` alanını günceller ve `astral::JointBroken` olayını gönderir (entity veya component otomatik silinmez; oyun/Luau kuralı karar verir).

### 3. Shape Cooking ve Sürümlü Şekil Kaynakları (`ASSH` v1)

- `ShapeHandle` (`Handle<ShapeTag>`) paylaşımlı, değiştirilemez çarpışma şeklidir.
- `PhysicsWorld::create_shape(const ShapeDesc&)`, `cook_shape(const ShapeDesc&) -> std::vector<std::byte>`, `create_shape_from_cooked(std::span<const std::byte>)`, `destroy(ShapeHandle)`, `valid(ShapeHandle)`.
- **İkili kapsayıcı (`ASSH` v1):**
  - `magic[4] = "ASSH"`, `uint32_t format_version = 1`, `uint32_t backend_abi_hash` (Jolt sürüm/yapılandırma karması), `uint32_t shape_type`, `uint32_t flags` (ör. `static_only`), `uint32_t payload_size`, ardından Jolt `SaveBinaryStateWithChildren` ikili çıktısı.
  - Uyumsuz `magic`, `format_version` veya `backend_abi_hash` algılandığında `create_shape_from_cooked` geçersiz `ShapeHandle` döner ve açık teşhis loglar (sessiz çökme yok).
- `BodyDesc::cooked_shape` (opsiyonel `ShapeHandle`) verildiğinde `create_body` bu şekli yeniden pişirmeden doğrudan kullanır; geçersizse mevcut `BodyDesc::shape` yolu aynen çalışır (%100 geriye uyumlu).
- `astral-cook` `.ashape` (JSON `ShapeDesc` tanımı) kaynaklarını sürümlü `ASSH` v1 ikilisine derleyebilir.

### 4. Fizik Materyal / Yüzey Tablosu

- `PhysicsCombineMode` (`Average`, `Min`, `Multiply`, `Max`) ve `PhysicsMaterialDesc` (`friction`, `restitution`, `friction_combine`, `restitution_combine`, `density`, `name`).
- `PhysicsWorld::register_material(std::uint16_t surface, const PhysicsMaterialDesc&)` ve `material(std::uint16_t surface)` ile `BodyDesc::surface` (0..65535) değerleri tablodan çözülür.
- Jolt `SetCombineFriction` / `SetCombineRestitution` callback'leri her iki gövdenin kayıtlı materyallerini (kayıtlı değilse gövde üzerindeki `friction`/`restitution` değerlerini) birleştirme moduna göre hesaplar. `surface == 0` kayıtlı değilse eski Jolt davranışı korunur.

### 5. Batched Sorgular ve F1 Komut Hattı Genişletmesi

- `PhysicsWorld`:
  - `raycast_batch(std::span<const RayQuery>, std::span<std::optional<RayHit>>, JobSystem* jobs = nullptr)`
  - `shape_cast_batch(std::span<const ShapeCastQuery>, std::span<std::optional<ShapeHit>>, JobSystem* jobs = nullptr)`
  - `overlap_batch(std::span<const OverlapQuery>, std::span<std::vector<OverlapHit>>, JobSystem* jobs = nullptr)`
  - `jobs != nullptr` ve sorgu sayısı eşik üstündeyse (`>= 32`) `JobSystem::parallel_for` ile kilit çatışmasız okuma yapılır (`NarrowPhaseQuery` thread-safe).
- `PhysicsService` (F1 deterministik komut kuyruğu):
  - Mevcut gövde komutlarına ek olarak: `set_body_transform`, `wake_body`, `sleep_body`, `set_joint_motor_velocity`, `set_joint_motor_target`, `break_joint` + `raycast_batch` ve `capabilities()`.
- `ScriptPhysics` + Luau `astral.physics.*`:
  - `raycast_batch`, `set_joint_motor_velocity`, `set_joint_motor_target`, `break_joint`, `wake`, `sleep`, `teleport` ve `astral::JointBroken` olayı. `API.md` ve `astral.d.luau` tek katalogdan güncellenir.

### 6. Streaming Detach/Attach ve Floating Origin

- `PhysicsWorld::shift_origin(Vec3 offset)`:
  - Tüm gövde ve karakter konumlarına ek olarak, bir ucu dünyaya sabitli (`!body_a.valid()`) eklemlerin dünya çapa konumları da `-offset` ile tutarlı kaydırılır (kısıtlar yeni dünya merkezinde patlamadan korunur).
- `PhysicsBridge` sahiplik ve streaming sözleşmesi:
  - Bir eklemin bağlı gövdelerinden biri hücre unload nedeniyle yok edildiğinde, eklem entity'si hâlâ hayattaysa kısıt Jolt'tan temizce ayrılır (`detached`), `Joint::broken` **false** kalır (`JointBroken` yanlışlıkla tetiklenmez).
  - İlgili gövde yeniden yüklendiğinde (`sync_bodies`), kırılmamış ve bağlantısı kopmuş `Joint` otomatik olarak yeniden bağlanır (`re-attached`).
  - `Joint::broken == true` olan bir eklem kullanıcı açıkça sıfırlamadıkça otomatik yeniden bağlanmaz.

## Doğrulama ve Kabul

- Birim & entegrasyon testleri: `astral_physics_tests`, `astral_app_tests`, `astral_script_tests`, `physics_playground_tests`.
- Kabul örneği: `examples/physics_lab` + `physics_lab_tests` (motorlu eklemler, limit/stres, kırılma, materyal birleştirme, cooked shape, batched sorgu, streaming detach/attach + origin shift, sabit adım replay determinizmi).
- Kapılar: Debug & Release (`-DASTRAL_WARNINGS_AS_ERRORS=ON`), `code_size_check`, `layer_check`, `astral_public_headers_clean`, `*_features_check`, `*_script_types`.
- **Release Ölçüm Özeti (`astral-bench`, Windows 11 x64, MSVC 19.51, Jolt `v5.7.0`):**
  - 400 dinamik convex hull spawn: uncooked `1.53 ms` → cooked `ASSH` v1 `ShapeHandle` **`0.14 ms`** (~10.9× hızlanma).
  - 400 gövdeli adım + worker çekişmesi: `step_solo` p50/p95 = `0.184 / 0.304 ms`; 4 worker Astral `JobSystem` eşzamanlı yükü altında `step_contended` p50/p95 = `0.237 / 0.346 ms` (+0.053 ms fark → Jolt `JobSystemThreadPool` korunması doğrulandı).
  - 4096 batched raycast: seri `1.24 ms`, paralel (`JobSystem`) `1.13 ms`, RSS `39.4 MB`.

