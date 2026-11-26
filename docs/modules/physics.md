# Physics (`Astral::Physics`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core (public); Jolt Physics v5.7.0 (private, `modules/physics/CMakeLists.txt:24`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Jolt Physics üzerinde, public header'larda Jolt tipi göstermeyen bir fizik katmanı: gövdeler, şekiller, eklemler, sorgular,
sanal karakter kontrolcüsü, materyal tablosu ve rüzgâr/su alanı fonksiyonları. ECS'i bilmez (headless test edilir);
ECS ile bağ `Astral::App` içindeki `PhysicsBridge`'dedir (`modules/app/src/services/physics_bridge.cpp`) ve bu belgede
"App köprüsü" bölümünde özetlenir. Yapmadıkları: ragdoll/araç yüksek seviye API'si, yumuşak cisim/kumaş, çapraz platform
deterministik simülasyon garantisi, mesh/heightfield'ı dinamik gövde yapma.

## Public API

Header'lar `modules/physics/include/astral/physics/` altındadır.

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `PhysicsWorld` | Tüm simülasyon; tek thread'den kullanılır (const sorgular `JobSystem` ile paralel çalışabilir) | `modules/physics/include/astral/physics/physics.hpp:255` |
| `PhysicsDesc` | `gravity`, `max_bodies` (65536), `max_body_pairs` (65536), `max_contact_constraints` (10240), `threads` (0 = donanım−1) | `modules/physics/include/astral/physics/physics.hpp:238` |
| `ShapeDesc` (Box, Sphere, Capsule, Cylinder, ConvexHull, Mesh, HeightField, Compound), `ShapeType` | Şekil tanımı; Mesh ve HeightField yalnız statik/kinematik | `modules/physics/include/astral/physics/physics.hpp:34`, `modules/physics/include/astral/physics/types.hpp:23` |
| `BodyDesc`, `MotionType`, `create_body`, `destroy`, `set_transform`, `move_kinematic`, `set_linear_velocity`, `add_impulse/force/force_at/torque/angular_impulse`, `wake`, `sleep` | Gövde yaşam döngüsü ve kontrolü | `modules/physics/include/astral/physics/physics.hpp:93`, `modules/physics/include/astral/physics/physics.hpp:280` |
| `create_shape`, `cook_shape`, `cook_physics_shape`, `inspect_cooked_shape`, `create_shape_from_cooked`, `CookedShapeInfo` | Paylaşımlı `ShapeHandle` ve `ASSH` v1 ikili şekil biçimi | `modules/physics/include/astral/physics/physics.hpp:248`, `modules/physics/include/astral/physics/types.hpp:71` |
| `register_material`, `PhysicsMaterialDesc`, `PhysicsCombineMode` | `BodyDesc::surface` kimliği başına sürtünme/sekme/yoğunluk; birleştirme kuralları | `modules/physics/include/astral/physics/physics.hpp:273`, `modules/physics/include/astral/physics/types.hpp:62` |
| `set_layer_collision`, `layers_collide`, `QueryFilter` | 16 katman, simetrik matris; sorgu filtresi `layers`, `ignore`, `ignore_list`, `include_sensors` | `modules/physics/include/astral/physics/physics.hpp:311`, `modules/physics/include/astral/physics/physics.hpp:123` |
| `raycast`, `shape_cast`, `overlap`, `raycast_batch`, `shape_cast_batch`, `overlap_batch` | Sorgular; toplu sürümler `JobSystem*` ile paralel (≥ 32 öğe) | `modules/physics/include/astral/physics/physics.hpp:344` |
| `JointDesc`, `JointType` (Fixed, Point, Hinge, Slider, Distance, SixDOF, SwingTwist), `create_joint` | Eklem oluşturma; `JointLimitsDesc`, `JointMotorDesc`, `break_force`/`break_torque` | `modules/physics/include/astral/physics/physics.hpp:200`, `modules/physics/include/astral/physics/types.hpp:95` |
| `set_joint_motor_*`, `set_joint_limits`, `break_joint`, `joint_broken`, `joint_reaction`, `broken_joints` | Çalışma anı motor/limit değişimi, kırma, tepki ölçümü, kırılma olayları | `modules/physics/include/astral/physics/physics.hpp:323` |
| `create_character`, `update_character`, `character_state`, `set_character_position`, `CharacterDesc` | Kapsül `CharacterVirtual` | `modules/physics/include/astral/physics/physics.hpp:360` |
| `step`, `contacts`, `shift_origin`, `capabilities` | Simülasyon adımı, Begin/End temas ve sensör olayları, floating origin, yetenek bayrakları | `modules/physics/include/astral/physics/physics.hpp:338`, `modules/physics/include/astral/physics/physics.hpp:314` |
| `FieldWorld`, `WindDesc`, `WindVolume`, `WaterVolume`, `wind_of`, `water_surface_height`, `field_noise` | Konum ve zamanın saf fonksiyonu olarak rüzgâr ve su; aynı formül `fields.glsl`'de | `modules/physics/include/astral/physics/fields.hpp:68`, `modules/physics/shaders/include/astral/fields.glsl` |

