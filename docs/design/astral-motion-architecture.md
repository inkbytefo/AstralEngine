# Astral Motion — Hybrid Animation & Character Architecture

**Tarih:** 2026-11-25 · **Taban:** [1567543](https://github.com/inkbytefo/AstralEngine/commit/156754398e90f994d4b2c65fd7cb4c87e1a00ff3) · **Durum:** Önerilen uygulama tasarımı, henüz implementasyon değil.
**İlişkiler:** [ADR-0025](../decisions/0025-physics-based-character-animation.md), [Technology Labs](../technology-labs-roadmap.md), [MSR](../status/master-stabilization-roadmap.md), [ATLAS standardı](atlas-mannequin.md). VR/XR kapsam dışı.

## Amaç

Genel amaçlı hybrid animation: clip playback + procedural adaptation + physical animation. Football, GTA-style city, competitive FPS, sword combat ve battlefield lab'leri aynı API'yi kullanır. Oyun kuralları motor içinde değildir. Hayvan, robot ve mekanik rig'ler humanoid varsayımına zorlanmaz. Mevcut ADR-0025 korunur; çelişen ayrıntılar implementation ADR'siyle çözülür. Mevcut glTF import ve MeshData static mesh temeli, skin/animation/morph için genişletilecek.

## Modül sahipliği ve bağımlılık

| Modül | Sorumluluk |
|---|---|
| Core | math, jobs, allocator, handle primitives |
| Animation (yeni) | Skeleton, Pose, Clip, graph, sampling/blend, root delta, IK, retarget, motion matching; yalnız Core bağımlılığı hedefi |
| Character (yeni) | controller intent, locomotion, fizik/animasyon koordinasyonu, balance, recovery, pose authority |
| Physics | Jolt collider, constraint, query, ragdoll handle/motors; backend private |
| World | ECS identities, component lifecycle, scheduling; bone başına entity yok |
| Assets | glTF skin/clip/morph validation, dependency registry, cook/compression, immutable revisions |
| Render | palette buffers, CPU reference / vertex / compute skinning, morph, motion vectors, shadow, skinned BLAS; no gameplay state |
| App | services/bridges, fixed and presentation phase ordering, component registration, asset/runtime lifetime |
| Script | typed high-level Luau intent, markers, pose queries; no VM in hot loop |
| Navigation / Network | movement goals and authoritative action/pose state; no raw pose memory replication |
| Studio (sonra) | graph, rig, timeline, preview, authoring via same API |

Direction: App composition → Character → Animation + Physics; Animation → Core. Assets produces immutable data consumed by Animation/Render; avoid cyclic module references by choosing shared data contract in ADR. Render receives pose snapshots and geometry, not Character logic. Dedicated server no GPU/UI dependency; full presentation animation optional. Ozz sampling/compression/blending and IK candidate, not selected; Astral public types, no Ozz headers. Jolt ragdoll/motors candidate, balance/behaviour Astral-owned.

## Data model / runtime ownership

Skeleton: stable joint ids, ordered parent indices, names, rest local TRS; validate acyclic hierarchy, index and finite values. SkinBinding separately maps mesh palette joints and inverse bind matrices; multiple meshes may share Skeleton with distinct bindings. Rig: semantic bone map, IK chains, constraints, masks and sockets; control bones not required in runtime palette. Clip: immutable tracks, duration, interpolation, marker intervals, root motion metadata. Pose: dense local TRS and derived model-space transforms; instance owns graph state, pose buffers and scratch; generation-aware immutable asset handles. No raw pointer across reload. Canonical engine meters/+Y/-Z, glTF and Blender conversion exactly once, skeleton/mesh/skin spaces explicit.

ECS proposed Animator(graph, parameters, runtime instance), SkinnedMeshRenderer(model/skin), CharacterBody(profile), BoneAttachment(target entity, semantic socket, offset). Transform hierarchy and skeleton hierarchy distinct; exposed sockets only as needed. Simulation attachments versus cosmetic attachments explicit. Hot reload stage all dependent graph/rig/mesh generations, validate compatibility and preserve last-good; MSR S4.3.

## Motion pipeline

Gameplay/AI intent → clip/graph/motion matching → procedural adaptation → character targets → physics → authoritative pose → presentation interpolation/IK → attachments → skinning/render.

Fixed tick (0..N per render frame):
1. consume tick input and gameplay/AI intent, evaluate authoritative animation and marker interval.
2. extract root delta; Character submits desired movement/sweep and pre-physics IK / motor targets.
3. Jolt step; resolve contacts and actual root transform.
4. read physical bodies, reconcile pose and publish tick snapshot + gameplay events.
Presentation: interpolate committed snapshots, visual-only IK/morph/attachments, compute skin matrices and render. Teleport/origin/reload invalidates history as appropriate. Animation LOD never changes authoritative hit/interaction outcomes. Scheduler aligns MSR S5.1/S5.2/S5.4; existing FrameLoop not claimed to implement this.

Modes: in-place (controller owns translation), root-motion (animation requests delta, collision decides applied movement), physical (body owns root), hybrid (explicit blending). Exactly one root transform writer per phase, no double delta. Physics queries supplied to IK as typed target inputs; Animation does not call Jolt. Ragdoll kinematic/active/passive transitions map pose/velocities, limits, sleeping and collision, get-up validation. Joint PD target orientation error and angular velocity with bounded torque/damping; no universal gains, calibrate with fixture.

## Graph and motion algorithms

Compiled typed graph (no per-frame JSON parse or graph allocation): clip player, blend 1D/2D, state machine, additive, layer masks, cached pose, inertialization, IK, output. Validate node topology, parameter types, graph dependencies, bounded evaluation and transition/loop semantics; feedback only explicitly defined state. Markers use stable event identity and tick/loop occurrence, blend policy, rollback/seek dedup; critical game events authoritative, cosmetics separate.

IK: analytical two-bone with pole vector, look-at, FABRIK, foot lock/pelvis adjustment, hand grip; full-body IK later. Test unreachable/degenerate targets, limits, convergence and finite output. Retarget semantic mappings, rest orientation and limb scale, preserve contacts. Motion warping bounded by collision and plausible motion. Motion matching after stable baseline: offline feature extraction (pose/velocity/future trajectory/contact), normalized weights, search index, deterministic tie-break, hysteresis, transition continuity and measured dataset quality. No requirement for ML or external inference. Procedural motion and clips composable.

## Asset pipeline and public API

Blender/mocap → glTF/GLB → validated skin/clip import → cook/versioned compressed runtime blobs → APAK. glTF STEP/LINEAR/CUBICSPLINE, normalized weights, joint ranges, inverse bind, animated node transforms, skeleton roots and morph weights tested. LBS first; DQS research optional. CPU skinning numerical reference; vertex shader raster first, compute skinning if reuse/RT warrants; previous palette/morph and world transform for motion vectors; conservative bounds and skinned BLAS build/refit policy measured. GPU allocation/lifetime follows MSR S1. No silent bind-pose fallback for unsupported gameplay animation.

Proposed asset types .skeleton, .anim, .animgraph, .blendspace, .rig, .retarget, .motiondb, .ragdoll, .muscle, .behaviour; not committed extensions, prefer container where appropriate. Source/cooked schema migration and content hash, dependency closure, diagnostic provenance. 
Proposed API (not current): ctx.animation().set_float/set_bool/trigger/set_ik_target, play/blend/pose_query, marker events; ctx.characters() for move/physical profile. Luau astral.animation equivalents high-level. Validate handle/world, parameter type, phase/thread, units and errors; no unchecked names in hot loops (resolve to typed IDs). Generated API.md/astral.d.luau, installed SDK tests, reflection and save/cook compatibility. Network: replicate intent, state and selected authoritative transforms; full ragdoll snapshot only when needed, not all bones by default.

## Performance

Per-character batch jobs, SIMD/SoA as measured, shared immutable clips, pose cache, compressed tracks, reusable scratch, no allocation per joint/frame, pose sharing and visibility-aware skinning. LOD0 full, LOD1 reduced IK, LOD2 reduced pose frequency/joints, LOD3 cheap clips/sharing, LOD4 cosmetic evaluation suspended. Importance and gameplay contacts override view LOD. Bound async jobs, palette memory, motion DB query and RT update. Benchmark CPU evaluation, GPU skinning, frame pacing, RAM/VRAM, contact error, foot slip, pose transitions, 22 actors and 100/500/1000 actor stress ladder. Targets only; no measured results. Record SHA, CPU/GPU, resolution, profile, seed, p50/p95/p99. RTX 3060 12GB reference GPU. Do not assume compute wins versus vertex skinning.

## Delivery plan

| Work package | Deliverable | Acceptance |
|---|---|---|
| AN0.1 | skeleton/pose/skin data, CPU math, backend spike | validated hierarchy, bind pose roundtrip, known transforms |
| AN0.2 | glTF import/cook, fixture asset | malformed and valid skin/clip corpus; source/cooked parity |
| AN0.3 | clip sampling/blend/root extraction, minimal graph player, markers | step/linear/cubic, loop, root delta, no duplicate event |
| AN0.4 | raster skinning, shadows, motion vectors, morph path; skinned RT subgate | CPU/GPU parity, animation bounds, history/BLAS lifetime |
| CH0 | controller suite, intent/root contract | stairs/slopes/platform, collision and fixed tick replay |
| AN1.1 | typed graph/state/layers/masks/events | graph cook, transitions and stable event semantics |
| AN1.2 | rig, IK, contacts and sockets | foot/hand error, degenerate targets, two-consumer |
| AN1.3 | retarget/warping/foot lock | rest-pose mapping, collision-respecting root |
| AN1.4 | LOD/compression/jobs/sharing | measured budgets, no authoritative state degradation |
| AN2 | motion matching | dataset licensing, search accuracy/latency, transitions |
| CH1.1 | passive ragdoll & transitions | body/joint map, momentum and lifetime |
| CH1.2 | active ragdoll/physical blending | motor convergence, torque bounds, stable contacts |
| CH1.3 | balance/stumble/recovery behaviours | scripted disturbances, get-up, stable state changes |

AN0 maps M20/A0, AN1/AN2 extend its outcomes, CH1 maps M21/S1. Keep legacy M20 IK and skinned RT exit gates: AN1.2 and AN0.4 RT subgate must pass before M20 closure. CH0 can start in parallel once fixed/physics contracts are stable. MSR S4/S5 and S1 dependencies are closure gates, not reasons to block CPU-only fixtures. No new M numbers.

Labs: character_lab (locomotion/IK/retarget), football_lab (ball contact and 22 actors), sword_combat_lab (root motion/hand grip/fixed collision/physical reaction), animation_stress_lab (scale), competitive_lab (server movement and pose contract). ATLAS is shared optional reference data, not hardwired to engine. Minimal synthetic rig tests do not wait for ATLAS production. Each increment public API + headless test + playable lab + second installed-SDK consumer + measurements. Studio authoring later. No build executed or source code changed in this documentation branch.
