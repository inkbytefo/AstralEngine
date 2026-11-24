# Astral Game Architecture — World, Scene, Prefab, ECS ve Gameplay

**25 Kasım 2026 · İncelenen kaynak:** [21a17b6fe217219f37b557e6dcaaf69802a534d1](https://github.com/inkbytefo/AstralEngine/commit/21a17b6fe217219f37b557e6dcaaf69802a534d1).
Mevcut runtime'ın kaynak temelli rehberi; yeni Studio/AI hedef mimarisi değil. Örnekler gerçek repository formatları ve API'leriyle uyumlu olacak şekilde hazırlanmıştır, bu oturumda derlenip çalıştırılmadı. Önceki audit bulguları geçerlidir.

## 1. Zihinsel model

Astral'da Game + FeatureSet davranışları kurar; App frame loop ve servisleri çalıştırır; C++ World runtime ECS container'dır. Bir .scene ya da .world dosyası bu ECS'ye veri sağlar. Entity bir kimlik, component veri, system davranıştır. Prefab tekrar kullanılan tek köklü scene şablonu. .world streaming haritasıdır, ECS'nin kendisi değildir.

```text
Game Project (CMake, Game, FeatureSet, assets)
 ├─ Game / Feature packages (C++ + Luau)
 └─ Source data
    ├─ .scene (küçük seviye / oda / menü)
    │   └─ entity records, prefab instances, Parent refs
    ├─ .world (büyük harita)
    │   ├─ settings, grids, data layers
    │   └─ <stem>.entities/*.ent
    │       ├─ entity / prefab placement
    │       └─ Level Instance (.scene placement)
    └─ .prefab (tek köklü yeniden kullanılabilir entity ağacı)
                         ↓ load / expand / stream
App → Context → World (runtime ECS)
                ├─ Entity(index,generation)
                ├─ Component columns (archetypes)
                └─ Systems / events / commands
                         ↓ bridges
              Jolt / Renderer / Audio / Luau
```

Aynı runtime World'e birden fazla scene yüklenebilir; load_scene additive'dir, otomatik eski scene unload yok. .world open/close MapRuntime tarafından yönetilir; düz scene'ler ile map ownership aynı değildir. `astral.world` adlı save_world serialization formatı, `astral.map` formatındaki .world haritasından ayrıdır.

Kaynak: [scene.hpp](../../modules/world/include/astral/world/scene.hpp), [map.hpp](../../modules/world/include/astral/world/map.hpp), [WorldBridge](../../modules/app/src/services/world_bridge.cpp), [scene.cpp](../../modules/world/src/scene.cpp).

## 2. Dosya türleri

| Tür | Format | Kullanım | Runtime karşılığı |
|---|---|---|---|
| .scene | astral.scene v1 | küçük level, oda, arena, menu | entity kayıtları topluca açılır |
| .prefab | astral.scene v1 | tekrar kullanılan tek köklü nesne ve parçaları | PrefabInstance + PrefabPart |
| .world | astral.map v1 | geniş harita, grids/layers/settings | MapRuntime, MapState, CellStreamer |
| .ent | astral.entity v1 | bir harita yerleştirmesi; entity/prefab/level instance | CellMember, persistent MapEntity::id |
| .cell | astral.cell v1 | cooker tarafından üretilen streaming cell | yüklenir/boşaltılır |
| ECS save | astral.world | save_world / load_world verisi | component serialization; harita manifesti değil |

ModelRef glTF/GLB referansı, PrimitiveMesh built-in şekil verisi, .material/.ashader görsel kaynak; runtime MeshRenderer native mesh/material handle taşır ve dosyaya ham handle yazılmaz.

## 3. .scene: basit arena

Gerçek şablon: [main.scene](../../templates/blank/assets/main.scene). Aşağıdaki küçük türev, prefab dosyası aşağıda tanımlanır.

```json
{
  "format": "astral.scene",
  "version": 1,
  "sun": {"direction": [-0.4, -1, -0.5], "intensity": 1.6},
  "environment": {"ambient": 0.3, "exposure": 0.6},
  "entities": [
    {"id": 1, "components": {
      "astral::Name": {"value": "camera"},
      "astral::Transform": {"position": [0, 1.2, 4.5]},
      "astral::Camera": {}
    }},
    {"id": 2, "components": {
      "astral::Name": {"value": "floor"},
      "astral::Transform": {"position": [0, -0.6, 0], "scale": [12, 1, 12]},
      "astral::PrimitiveMesh": {"shape": "Plane", "material": {"roughness": 0.8}}
    }},
    {"id": 3, "prefab": "prefabs/lamp.prefab",
     "components": {"astral::Transform": {"position": [3, 0, 0]}},
     "parts": {"2": {"components": {"astral::PointLight": {"intensity": 8}}}}}
  ]
}
```

`ctx.scenes().load_scene("levels/arena.scene")` asset_root'a göre yükler. Loader format/ID kontrolü yapar, bütün top-level Entity'leri oluşturup SceneId map kurar, component ve prefab'ları uygular, entity refs resolve eder; LoadResult.errors kontrol edilmeli. Bir sahne otomatik isolated World değildir. Başarısız load partial entity bırakabilir; transactional load geliştirme planında. `save_scene` runtime scene snapshot'ı yazar; gameplay save ile aynı iş değildir. Genel root .scene hot reload'ını her frame garanti etme: [FrameLoop](../../modules/app/src/services/frame_loop.cpp) asset/material/script/prefab/model sync çağırıyor, arbitrary loaded root scene replace yolu bu kaynakta gösterilmiyor.

## 4. .prefab: tekrar kullanım

`assets/prefabs/lamp.prefab`:

```json
{
  "format": "astral.scene",
  "version": 1,
  "entities": [
    {"id": 1, "components": {
      "astral::Name": {"value": "lamp"},
      "astral::Transform": {},
      "astral::PrimitiveMesh": {"shape": "Cube"}
    }},
    {"id": 2, "components": {
      "astral::Name": {"value": "light"},
      "astral::Parent": {"entity": 1},
      "astral::Transform": {"position": [0, 1, 0]},
      "astral::PointLight": {"intensity": 15, "radius": 8}
    }}
  ]
}
```

Prefab exactly one root; children Parent ile bağlanır. Sahnedeki instance 3 kendi position'ını ve light part 2 intensity override'ını saklar; başka instance etkilenmez. Nested prefab ve variant desteklenir; root'un kendisi prefab instance olabilir. Root components / removed / parts / deleted alanları ile farklar. Instance içi document ref `"3/2"`; root ref `3`. `PrefabLibrary` source revisions ile flattened template cache'ler; `sync_prefabs` changed templates için instance parçalarını yeniden üretir ve overrides korur. Part runtime handle'ları reload'da değişebilir.

```cpp
Entity lamp = ctx.scenes().spawn_prefab(
    "prefabs/lamp.prefab", {.position = {3, 0, 0}});
Entity light = find_part(ctx.world, lamp, "light");
```

Spawn error loglanabilir ve root handle dönmesi tam başarılı prefab kanıtı değildir. Handle alındıktan sonra gerekli component/part kontrolü yap. Prefab ownership ile Parent transform relationship aynı kavram değildir.

Gerçek örnek: [pong paddle.prefab](../../examples/pong/assets/prefabs/paddle.prefab) root paddle + top/bottom/left/right children.

## 5. .world ve .ent: streaming harita

```text
assets/
  maps/
    arena.world
    arena.entities/
      1.ent
      2.ent
  levels/
    courtyard.scene
  prefabs/
    lamp.prefab
```

`arena.world`:

```json
{
  "format": "astral.map",
  "version": 1,
  "settings": {},
  "grids": [{"name": "main", "cell_size": 128, "load_range": 384}],
  "layers": [{"name": "props", "active": true}]
}
```

`arena.entities/1.ent`:

```json
{
  "format": "astral.entity",
  "version": 1,
  "grid": "main",
  "layers": ["props"],
  "entity": {
    "id": 1,
    "prefab": "prefabs/lamp.prefab",
    "components": {"astral::Transform": {"position": [200, 0, 20]}}
  }
}
```

`arena.entities/2.ent` level instance:

```json
{
  "format": "astral.entity",
  "version": 1,
  "grid": "main",
  "entity": {
    "id": 2,
    "level_instance": {
      "scene": "levels/courtyard.scene",
      "position": [500, 0, 200],
      "scale": [1, 1, 1]
    }
  }
}
```

Schema source: [map.cpp](../../modules/world/src/map.cpp), [level_instance.cpp](../../modules/world/src/level_instance.cpp). Source map reads .entities folder, expands level instances, assigns cells by ground plane floor(x/cell_size), floor(z/cell_size). Cooked map index plus .cells/*.cell; runtime loose path can construct cells from source. For first example lamp x=200,z=20 → cell (1,0) at 128m. Streaming sources from active StreamingSource, fallback first active Camera. Cell loaded within range of its square; budget and async load. always_loaded:true for persistent game rules/player as needed; empty layers visible, any named layer active visible. .world settings free-form, game must consume its rules; do not assume arbitrary key applies gravity/render automatically.

```cpp
if (!ctx.maps().open_world("maps/arena.world")) {
    ctx.quit();
    return;
}
```

Level Instance differs from prefab: source .scene may have multiple roots; placement expands into map entities with deterministic persistent IDs and world-space transforms, each independently assigned to cell. No artificial transform parent. Supports nested scenes and per-instance overrides/deleted lists, depth/entity limits; .world inside .world not supported. Source edit vs instance override distinction. E1 is present in actual source; old scenes-and-worlds guide saying planned is stale.

MapState tracks Unknown/Unloaded/Alive/Destroyed, load_count and cell diffs for unloaded entities. Runtime references across cells should use persistent MapEntity id/MapEntityRef/CrossCellRef + lookup; raw Entity can go stale after unload/reload. In-memory CellDiff is not automatic permanent save. Floating origin uses double world positions and local float transforms. Read [MapState](../../modules/world/include/astral/world/map_state.hpp) and [ADR-0036](../decisions/0036-world-authoring-and-level-instances.md). A09/AG13/PH03 remain open.

## 6. Entity and hierarchy

Entity runtime handle = index + generation, owned by one World; `World::alive(e)` validates, not merely handle.valid(). WorldEntityRef includes WorldId; `world.owns(ref)` verifies world association. File record id (SceneId), DocumentEntityId (record + prefab part path), MapEntity id and runtime Entity are distinct. Do not serialize index/generation as permanent ID; Name human-readable not unique identity.

Parent is component `Parent{Entity entity}`, not object ownership. Transform is local, WorldTransform calculated by update_transforms; root world=local, child world=parent world * local. Parent missing/dead is ignored; cycle handling exists, authoring should prohibit cycle. Do not write WorldTransform manually. Parent hierarchy doesn't generally imply recursive destroy; prefab part ownership separately managed. RigidBody documented root-only. Character/physics geometry uses desc, don't assume visual scale automatically changes collision.

```cpp
using namespace astral;
Entity player = ctx.world.create(Name{"Player"}, Transform{});
Entity camera = ctx.world.create(
    Name{"Camera"}, Transform{.position = {0, 1.7f, 0}},
    Parent{player}, Camera{});
Entity sword = ctx.world.create(
    Name{"Sword"}, Transform{.position = {0.5f, 1.0f, 0}},
    Parent{player});
```

[Transform](../../modules/world/include/astral/world/transform.hpp), [implementation](../../modules/world/src/transform.cpp), [World](../../modules/world/include/astral/world/world.hpp), [components.hpp](../../modules/app/include/astral/app/components.hpp). Camera looks -Z; Y up. Local rotation quaternion [x,y,z,w]. Transform update occurs after physics/post-update in frame loop; latest WorldTransform not guaranteed in early FixedUpdate (PHR02).

## 7. Component system

Pure data, not GameObject methods or inheritance. Same set of component types → same archetype; SoA columns contiguous, add/remove moves entity to different archetype; storage may relocate on structural changes. Component types must be nothrow move constructible; C06 typed add replacement contract tracked. World stores instances; C++ definitions in game/engine headers, values in JSON/Luau or code. Runtime ComponentId per-type address; not persistent key. TypeRegistry + reflection names provide serialized fields/schema; register BEFORE loading scenes.

```cpp
namespace arena {
struct Health { int value = 100; };
struct Player { float speed = 6.0f; };
struct Enemy {};
}
ASTRAL_REFLECT(arena::Health, value)
ASTRAL_REFLECT(arena::Player, speed)
```

In feature installation use feature.component<T>(); manifest declares ownership. Outside FeatureSet ctx.types.add<T>() for reflection. Plain nonreflected component can be used internally but not automatically exposed in scene/Inspector/Luau. Dynamic Luau components through astral.define_component with supported field types (f32,i32,u32,bool,vec2,vec3,vec4,quat,entity); schema cannot change while running, restart required.

```cpp
Entity e = ctx.world.create(Transform{}, arena::Health{}, arena::Player{});
if (auto* health = ctx.world.get<arena::Health>(e))
    health->value -= 10;
ctx.world.add(e, arena::Enemy{});
ctx.world.remove<arena::Enemy>(e);
ctx.world.each<const arena::Health>([&](Entity ent, const arena::Health& h) {
    if (h.value <= 0) ctx.world.commands().destroy(ent);
});
```

Do not retain component pointers across structural changes. During each/parallel_each, defer create/destroy/add/remove via commands. const read not marked changed, mutable access conservatively changed. Query with/without/changed/added, resources one per type per World, typed EventReader and send. parallel_each only own row, SystemAccess explicit reads/writes or exclusive default; event/resource/service access and nested ticks risks C01/CR01/CR06 remain open. Do not assume full thread safety.

Engine component examples: Name/Transform/Parent, PrimitiveMesh/ModelRef/MeshRenderer, Camera, PointLight/RectLight, RigidBody/CharacterController, Joint/JointMotor, AudioSource, TerrainRef, Atmosphere, VFX. Consult actual registered schema/API rather than guessing serializable fields.

## 8. Gameplay: Game, FeatureSet, systems, events

Repository pattern from [Blank](../../templates/blank/README.md), [Pong](../../examples/pong/src/game.cpp), [AGENTS](../../AGENTS.md):
```text
src/game.cpp
src/game.hpp
src/features/spin/spin.hpp       # component definitions + reflection
src/features/spin/spin.cpp       # install function
assets/features/spin/feature.json
assets/features/spin/spin.luau    # behaviour
assets/main.scene
tests/game_test.cpp
```

Actual Blank feature (abridged):
```cpp
namespace blank {
struct Spin {
    astral::Vec3 axis{0, 1, 0};
    float speed = 1.0f;
};
void install_spin(astral::FeatureContext& feature) {
    feature.component<Spin>();
}
}
ASTRAL_REFLECT(blank::Spin, axis, speed)
```

Reflection must be visible before instantiation: in actual code Spin and reflection live in header, install_spin in .cpp. Use actual files [spin.hpp](../../templates/blank/src/features/spin/spin.hpp), [spin.cpp](../../templates/blank/src/features/spin/spin.cpp) for compile-ready split.

Actual manifest:
```json
{
  "format": "astral.feature",
  "version": 1,
  "name": "spin",
  "description": "Things that turn.",
  "depends_on": [],
  "components": ["blank::Spin"],
  "events": [],
  "exports": {"headers": ["spin.hpp"]},
  "scripts": ["features/spin/spin.luau"],
  "actions": []
}
```
Game on_start:
```cpp
void BlankGame::on_start(astral::Context& ctx) {
    astral::FeatureSet game;
    game.add("spin", install_spin);
    if (!game.install(ctx)) {
        ctx.quit();
        return;
    }
    ctx.scenes().load_scene("main.scene");
}
```

Scene entity component:
```json
"blank::Spin": {"axis": [0, 1, 0.3], "speed": 1.2}
```

Actual Luau system:
```lua
--!strict
astral.system({
    name = "spin",
    phase = "Update",
    query = {"astral::Transform", "blank::Spin"},
    update = function(ctx, _, transform, spin)
        local turn = astral.quat.axis_angle(spin.axis, spin.speed * ctx.delta)
        transform.rotation = astral.quat.normalize(astral.quat.mul(turn, transform.rotation))
    end,
})
```

C++ alternative system for non-physics movement (illustrative, register Phase::Update via ctx.add_system or FeatureContext::system):
```cpp
class PlayerMovement final : public astral::System {
public:
    void update(astral::Context& ctx) override {
        const auto move = ctx.input.wasd();
        ctx.world.each<astral::Transform, const arena::Player>(
            [&](astral::Transform& t, const arena::Player& p) {
                t.position += astral::Vec3{move.x, 0, -move.y} * p.speed * ctx.time.delta;
            });
    }
};
```
For physical characters set CharacterController move/jump or physics service, not direct Transform every frame; fixed authoritative motion and input buffering follow physics contract. Heavy engine math in C++, gameplay rules Luau first, state in components, events decouple features. A feature exports component/event declarations and approved headers; no direct cross-feature implementation calls. FeatureSet topologically sorts dependencies, registers C++ and loads manifest actions/scripts. Current AG06 multiple .actions files overwrite issue: until fixed, use one shared action map per game and don't pretend feature composition is complete. Named action examples [pong.actions](../../examples/pong/assets/features/pong_rules/pong.actions); generate API.md and astral.d.luau from actual game executable with --script-api.

Pong proves intended structure: pong_rules owns Court/Paddle/Ball/Match + Scored/Bounced; Luau Update handles input, FixedUpdate ball movement/scoring, PostUpdate visual depth marker. pong_audio and pong_scorelights C++ consume events; court.scene/prefabs data. Not a full physics-based FPS. No build/run in this audit.

## 9. Frame execution and gameplay guidelines

[FrameLoop](../../modules/app/src/services/frame_loop.cpp):
input route → event age → streaming/origin → asset/material/script/prefab/model sync → PreUpdate → Game::on_update → Update → physics accumulator (0..N FixedUpdate + deferred commands + body sync + physics step) → PostUpdate → atmosphere → world transforms/primitives → audio → render → time advance.
Each scripted phase C++ then Luau; apply deferred commands at boundaries. Physics contacts collected across steps and published by bridge. Use ctx.fixed_delta in Luau FixedUpdate, frame delta for presentation; do not assume current frame pressed() is consumed exactly once by fixed ticks (AGR01) or elapsed deterministic (PH04/AGR02). For now store explicit one-shot requests and consume once, regression-test; full TickInputBuffer planned.

Gameplay example responsibilities:
- Player: input intentions + CharacterController.
- Combat: Health, weapon/cooldown state, Damage/Hit events, fixed systems; melee animation/sweeps and network replication require capability proof.
- Inventory: item ids and quantities in data; pickup events trigger rule; persistent state via validated save.
- Match: rule entity (no Transform required), team, score, timer, win condition.
- HUD/Audio/VFX: read state/events, not source of gameplay truth.
- Scene/world: placement, authored configuration, not game rules. Spawn prefab for runtime entities.

## 10. Choosing scene or world

Small FPS arena, sword arena, football pitch, main menu, puzzle room: start .scene; compose prefab players/weapons/props. Open-world city/island: .world + .ent + reusable level instance .scene. Use multiple separate .scene files for authored levels but transition lifecycle explicitly (load_scene additive; AG13 map switch failure, AG01 save loading defects tracked). Don't introduce .world streaming just because game has multiple menus or levels.

Proposed ARENA example folder structure (not an existing Astral project):
```text
arena/
  CMakeLists.txt
  src/game.cpp
  src/features/{player,combat,match,hud,audio}/
  assets/
    levels/arena.scene
    prefabs/player.prefab
    prefabs/sword.prefab
    features/{player,combat,match,hud,audio}/feature.json
  tests/
```
Start from Blank; register types/features before scene load; one working gameplay vertical slice; add world when streaming is needed. Headless App run_frames and CTest, actual --script-api; package via template's build target. Current Studio project manager/PIE/AI tools are planned; manual code and assets are real workflow. See [writing-a-game](writing-a-game.md) and [GDX](../game-development-experience-roadmap.md).

## 11. Known limitations and verification

This guide does not fix previous findings. C01/C02/C06, PH01–05, AG01–13, A09/A10, AS01–13 etc. affect authoring/runtime reliability. Root scene hot reload not proven by FrameLoop; prefab/material/script reload supported but failure/atomicity issues documented. .world settings aren't automatic universal configuration; Level Instance support exists; source and cooked differences require tests. Examples are source-reviewed, not compiled/played. Engine docs old claims should be updated against current source after fixes; do not mark capability verified until actual test evidence. [MSR](../status/master-stabilization-roadmap.md).