## Nasıl çalışır

**Jolt kurulumu.** `FetchContent` ile Jolt `v5.7.0` indirilir; taşınabilir SIMD (AVX/AVX2/AVX512/LZCNT/F16C/FMADD kapalı),
Jolt tarafında uyarı-hata/LTO/FP tuzağı yok, RTTI ve istisnalar açık, dinamik CRT (`modules/physics/CMakeLists.txt:4`,
`modules/physics/CMakeLists.txt:13`). Tek duyarlıklı (float) Jolt kullanılır; CMake çift duyarlık seçeneğini açmaz (`modules/physics/CMakeLists.txt:4`),
büyük dünyalar için floating origin kullanılır. `acquire_jolt_registry`/`release_jolt_registry` süreç genelinde referans sayımlıdır;
ilk `PhysicsWorld` türleri kaydeder, sonuncusu siler (`modules/physics/src/physics.cpp:41`). Her `PhysicsWorld` kendi
`JPH::JobSystemThreadPool`'unu (donanım−1 thread) ve 32 MiB'lık `TempAllocatorImpl`'ini kurar
(`modules/physics/src/physics.cpp:72`). Jolt assert'leri etkinse loglanıp devam edilir, tuzağa düşülmez
(`modules/physics/src/physics.cpp:30`).

**Katmanlar.** Astral katmanı `0..15`; Jolt nesne katmanı = `katman * 2 + (hareketli ? 1 : 0)`, yalnız iki broad-phase katmanı
vardır (statik/hareketli) (`modules/physics/src/physics_internal.hpp:32`). İki statik gövde hiç test edilmez; aksi halde
çarpışma matrisi (`collide[a]` bit `b`) kullanılır (`modules/physics/src/physics_internal.hpp:68`). Sorgular katman maskesini
`LayerMask` ile uygular (`modules/physics/src/physics_internal.hpp:81`).

**Gövde.** `create_body` şekli doğrular (pozitif/sonlu boyutlar, hull için ≥ 4 nokta ve hacim, mesh indeks aralığı,
heightfield örnek sayısı ≥ 4) ve hatada geçersiz handle döndürüp log yazar
(`modules/physics/src/physics_shapes.cpp:102`, `modules/physics/src/physics.cpp:126`). Dinamik gövdede mesh/heightfield reddedilir
(`modules/physics/src/physics.cpp:146`). Kütle 0 ise yoğunluk × hacim (materyal varsa) ya da Jolt varsayılanı kullanılır
(`modules/physics/src/physics.cpp:171`). Sürtünme/sekme gövde oluşturulurken kayıtlı materyalden alınır, temas anında ise
`Contacts::apply_materials` iki yüzeyin birleştirme moduyla `mCombinedFriction/Restitution`'ı ezer
(`modules/physics/src/physics_internal.hpp:238`). İki farklı mod varsa öncelik `Average < Min < Multiply < Max`
(`modules/physics/include/astral/physics/types.hpp:53`).

