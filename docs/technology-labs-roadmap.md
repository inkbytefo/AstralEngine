# Astral Technology Labs & Native Game API Roadmap

**25 Kasım 2026 · Plan v1 · Taban:** [ddb097e](https://github.com/inkbytefo/AstralEngine/commit/ddb097e1c3627e9cfc743e1b128f7c6243106cf3)
**Durum:** Kullanıcı kararıyla roadmap'e eklenen geliştirme programı; tüm yeni teslimler planlıdır. Implementasyon veya benchmark sonucu değildir.
**Bağlantılar:** [Motor roadmap](roadmap.md), [MSR](archive/msr/master-stabilization-roadmap.md), [GDX](game-development-experience-roadmap.md), [Game Architecture](guides/game-architecture.md).
**Kapsam dışı:** XR/VR, OpenXR, body tracking ve sanal evren platformu süresiz ertelendi. Aktif görev, bağımlılık, lab, SDK veya kabul bütçesi yok. Ayrı bir kullanıcı kararıyla yeniden ele alınabilir.

## 1. Ürün sözleşmesi

Futbol (FIFA/EA FC esinli), açık dünya (GTA), büyük ölçekli savaş (Battlefield), competitive FPS (CS2), sürüş simülasyonu (Assetto Corsa) ve araç deformasyonu (BeamNG) motorun farklı özelliklerini sınayan özgün reference gameplay projeleridir. Ticari oyunları kopyalamak, onların kaynak koduna erişildiğini varsaymak veya AAA parity iddia etmek hedef değildir. Örnekler özgün/minimal asset'lerle, bağımsız CMake/game target'larıyla geliştirilir.

Her technology increment: public contract + headless unit/integration tests + çalışan lab dilimi + ikinci tüketici veya bağımsız SDK consumer + C++/uygulanabilir Luau API + asset/reflection + cook/package + ölçüm. Lab'de işe yarayan kodu doğrudan çekirdeğe taşımak yerine, genel sözleşme ve iki farklı kullanım ihtiyacı doğrulandıktan sonra promote et. Kullanıcının oyunu örnek proje veya editöre linklenmez.

### Üç seviye

1. **Native Engine:** genel amaçlı matematik, ECS, GPU/render, simülasyon, animasyon, navigation, ağ, input, audio, dünya ve assets. Native C++ sıcak yollar; küçük typed command/query/event API; engine handles, no vendor types in public ABI. Opsiyonel modül derleme ve runtime activation; kullanmayan oyun CPU/GPU simülasyonu veya gereksiz bağımlılık yükü taşımaz.
2. **Astral Game Framework:** mevcut features/ + FeatureSet tabanında isteğe bağlı yeniden kullanılabilir gameplay paketleri. FPS/TPS camera, locomotion policy, interaction, inventory/equipment, health/damage, abilities/cooldown, team/match, generic AI behaviours, vehicle controls, checkpoints, UI widgets. Bunlar önerilen paketlerdir; mevcut fly_camera dışında hazır olduğu varsayılmaz. Shared feature source of truth ve sync_features.py --check korunur; ölçeklenince versioned package distribution ayrı ADR. App bridge'leri native yetenekleri ECS'ye ve script'e bağlar.
3. **Game / Lab:** spor kuralları, skorlama, takım taktikleri, silah dengesi, yarış kuralları, trafik kanunu, şehir ekonomisi, görevler ve özgün içerik. Engine bu domain'leri bilmez; oyun feature'ları kendi state/event/schema sahibi olur.

## 2. Yerleşik yetenek kataloğu

| Yetenek | Native motor sorumluluğu | Game Framework / oyun sınırı | Ana program |
|---|---|---|---|
| ECS, lifecycle, scheduler | World/entity ID, query, resource/event, deterministic producer order, fixed tick, snapshots | Gameplay component ve system tanımları | MSR S5.1–5.5 |
| Character movement | Capsule queries, stairs/slopes, ground state, sweep, moving platforms, physical interaction, pose separation | FPS/TPS hareket ayarları, sprint/crouch/jump policy, kamera davranışı | CH / AN |
| Animation | Skeleton/clip/pose, blend graph, state transitions, root motion, retarget, IK, motion warping/matching, events, pose cache, skinning/motion vectors | Karakter animasyon grafikleri, hareket seti, eylem seçimi | AN / M20–21 |
| Physical animation | Ragdoll, joint motors, balance/controller primitives, blending, collision groups | Düşme, kalkma, hit reactions, karakter tasarımı | CH / M21 |
| Physics | Rigid/character/constraint, shape cooking, contact/trigger/query, CCD, field forces, material properties | Futbol topu/oyun objesi ayarları, hasar kuralları | PH / P0 |
| Navigation | Tiled navmesh, dynamic obstacles/offmesh links, async path requests, local avoidance/crowd, path budgets, streaming invalidation | NPC hedefleri, takım taktiği, yarış çizgisi seçimi | NAV |
| Gameplay AI | Perception queries (vision/hearing/proximity), blackboard/typed state, behaviour tree/state machine primitives, utility scoring, bounded scheduler | NPC kararları, takım stratejisi, trafik sürüş davranışı; no LLM dependency | NAV |
| Networking | Secure transport/session primitives, network identities, RPC/schema, replication/snapshot/delta, relevance, prediction/reconciliation, interpolation, bounded lag compensation, network clock | Game authority, rules, matchmaking policy/service, anti-cheat rules | NET / N0 |
| Replay | Tick input/event capture, snapshot checkpoints, versioned replay, seek, determinism diagnostics | Killcam/sports replay presentation, spectator modes | NET / BASE |
| Vehicle dynamics | Wheel contact, suspension, drivetrain, tire force/slip, surface friction, aero, telemetry, FFB abstraction | Vehicle setup, driver assists/control policies, race rules | VEH / M23 |
| Structural simulation | Node-beam, plastic deformation, breakage, self-collision, render deformation, rigid/soft coupling; separate solver | Vehicle structures/assets and damage policy | DEF / M23 |
| World destruction | Fracture assets, support graphs, chunk activation, SDF terrain edits, collision/nav invalidation, bounded debris and persistent authoritative deltas | Which structures are destructible, gameplay damage mapping | DEST / M19/M23 |
| World streaming | Persistent world identities, cell/source budgets, async load, priority/prefetch, stable floating origin, persistence | Map layout, content placement, population policy | WLD / M9/M15/M22 |
| Roads / crowds | Spline/path geometry, lane graph and connectivity, path/avoidance primitives, simulation LOD and agent scheduling | Traffic laws, signals timing, routes, population/mission rules | WLD / NAV |
| Rendering | GPU-driven draw/visibility, mesh/texture LOD, HLOD, occlusion, indirect, instancing, per-view history, shadows, RT/raster profiles, effects | Artistic content, scene-specific quality presets | REND / M14–22 |
| Asset pipeline | Typed dependency registry, async IO/decode/upload, memory residency budgets, shader cache, atomic publish, package closure | Content, import profiles and authored assets | REND / MSR S4 |
| Input | Device identity, action maps, tick transitions, rebinding, raw mouse, gamepad, wheel/pedal/FFB capability | FPS bindings, driving controls, menus | BASE / VEH |
| Camera | Projection, view management, camera collision query, interpolation and camera blending primitives | FPS/TPS, orbit, chase, spectator, shake presets | BASE / CH |
| Spatial audio | Buses/mixing, streaming, spatialization, occlusion/reverb, material acoustics, voice virtualization, optional HRTF | Sound design, music, dialogue and event routing | AUD |
| UI | Runtime layout, text/font, anchors, focus/nav, data binding, events and render interface | Menus, scoreboards, minimap, HUD widgets | BASE / GDX E8 |
| Save / settings | Versioned atomic storage, entity refs, staged scene/world switch, migration | Progress, checkpoint, inventory and game-specific persistence schema | BASE / MSR S5.3 |
| Diagnostics | CPU/GPU/physics/network/IO counters, trace/replay, allocator/residency metrics, fault injection, benchmark runner | Lab workload and scenario definition | BASE / MSR S0 |

Weapon/equipment/health/interaction/quest/economy rules are not mandatory native core. General physical force, collision and event primitives are native; gameplay semantics remain opt-in features. No universal GameObject inheritance hierarchy, no automatic arbitrary ECS state replication, no mandatory scripting VM on pure native consumers.

## 3. Delivery tracks (new IDs separate from historic M/S/E phases)

All planned. AN0 references M20/A0, CH1 references M21/S1, VEH0/DEF0 reference existing M23/V0/V1, DEST references M19/M23, NET0 references N0. These are cross-cutting work tracks, not duplicate implementations or renamed phases. MSR S1/S4/S5 prerequisites apply to closure of affected tasks, not unrelated prototype starts.

| Track | Deliverables | Dependencies | Primary lab |
|---|---|---|---|
| BASE0 | MSR root contracts, fixed clock, input transitions, save/world transactions, device & headless service boundaries, diagnostics | MSR S1.1/S4.1/S5.1 then S5.3–5.5 as relevant | Blank/Pong/Trials |
| BASE1 | Camera framework, interaction, optional health/ability/equipment/team/match packages, runtime UI primitives | BASE0, GDX E8 UI contract | competitive_lab, football_lab |
| AN0 | M20 skeleton/pose/clip importer+cook, blending, root motion, GPU skinning, motion vectors; raster first, skinned RT after | MSR S4, S1, M20 | character_lab |
| AN1 | IK, retarget, animation graph/markers, motion warping, foot/hand contacts, animation LOD | AN0 | character_lab, football_lab |
| AN2 | Motion matching, searchable pose/trajectory database, compressed clips, performance budget; optional ML research later | AN1 | football_lab |
| CH0 | Controller suite (stairs, slopes, platform, body/pose/intent), first/third person reuse | MSR S5.2, BASE0 | character_lab, competitive_lab |
| CH1 | M21 passive/active ragdoll, physical animation, get-up, joint constraints, optional hit reaction profile | AN0/AN1, CH0, P0 | character_lab, battlefield_lab |
| NAV0 | Recast/Detour candidate: tiled navmesh build/cook, async paths, obstacles, links, cell lifecycle | S4, S5.2, M9/M15 | city_lab |
| NAV1 | Local avoidance/crowds, perception, state/behaviour/utility primitives, budget scheduler | NAV0, BASE0 | city_lab, football_lab |
| NET0 | N0 transport + authoritative session, headless server build, IDs, component schema replication, snapshot/interpolation, net diagnostics | BASE0, MSR S6 consumer portions | coop_lab |
| NET1 | Prediction/reconciliation, bounded history/lag comp, tick input, replay, spectator primitives, packet simulation/security validation | NET0, CH0 | competitive_lab |
| NET2 | Relevance/spatial replication, priority/bandwidth budgets, large-world streaming, vehicles/destruction state, load/bot harness | NET1, WLD0, DEST0/VEH0 for respective features | battlefield_lab |
| VEH0 | Existing M23/V0: Jolt vehicle prototype, suspension/wheel/drivetrain, surface, controls, telemetry | P0, MSR S5.2, BASE0 | driving_lab |
| VEH1 | Tire slip/load/combined forces, temperature/wear optional, aero, calibrated suspension, wheel/pedal/FFB | VEH0 | driving_lab |
| DEF0 | Existing M23/V1: node-beam standalone solver experiment, time-step stability, contact, plasticity, breakage and mesh deformation; coupling ADR | VEH0, P0 | crash_lab |
| DEF1 | M23/S2 cloth/soft body/hair and common breakage events; physics/render LOD, data budgets | P0, AN0 where skeletal binding, DEF0 only when coupling needed | character_lab, crash_lab |
| DEST0 | M19/M23 fracture, structural support, SDF terrain changes, collision/nav deltas and persistence | P0, M15, WLD0; NAV0 for nav updates | destruction_lab |
| DEST1 | Replicated authoritative destruction, chunk/debris LOD, budgets, rebuild/replay | DEST0, NET1 | battlefield_lab |
| WLD0 | M9/M15 streaming hardening, persistent identity, IO priority, origin and active cell budgets | MSR S4.4/S5.2/S5.3 | open_world, city_lab |
| WLD1 | Road/lane graph, splines, population state, simulation LOD, dynamic NPC/vehicle ownership | WLD0, NAV1, VEH0 for traffic | city_lab |
| REND0 | M14/M15 visual foundations and correct allocation/history, device metrics, raster competitive profile | MSR S1 | existing render/terrain labs |
| REND1 | M22 HLOD, GPU culling/indirect, instanced foliage/crowds, mesh/mip residency, async staging/pipeline cache | WLD0, REND0, MSR S4.3 | city_lab, football_lab |
| REND2 | M16–M18 water/weather, effect budgets, large-world views; quality and latency profiles | REND0, M15; REND1 integration | open_world, battlefield_lab |
| AUD0 | Audio streaming, bus/mix, listener/voice lifetimes, occlusion, reverb and acoustic zones | MSR S5.3, BASE0 | competitive_lab, city_lab |
| AUD1 | Optional HRTF, propagation/portal experiments, crowd/engine sound voice virtualization and mixing budget | AUD0, WLD0 | battlefield_lab, driving_lab |

Physical backend selection remains ADR-driven. Ozz (animation), Recast/Detour (navigation), GameNetworkingSockets (transport), Meshoptimizer (geometry) are candidates, not adopted dependencies. Jolt is already pinned v5.7.0 in modules/physics/CMakeLists.txt; its VehicleConstraint is a candidate for VEH0. Evaluate license, version, native API isolation, install/export, headless needs and measurements before integrating. No need for solver rewrite when backend meets contract.

## 4. Technology Labs: incremental playable acceptance

Existing examples (Blank/Pong, physics_lab, open_world, render/terrain demos) extend in place where appropriate. New names below are planned targets, not claims that folders exist. Do not create empty demo directories to mark progress.

| Lab | First playable slice | Next scale / reusable technology | Evidence |
|---|---|---|---|
| character_lab | walk/run/turn, capsule and clip blend | slopes/stairs/platform, retarget/IK, ragdoll/get-up, motion matching | contact error, foot sliding, pose time, skinning cost, transition tests |
| football_lab | 1v1 passing/shooting on .scene | 5v5 then 11v11, shared animation, ball aero/rolling, tactical queries, crowd instances, replay | measured ball trajectories, 22 actor update budget, input and replay tests; football rules remain game-owned |
| competitive_lab | local movement and two-player authoritative match | 1v1 → 2v2 → 5v5, prediction, reconciliation, replay, compact FPS render preset | fixed tick cost, input latency, correction magnitude, RTT/jitter/loss tests, bandwidth; no cheat-proof claims |
| driving_lab | one rigid vehicle, closed track, steering/braking | multiple vehicles, calibrated tires, suspension/aero, wheel/pedal/FFB | braking distance, skidpad, load transfer, lap repeatability, solver CPU, hardware device matrix |
| crash_lab | node/beam sample then deformable car | low/high-speed controlled impacts, plastic deformation, collisions, rigid coupling, solver LOD | convergence, energy/momentum accounting with dissipation, penetration, breakage, time/step, mesh mapping |
| city_lab | small .world district with pedestrians and one traffic route | expanding city, streamed interiors, multiple routes/crowd/vehicles, NPC persist, HLOD | traverse without missing refs, frame spikes, streaming latency, memory high water, NPC budgets; candidate scale 2×2 km before larger |
| destruction_lab | fracture wall/building, rigid chunks | SDF terrain/structural changes, reload persistence, navigation and network delta | collision/render/nav agreement, deterministic state transitions, chunk limits, before/after capture |
| battlefield_lab | small terrain team scenario with destruction | 8 → 16 → 32 → 64 simulated/connected clients as feasible; vehicles, effects, interest management | server p95/p99 tick, per-client bandwidth, destruction state convergence, GPU/CPU budget; scale ladder is target not proof |
| coop_lab | two network clients and GPU-free server | state replication, streaming and reconnect, shared networking API for other labs | loopback and simulated adverse network, server packaging, version/schema mismatch handling |
| TRIALS / existing labs | finished small single-player puzzle | manual and later Studio authoring, package, repeatable regressions | end-to-end play, clean release package and data recovery |

Each lab has reproducible scenario, seed/config, scripted/headless acceptance when feasible, visible play mode, minimal original assets, capture and benchmark runner, no engine private headers. Performance stress scenes separate from playable mode. Second consumer test can be small synthetic installed-SDK app; no requirement to finish another game before feature release. Cross-game examples: AN1 character/football, NET1 competitive/coop, VEH0 driving/city, DEST0 destruction/city, NAV1 city/football, AUD0 competitive/city.

## 5. Game API release contract

Proposed naming, not existing API: ctx.animation(), ctx.characters(), ctx.navigation(), ctx.network(), ctx.vehicles(), ctx.destruction(), ctx.replay(); existing ctx.physics()/maps()/scenes()/assets interfaces extended rather than replaced arbitrarily. Decide exact methods/components in ADR at each slice. Prefer typed handles, immutable snapshots, command queue and explicit Result/diagnostics. All calls document owning thread/phase, units, lifetime, failure, world identity, capability and budget. World coordinate vs floating-origin local and simulation vs presentation pose are explicit.

Example target surfaces:
- Animation: Animator/AnimationGraph/IKTarget, play/blend/parameter/pose query, marker events. Bulk pose evaluation native, script sets intent.
- Navigation: NavAgent, request_path/poll/cancel, reachable/project/query, path result version and streamed tile lifetime. Gameplay AI consumes it.
- Network: NetEntityId and explicit opt-in replicated schema, send_input, authority/relevance, snapshot and connection events; no raw memory serialization or user-driven unchecked authority. Transport private.
- Vehicle: VehicleConfig/WheelState/VehicleInput, set_controls/read_telemetry, grip/gear/contact events. Node-beam shares intent and telemetry abstractions, not identical solver state.
- Destruction: FractureAsset/Destructible, request_damage/force by game rules, committed topology events, collision/nav update and cell delta. Generic damage semantics not forced.
- Camera/audio/replay/input: simple services and optional ECS components; no per-entity script overhead in native inner loops.

C++ public headers installed/exported and native tests; Luau supported high-level subset, generated API.md and astral.d.luau from actual executable, type checks; unavailable/unsupported reported explicitly, not fake stub success. Authorable fields through reflection/versioned formats, defaults, migration, editor integration later. Add format extension only when necessary; proposed names not file format commitment. New public API use examples in guide. Consumer uses SDK components without editor, AI, sample game or third-party headers. Headless server links no GPU/window/audio runtime where applicable; test SDK build graph separately from running headless.

## 6. Performance and correctness budget

Reference RTX 3060 12 GB as one GPU profile, CPU/RAM/OS/compiler/driver/quality/resolution and commit recorded. Choose additional low-end CPU/GPU and headless host later, not fabricated hardware. Design target competitive 144–240 FPS (6.94–4.17ms frame interval), open-world 60 FPS (16.67ms), simulation presentation 60–120 FPS; not measured results. CPU/GPU overlap and total latency measured, not simply sum independent times. Server simulation tick rate separately configured (e.g. 60Hz initial candidate), vehicle solver substeps independent, no promise cross-platform bitwise determinism. Fixed tick/replay with tolerances and snapshot reconciliation.

Measure per-system and p50/p95/p99 CPU/GPU frame, stalls/hitches, jobs/allocations, physics step/active body/constraints, animation pose/skinning, navigation requests/crowd, network bandwidth/correction/tick backlog, RAM/VRAM/residency, IO throughput/latency, audio voices. Report scene complexity, simultaneous entities, duration, warmup, seed and quality. Capability flags/profile switches, feature disabled overhead, budgeted work with fallback, graceful resource pressure and error handling; no blanket GPU migration, custom allocator or work stealing without evidence. Set acceptance thresholds from baseline before implementation, regression comparison same workload; never invent performance results. Use MSR source defect and runtime acceptance separately, no verified fix without logs.

## 7. Execution order and parallel ownership

Wave A: MSR core contracts, BASE0, CH0 and AN0 research/fixture; REND0 and WLD0 work after relevant upstream contracts. Existing physics/render labs and TRIALS provide immediate tests. No delay all gameplay until renderer features finish.
Wave B: AN0/AN1, NAV0, NET0, VEH0 independent modules with explicit dependency ownership; labs start as soon as minimal public slice works. BASE1 and AUD0 incrementally. Public ABI reviews at integration.
Wave C: CH1, AN2, NAV1, NET1, VEH1, REND1, WLD1, DEST0; football/competitive/driving/city slices expand; DEF0 solver prototype can start earlier without shipping dependency.
Wave D: DEF0/DEF1 maturation, DEST1, NET2, REND2/AUD1 integration; crash and battlefield scale ladder, reusable APIs verified in second consumers. Complex simulation budgets determine whether to promote optional features. No XR.
MSR S0 external CI blocker remains as previously recorded, builds not performed in this planning change. Static implementation can proceed but phase closure requires test evidence. Studio/AI rewrite deferred per user, native API/reflection/CLI must not wait for it. Existing M14–M23/N0 and GDX identifiers/statuses remain, no phase falsely completed; known historical M14 docs drift still requires revalidation.

## 8. Definition of done for native capability

- ADR when contracts change; narrow public API, no backend leakage, optional linkage and explicit capability/error reporting.
- Regression and numerical/physics reference tests (including negative inputs, cancellation, lifetime and streaming), headless where applicable.
- First lab playable slice + second independent public consumer, clean build and packaged execution when relevant.
- C++ docs and sample, Luau binding/types if appropriate, serialization/reflection/cook migration and future authoring notes.
- Reproducible performance report, exact SHA/test logs, quality settings and budget; fail/regression rationale and owner.
- Update module docs/capability matrix/roadmap; follow process.md. No success claims until executed.