**Adım ve olaylar.** `step(dt)` önceki adımın temas ve kırılma listelerini temizler, `PhysicsSystem::Update` çağırır, taşma
durumunda yalnız log yazar (`PhysicsWorld::step` dönüş değeri `void`), sonra kırılabilir eklemleri denetler
(`modules/physics/src/physics.cpp:313`). Temas Begin/End olayları Jolt worker'larından `mutex` altında toplanır
(`modules/physics/src/physics_internal.hpp:188`). Yok edilen gövdelerin kimliği bir adım daha tutulur ki o adımdaki End olayı
`user_data` taşıyabilsin (`modules/physics/src/physics.cpp:319`).

**Sorgular.** Işın, şekil süpürme ve çakışma `NarrowPhaseQuery` ile yapılır. `QueryFilter::ignore` + `ignore_list` her sorguda
bir `BodyID` vektörüne çevrilir (sorgu başına tahsis) (`modules/physics/src/physics_queries_joints.cpp:30`); sensörler yalnız
`include_sensors` ile görülür (`modules/physics/src/physics_internal.hpp:101`). Mesh/heightfield ile süpürme ve çakışma boş
döner (`modules/physics/src/physics_queries_joints.cpp:121`, `modules/physics/src/physics_shapes.cpp:157`). Toplu sorgular `n ≥ 32` ve
`JobSystem` verilmişse `parallel_for` ile çalışır, aksi halde seri (`modules/physics/src/physics_queries_joints.cpp:20`,
`modules/physics/src/physics_queries_joints.cpp:132`). Çakışma sonuçlarında gövde tekrarları doğrusal `std::find` ile ayıklanır
(`modules/physics/src/physics_queries_joints.cpp:78`). Karakterin iç gövdeleri `BodyHandle`'sız olarak `user_data` ile bildirilir
(`modules/physics/src/physics_queries_joints.cpp:82`).

**Şekil cook.** `ASSH` başlığı: sihir, sürüm 1, backend hash'i, şekil tipi, bayraklar (`static_only`), yük boyutu, ardından Jolt'un
`SaveWithChildren` verisi (`modules/physics/src/physics_shapes.cpp:20`). `create_shape_from_cooked` başlığı ve boyutu doğrular, sürüm
ve backend hash'i uyuşmazsa reddeder (`modules/physics/src/physics_shapes.cpp:267`). Backend hash'i sabit bir dizgenin
(`"Jolt-5.7.0-f32-ASSH-v1"`) FNV-1a'sıdır; Jolt'un gerçek sürümünden türetilmez (`modules/physics/src/physics_shapes.cpp:146`).

**Eklemler.** Yedi eklem tipi Jolt kısıtlarına eşlenir (`modules/physics/src/physics_joints.cpp:130`). SixDOF: eksen başına
Fixed/Free/Limited (`min > max` = kilitli, ±1e30 = serbest) (`modules/physics/src/physics_joints.cpp:42`); SwingTwist: koni/piramit
swing + twist sınırları ve motor (`modules/physics/src/physics_joints.cpp:72`). Motor: Off/Velocity/Position, frekans+sönüm yayı,
kuvvet/tork sınırı (`modules/physics/src/physics_joints.cpp:29`, `modules/physics/src/physics_joints.cpp:91`). İki gövdesi arasındaki çarpışma
`collide_connected == false` ise `PairFilter` ile kapatılır (`modules/physics/src/physics_internal.hpp:166`,
`modules/physics/src/physics_joints.cpp:343`). Kırılma: her adım sonunda Jolt'un toplam lambda'sı `dt`'ye bölünerek kuvvet/tork
tepkisi üretilir; eşik aşılırsa kısıt sistemden çıkarılır ve `JointBrokenEvent` yazılır
(`modules/physics/src/physics_joints.cpp:206`, `modules/physics/src/physics_joints.cpp:279`). Dünyaya bağlı eklemler `shift_origin`'de silinip yeniden
kurulur (`modules/physics/src/physics_joints.cpp:293`).

**Karakter.** Kapsül `CharacterVirtual`; toplam boy `height`, ayak konumu pozisyon (`modules/physics/src/physics_queries_joints.cpp:189`).
`height < 2*radius + 0.02` reddedilir (`modules/physics/src/physics_queries_joints.cpp:198`). `update_character` yatay hızı
(x, z) ayarlar, zemin hızını izler, yer çekimini ekler ve `ExtendedUpdate` çağırır; zıplama yer temasındayken `jump_speed` ekler
(`modules/physics/src/physics_queries_joints.cpp:244`).

**Alan fonksiyonları.** Rüzgâr: sabit hız + hız doğrultusunda gust (1B değer gürültüsü) + 3B türbülans; hacimler eksene hizalı
kutu, kenarda `falloff` ile yumuşar, `replace` ise global rüzgârla `lerp` edilir (`modules/physics/src/fields.cpp:59`,
`modules/physics/src/fields.cpp:93`). Su: üst yüzey `center.y + half_extents.y`, iki sinüsün karışımıyla dalga
(`modules/physics/src/fields.cpp:83`); `water_at` yüzeyin altındaki en derin hacmi döndürür (`modules/physics/src/fields.cpp:105`).
Gürültü/hash CPU ve GLSL'de aynıdır; GPU eşitlik testi `tests/integration/fields_gpu_test.cpp` içinde (bu incelemede çalıştırılmadı).

### App köprüsü (özet)

`PhysicsBridge` (`modules/app/src/services/physics_bridge.hpp:38`) ECS ile bağdır: `RigidBody`, `CharacterController`, `Joint`,
`JointMotor` component'lerini gövde/karakter/eklem handle'larıyla eşler (`modules/app/include/astral/app/components.hpp:88`,
`modules/app/include/astral/app/components.hpp:171`); `RigidBody.desc` yaratıldıktan sonra yeniden okunmaz ve `Parent`'lı
entity desteklenmez (`modules/app/include/astral/app/components.hpp:90`). Sabit adım birikimi `AppDesc::physics_rate` (60 Hz) ve
`max_physics_steps` (4) ile sınırlıdır; kalan süre düşer (`modules/app/include/astral/app/app.hpp:224`,
`modules/app/src/services/physics_bridge.cpp:378`). Her sabit adımda sırayla: `FixedUpdate` sistemleri, `apply_commands`,
`sync_bodies`, motor eşitleme, komut kuyruğunu boşaltma, kinematik hareket, karakter güncelleme, alan kuvvetleri, `physics->step`,
kırılma toplama (`modules/app/src/services/physics_bridge.cpp:388`). Komut kuyruğu (`add_impulse`, `add_force`, `add_force_at`,
`add_torque`, `add_angular_impulse`, `set_*_velocity`, `set_body_transform`, `wake/sleep_body`, `set_joint_motor_*`, `break_joint`)
thread-safe'tir ve `(order_key, üretici sırası, sıra no)` ile deterministik sıralanır; `order_key == 0` ise entity'ye göre
(`modules/app/src/services/physics_commands.cpp:27`, `modules/app/src/services/physics_commands.cpp:47`,
`modules/app/src/services/command_producer.hpp:14`). Alan kuvvetleri `WindZone`, `WaterBody`, `Buoyant`, `AirDrag`, `Airfoil`
component'lerinden her adım yeniden kurulan `FieldWorld` ile uygulanır (`modules/app/src/services/physics_fields.cpp:96`,
`modules/app/src/services/physics_fields.cpp:127`). Streaming sırasında bağlı gövdesi kaybolan eklem için `JointDetached` olayı gönderilir
ve gövde dönünce eklem yeniden kurulur (`modules/app/src/services/physics_bridge.cpp:187`). Floating origin: `PhysicsBridge::shift_origin`
fiziği, bekleyen komut konumlarını ve `Joint::anchor`'ları kaydırır (`modules/app/src/services/physics_bridge.cpp:160`). Luau
`astral.physics.*` (raycast `{ignore = ..., sensors = true}`, `torque`, `force_at`, `angular_impulse`, `set_angular_velocity`, `angular_velocity`,
`move_character`, `character_state`, eklem motoru, `break_joint`) `modules/script/src/api.cpp:69` listesindedir.

## Güçlü yanlar

- **Jolt gizli:** public header'larda Jolt tipi yok; `Astral::Core` dışında bağımlılık yok (`modules/physics/CMakeLists.txt:40`).
- **Eklem kapsamı geniş ve test edilmiş:** Fixed/Point/Hinge/Slider/Distance/SixDOF/SwingTwist, motorlar, limitler, kırılma ve
  tepki ölçümü (`modules/physics/tests/joints_test.cpp`, `modules/physics/tests/physics_v2_test.cpp`).
- **Girdi doğrulaması:** geçersiz şekil/katman/eklem çökme yerine hata log'u + geçersiz handle döndürür
  (`modules/physics/tests/shapes_test.cpp` "bad shapes are refused ...", `modules/physics/tests/physics_test.cpp` "invalid shapes are refused ...").
- **Yaşam döngüsü sağlam:** eski handle'lar yeni gövdeye ulaşmaz, silinen gövdenin eklemleri gider, End temas olayı silinmiş
  gövdenin kimliğini taşır (PH01) (`modules/physics/tests/physics_test.cpp`).
- **Deterministik komut sırası:** paralel sistemlerden gelen çakışan komutlar kayıt sırasına göre çözülür
  (`modules/app/tests/physics_step_contract_test.cpp` "PH05 ...", "parallel and serial systems ... identical simulation state").
- **Toplu sorgu eşdeğerliği:** toplu sorgular tekil sorgularla seri ve paralel modda aynı sonucu verir
  (`modules/physics/tests/physics_v2_test.cpp` "Batched queries match single queries ...").
- **Alan fonksiyonları sayısal olarak savunmalı:** devasa ve sonlu olmayan koordinatlar tanımlı (PH07)
  (`modules/physics/src/fields.cpp:33`, test "PH07").

## Zayıf yanlar ve bilinen sınırlar

- **Çapraz platform determinizm yok:** `CROSS_PLATFORM_DETERMINISTIC` Jolt seçeneği CMake'te ayarlanmıyor
  (`modules/physics/CMakeLists.txt:4`); ayrıca AVX vb. kapatılmış, FMA yok. Determinizm iddiası "aynı ikili/makinede sabit adımlı
  tekrar oynatma" ile sınırlı (`examples/physics_lab/tests/game_test.cpp`, bu incelemede çalıştırılmadı). Çok thread'li Jolt
  çözümünün farklı çekirdek sayılarında aynı sonucu verdiği doğrulanmadı.
- **Tek duyarlıklı dünya:** Jolt float; uzak koordinatlarda hassasiyet kaybı floating origin'e bırakılmış
  (`modules/physics/src/physics.cpp:342`). `shift_origin` kayma yalnız App'in çağırdığı anda çalışır; eklemler ve bekleyen
  komutlar App'te ele alınır.
- **Mesh/heightfield kısıtları:** dinamik olamaz, süpürme/çakışma sorgularında şekil olarak kullanılamaz
  (`modules/physics/src/physics_shapes.cpp:157`, `modules/physics/src/physics_queries_joints.cpp:121`).
- **Cook doğrulaması zayıf:** `ASSH` başlığındaki `static_only` bayrağı geri yüklenen gerçek şekil ile karşılaştırılmaz
  (`modules/physics/src/physics_shapes.cpp:286`); backend hash'i gerçek Jolt sürümünden değil sabit dizgeden üretilir, Jolt
  güncellenirse dizge elle değişmezse eski cook çıktıları uyumlu görünür (`modules/physics/src/physics_shapes.cpp:146`). Jolt'un
  ikili yükleme kodu güvenilir girdi varsayar; bozuk içerikle fuzz yapılmadı (doğrulanmadı).
- **`step` hata bildirmez:** Jolt taşması (`max_body_pairs`/`max_contact_constraints`) yalnız log'lanır; dönüş değeri yok, App köprüsü
  adım sayacını koşulsuz artırır (`modules/physics/src/physics.cpp:322`, `modules/app/src/services/physics_bridge.cpp:408`).
- **Fizik ayarları AppDesc'ten verilemiyor:** köprü `PhysicsDesc`'i yalnız `logger` ile kurar; `max_bodies`, `max_body_pairs`,
  `max_contact_constraints`, `threads`, `gravity` varsayılanda kalır (gravity sonradan `set_gravity` ile değişir)
  (`modules/app/src/services/physics_bridge.cpp:105`). Bu bir kod okumasıdır; başka yerden ayar yolu aranmadı.
- **Ayrı iş parçacığı havuzları:** fizik Jolt'un kendi havuzunu (donanım−1 thread) kullanır; motorun `JobSystem`'i yalnız toplu
  sorgularda devreye girer. İkisinin birlikte çekirdek çekişmesi yaratıp yaratmadığı ölçülmedi
  (`modules/physics/src/physics.cpp:67`).
- **Karakter kısıtları:** kapsülün katmanı sabit `0` (hareketli), `CharacterDesc` katman almaz; çarpışma matrisi karakter için
  yalnız katman 0 üzerinden çalışır (`modules/physics/src/physics_queries_joints.cpp:215`,
  `modules/physics/src/physics_queries_joints.cpp:266`). Yatay hareket havada da doğrudan hızla ayarlanır (hava kontrolü modeli yok)
  (`modules/physics/src/physics_queries_joints.cpp:262`).
- **SixDOF yay ayarı yalnız doğrusal limitlerde:** `spring_frequency/damping` yalnız üç öteleme ekseninin limit yayına uygulanır
  (`modules/physics/src/physics_joints.cpp:63`). `set_joint_limits(min, max)` yalnız Hinge/Slider/Distance için Jolt'a işler;
  SixDOF/SwingTwist için `JointLimitsDesc` sürümü kullanılmalı (`modules/physics/src/physics_joints.cpp:455`).
- **Dünyaya bağlı eklem yeniden kurulumu durumu sıfırlar:** `shift_origin` kısıtı silip yeniden yaratır; warm-start birikimi gider,
  yeniden kurma başarısız olursa `JointRecord` geçerli ama kısıt boş kalabilir (`modules/physics/src/physics_joints.cpp:293`).
  Testte yalnız nokta eklemli sarkaç var (`modules/physics/tests/physics_v2_test.cpp` "Origin shift preserves ...").
- **Sistem içi paralel iş ve komut sırası:** bir sistem kendi içinde paralel iş açıp aynı entity'ye çakışan komut yollarsa
  `order_key`'i kendisi vermelidir; çalışma zamanı uyarısı yok (`modules/app/src/services/physics_commands.cpp:56`,
  [ADR-0043](../decisions/0043-deterministic-physics-command-producers.md)).
- **Köprüde her adım O(N) tarama:** `sync_bodies` her adımda (ve kare başında) `RigidBody`, `CharacterController`, `Joint`
  sorgularını gezer; `RigidBody` mutable erişimi her seferinde değişiklik damgalar
  (`modules/app/src/services/physics_bridge.cpp:247`, `modules/app/src/services/physics_bridge.cpp:264`). Büyük sahnede ölçülmedi.
- **Alan modeli basit:** rüzgâr/su hacimleri eksene hizalı kutular (dönüş ve ölçek yok) (`modules/physics/include/astral/physics/fields.hpp:29`);
  su yüzeyi analitik, akışkan simülasyonu yok. `Buoyant::volume == 0` ise hacim `kütle / 500` varsayılır (sabit varsayım)
  (`modules/app/src/services/physics_fields.cpp:41`); `water_at` yalnız en derin hacmi döndürür
  (`modules/physics/src/fields.cpp:113`).
- **Sensörler:** sensör gövdelerde Jolt'un `mCollideKinematicVsNonDynamic` bayrağı açılır
  (`modules/physics/src/physics.cpp:167`); uyuyan gövde çakışması ve bileşik
  şekil (compound) için Begin/End temas sayımının ne olduğu test edilmedi (doğrulanmadı; denetim notu PHR01).
- **Eksik ADR dosyası:** `ADR-0045` (sorgu `ignore_list`) kodda referans verilmiş (`modules/physics/include/astral/physics/physics.hpp:126`)
  ama `docs/decisions/` altında ADR-0045 dosyası yok.

## Testler

`modules/physics/tests/` (doctest, `modules/physics/CMakeLists.txt:50`):

| Dosya | Kapsam |
|---|---|
| `physics_test.cpp` | Yerçekimi, durma, statik, raycast, yok edilen gövde, impuls/hız, kinematik, temas ve sensör olayları, karakter yürüme/zıplama, End olay kimliği (PH01), çakışma sorguları karakteri bulur (PH02), `shift_origin` |
| `layers_test.cpp` | Katman matrisi, statik–statik, sorgu filtresi (`ignore`, sensör), shape cast, overlap, yüzey kimliği, sensör bayrağı |
| `shapes_test.cpp` | Hull, mesh zemin, heightfield, compound, hatalı şekiller, PH06 hull noktası sırası |
| `joints_test.cpp` | Sarkaç, menteşe+limit, slider, fixed, mesafe yayı, ömür/geçersiz eklem, plank köprü |
| `physics_v2_test.cpp` | Yetenekler, hinge/slider motorları, motor ayarı, SixDOF, SwingTwist, kırılma, `ASSH` gidiş-dönüş ve uyumsuzluk, materyal birleştirme, toplu sorgu eşitliği, origin shift + dünyaya bağlı eklem |
| `fields_test.cpp` | Gürültü, rüzgâr (gust/türbülans/hacim/falloff), su yüzeyi, PH07 |
| App düzeyinde | `modules/app/tests/physics_step_contract_test.cpp` (komut sırası, sabit adım, streaming detach), `modules/app/tests/physics_angular_test.cpp:126` (`ignore_list`, sensörler); `examples/physics_lab/tests`, `examples/physics_playground/tests` |
| GPU | `tests/integration/fields_gpu_test.cpp` (CPU–GLSL alan eşitliği; bu makinede çalıştırılmadı) |

Kapsanmayanlar: bozuk `ASSH` verisiyle fuzz, çok eklemli sistem stres/kararlılık ölçümü, farklı çekirdek
sayılarında determinizm, `max_bodies` aşımı davranışı, uyuyan gövde temas semantiği, karakter–dinamik gövde itme kuvveti
doğruluğu. `physics_playground` golden testi bu makinede kalıyor (`golden_hw_physics_playground`, ton farkı; fizikle ilgisi
araştırılmadı).

## Performans

Ölçülmedi (bu belge için). Repo içinde `astral-bench` fizik ölçümü içermez (`bench/main.cpp` ECS, transform, script, streaming,
shader derleme); `examples/physics_lab` 120 gövde/60 eklem stres sahnesi içerir fakat zamanlama değeri bu incelemede
alınmadı.

## Kararlar

- [0010 — Jolt Physics](../decisions/0010-jolt-physics.md)
- [0029 — Fizik alanları ve eklemler](../decisions/0029-physics-fields-and-joints.md)
- [0034 — Gameplay API ve fizik adım sözleşmesi](../decisions/0034-gameplay-api-and-physics-step-contract.md)
- [0038 — Physics API v2](../decisions/0038-physics-api-v2.md) (Jolt v5.7.0, SixDOF, motorlar, kırılma, cook, materyal, toplu sorgu)
- [0043 — Deterministik fizik komut üreticileri](../decisions/0043-deterministic-physics-command-producers.md)
- `ADR-0045` (sorgu `ignore_list`): kodda referans var, dosyası `docs/decisions/` altında yok.
