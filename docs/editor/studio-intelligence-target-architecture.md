# Astral Studio & Astral Intelligence — Hedef Mimari / Refaktör Sözleşmesi

**Tarih:** 2026-11-25 · **Taban:** [a29338b](https://github.com/inkbytefo/AstralEngine/commit/a29338bb17d9e275a76895ee692e7a362dd29df5)
**Durum:** Önerilen hedef tasarım. Kullanıcı kararıyla Editor & AI derin bug avı atlandı; mevcut editör daha sonra bu rapora göre yeniden inşa edilecek. Bu belge implementasyon, onaylanmış yeni ADR veya çalışma zamanı doğrulaması değildir.
**İlişkiler:** [GDX E3–E10](../game-development-experience-roadmap.md), [ADR-0035](../decisions/0035-standalone-ai-assisted-editor-core.md), [ADR-0036](../decisions/0036-world-authoring-and-level-instances.md), [ADR-0037](../decisions/0037-ai-first-native-editor-v2.md), [Master audit](../status/master-code-audit-2026-11-25.md), [Stabilization backlog](../status/master-stabilization-backlog.md).

## 1. Ürün ve kapsam

Astral Engine = bağımsız C++20 runtime/SDK. Astral Studio = native proje ve oyun üretim ortamı. Astral Intelligence = opsiyonel, provider-independent authoring ve geliştirme ajanı. Manual-first capability parity; AI-first interaction. İnsan veya AI aynı servisleri kullanır. AI yokken new/open/edit/play/export çalışır. Oyun/dedicated server editor veya agent linklemez; provider SDK engine public header'a sızmaz.

Mevcut editör implementation'ı yeniden yazılabilir; engine ECS, renderer, physics, assets, Luau ve oyun FeatureSet yeniden yazılmaz. E0–E2 kazanımları (GPU-free authoring, identity/revision, transactions, world/level instances, offscreen rendering, review ve offline provider) hedef davranış/compatibility test olarak korunur, eski sınıf şekilleri zorunlu değildir. Eski ADR'ler tarihsel gerçek olarak kalır; değişen kararlar implementasyondan önce superseding ADR ile kaydedilir.

## 2. Bağımlılık ve modül sınırları

Önerilen logical packages (isimler CMake veya fiziksel klasör kararı değil):
- Studio Project: ProjectContext, project manifest, schema/version migration, workspace session, template generator, canonical paths, engine/game target resolution.
- Authoring Core: DocumentStore, revisioned immutable snapshots, persistent identities, CommandRegistry, validation, changesets, transaction coordinator, undo/redo, durable journal. GPU/UI/provider/process bağımsız.
- Authoring Services: scene/world/prefab/terrain/asset/material/feature/UI/build domain adapters. Domain validation engine schema'yı tüketir, kopyalamaz. Headless çalışabilir.
- Studio Presentation: native shell, workspace/panel registry, ViewModels, viewport/input/gizmo/picking, design tokens, focus/advanced; UI doğrudan disk, ECS veya provider mutate etmez.
- Preview/Runtime Bridge: document projection, preview worlds, ViewId history, PlaySession state, IPC and process supervision. Engine interfaces editor-agnostic.
- Intelligence Core: TaskOrchestrator, context builder, tool discovery, structured proposals, review, verification, task store. Authoring Services tüketir; provider adapters ayrı.
- Adapters: offline, Codex CLI, future API/remote providers, external MCP server, CLI, filesystem, process, asset tools; dependency inversion.
- Test/support: fake filesystem/process/provider/clock/GPU-facing interfaces, deterministic fixtures, protocol conformance.

Composition root (AstralStudio executable) → Presentation / Intelligence / Runtime bridge → shared authoring services → authoring core + engine public SDK. No cycles; no engine → studio. MCP transport is an adapter, not the command system. Studio headless CLI must run the same authoring command handlers without SDL/Vulkan. Existing layer/consumer/isolation checks expanded.

## 3. Project and data model

Versioned manifest (name/schema not yet decided; candidate astral.project.json), ProjectId, engine compatibility, relative asset/source paths, start scene/world, game target, feature manifests, build profiles, renderer preset. Repository layout: project manifest, src/, features/, assets/, tests/, CMakeLists.txt, docs/; generated cooked/cache/build, personal layout and secrets excluded. Reopen projects using explicit project root; no hardcoded workshop; one open ProjectContext per session initially; explicit close/switch with dirty Save/Discard/Cancel. Per-project settings vs per-user preferences separate. Engine discovery/version mismatch diagnostics and read-only migration preview.

Stable DocumentId/DocumentEntityId/AssetId and level instance local path separate from ephemeral ECS handles. Revision = content fingerprint + monotonic session epoch, including dependency revisions; external file state Unchanged/Modified/Missing/Inaccessible. Multi-document open and tabs are workspace projections, not independent unsynchronized copies. Existing .scene/.prefab/.world/.ent/.material/.ashader/.actions and Luau/C++ source remain interoperable; schema changes require migration tooling, fixture corpus and rollback. Map/level instance identity, override vs edit-in-context, cross-cell refs and floating origin follow ADR-0036 unless superseded explicitly.

## 4. Single authoring mutation pipeline

Intent (GUI/CLI/AI) → typed CommandRequest → schema and capability check → canonical path and identity resolution → base revision/dependency validation → stage ChangeSet → semantic validation → preview/diff → user decision when required → revalidate exact staged digest + revisions → commit transaction → publish change events → projection updates.

Command metadata: schema version, command id, project id, actor, target document/asset ids, expected revisions, payload, permission class, undoability, idempotency key. ChangeSet lists read/write set, create/update/delete/rename, before/after data and dependency edges, diagnostics, plan provenance. Command handlers use pure transforms where possible and injected services for I/O. GUI gesture coalescing (single drag = single undo), AI selected subset revalidated from base, dependency-closed review; no silently applying omitted commands. Async work never publishes stale results. Clear conflict/rebase/revise flow.

DocumentStore is source of truth; ECS/render/inspector/agent views are projections. ManualSave keeps durable disk and unsaved overlay distinct. One DocumentAssetView resolves source plus overlay for Live/AI Preview/Play, including prefabs and pending assets. Store saved revision and working revision separately. Undo/redo covers every affected file and memory state; new transaction after undo clears redo branch; external changes invalidate unsafe history or explicit rebase. Code/binary changes may use reversible patches with content blobs instead of full copies; never claim undo for external irreversible actions.

Filesystem transaction: stage writes and tombstones, record manifest and old content, journal durable intent, per-file atomic replacement, durable commit marker, crash recovery idempotent roll-forward/rollback. Multi-file rename is not globally atomic; managed readers use committed generation/lock, external tools may see intermediate files. Fault injection at every stage; no success if flush/replace fails. Autosave recovery bundle includes document+asset overlay+base revision, bounded retention, restore preview; no overwrite on recovery without confirmation. Canonical path resolver validates project roots, symlinks/escape, collisions; avoid TOCTOU with platform-appropriate safe open/replace. Never store secrets in journal/audit.

## 5. Studio UX and viewport

Focus default: dominant viewport, compact hierarchy, contextual inspector, command palette, collapsible assistant; Advanced unlocks dockable Assets, World, Terrain, Materials, Gameplay/Script, Console, Profiler, Build, Timeline/Animation when runtime capability exists. Classic restrained premium design, tokenized typography/spacing/colors, no decorative neon. Native C++ shell; existing ImGui docking may be retained provisionally, widget toolkit replacement decided via prototype (DPI, IME, keyboard navigation, docking, accessibility, text editing, performance). Initial single OS window, no second ImGui context; multiwindow only after platform/WSI proof. UI state isolated from document state.

Viewport controller owns camera/tool/selection and input capture, not persistent ECS. Explicit ViewId, render target lifecycle, history and quality budget per view; scene/game/material/AI before-after independent. Offscreen rendering through engine API, RHI submit/lifetime fixes before parallel views. ID picking or CPU fallback with accurate policy; DPI/aspect/picking coordinate tests, gizmo local/world, snap/grid, multi-selection, drag transaction. Thumbnail/preview jobs cancellable and budgeted; no blocking UI I/O or process waits.

## 6. Play / build / game authoring

Preview is not Play. Separate worlds for live authoring projection, proposal preview, true game runtime. Prefer external game process + versioned local IPC first; in-process alternative only after ABI/lifetime/measurement ADR. Play snapshot from unsaved overlay and project revision (source/cooked dependency generation). Real game target loads FeatureSet, Luau, input, physics, audio, UI; not an editor-only simulation. IPC structured logs, diagnostics, resize/input focus, pause/step/resume/stop, capture and profiling; no shared raw pointers, no unsolicited filesystem writes to source. Session state Starting/Running/Paused/Stopping/Exited/Failed, generation IDs, timeouts, crash recovery, process tree cleanup. Stop discards runtime mutations, optional explicit reviewed Apply Runtime Changes later. Project manifest and unsaved data unchanged after crash/stop. C++ rebuild restart, Luau reload per runtime contract. Runtime save data isolated per Play session.

Feature wizard produces real manifest, C++/Luau files, actions, tests; schema and API derived from game executable, not guessed. Asset browser uses engine registry/dependency graph, import diagnostics, reference-safe moves/deletes, transactional reimport; material/ashader reflection shares ABI; cooked output not edited. Build service configure/build/test/cook/package and clean-directory smoke, content/revision/toolchain manifest, progress/cancel, last-good package retention; editor and CLI same service. Game UI authoring separate from editor GUI. E10 TRIALS validates end-to-end.

## 7. Intelligence architecture

One orchestrator, specialized tool profiles (Scene, Gameplay, Asset, QA/Build). Multiple simultaneous model agents not required. Context builder reads project/selection, revisioned scene graph, feature dependency manifests, actual generated API.md/astral.d.luau and code search results, errors/logs/test history. Source references include path/revision; untrusted content treated as data; token/context budgets, secret exclusion and user control. No model-specific state in document.

Task state: Idle → Planning → Executing (read-only/staging) → Review → Committing → Verifying → Complete; alternate Blocked/Failed/Cancelled, explicit retry/revise. Map E2 seven states to this richer internal model; exact UI enum ADR later. Stable TaskId/attempt/generation, cancellable asynchronous jobs, monotonic state transitions and stale completion reject; task cancellation never joins blocking worker on UI thread. Approval binds user to exact changeset hash, selected subset, base revisions and execution scopes. Verification failure shows AppliedButVerificationFailed or rolls back per explicit policy; do not call task successful merely because tool returned success. Human can inspect/modify proposal, accept/reject, undo committed reversible changes. Multiple agents may prepare isolated candidates but commit serialized per project conflict domain.

Provider interface: structured requests/messages, tool calls/results, streaming progress, cancellation, capabilities and diagnostics. Offline provider uses same tool registry, relative transforms based on actual current values. Codex CLI external-process adapter default disabled, no shell-string concatenation, separate working dir/staged copy, process-tree timeout/cancel, limited env, bounded output and logs. API provider optional opt-in; no raw API keys in project, use OS credential store. MCP adapter advertises versioned typed schemas; enforces identical authorization on each tool call. No uncontrolled arbitrary shell or remote execution; source edits from coding agents are staged patches; external commands/network require scoped explicit permission. External provider sandbox strength is platform-dependent; if isolation unavailable, block or clearly present limitations, never label allowlist as OS sandbox.

Tool surface (proposed, not existing API):
- read: project.inspect, scene.inspect, asset.list, feature.inspect, api.discover, diagnostics.list, code.search.
- stage: scene.create_entity, entity.set_component, entity.reparent, prefab.create, asset.import, feature.create, code.propose_patch, material.set_parameter.
- verify: document.validate, script.validate, tests.run, game.capture, profiler.inspect.
- execute: game.build, game.play, game.package (explicit scope, staging or runtime session).
- review/commit: changeset.preview, changeset.validate, human approval gate owned by host, no agent tool to self-approve.

Typed JSON schema input/output, protocol version, structured error codes, expected revisions and tool provenance. Agent has no direct live disk/ECS path around services. Read-only local discovery default; provider transmission consent, network and shell separate permissions; deny by default project-outside reads/writes, secrets, symlink escapes and executable scripts. Tool output/asset/code text may contain prompt injection; never grants authority. Budgets for time, bytes, tool calls, memory and cost; redacted audit log and user-visible provider status. No AI dependency in Play/export.

## 8. Verification and audit mapping

| Prior finding | Required new architecture regression |
|---|---|
| A02 | Fault during multi-file create/update/delete leaves old state or recoverable committed generation; no orphan file. |
| A03 | Scene+prefab+asset undo/redo roundtrip all affected bytes/revisions. |
| A04 | Crash recovery restores unsaved scene and dependent asset overlay together. |
| A05 | Subset approval validates exact dependency-closed command set and exact preview digest. |
| A06 | Live/Preview/Play resolve identical document/asset revision and overlay. |
| A07 | Cancel/timeout leaves UI responsive, process tree cleaned, late result discarded. |
| A08 | Offline relative translate uses current transform and correct world/local semantics. |
| A13 | Dirty document/project close or switch Save/Discard/Cancel, no silent data loss. |
| A14 | Modified/Missing/Inaccessible external source differentiated; conflict prevents destructive write. |
| R01–R03 | Provider/MCP identical policy enforcement, canonical root/symlink tests, process/env/secret isolation. |

All previously reported IDs stay open until implemented and tested; no new bug IDs assigned from design alone. S2/S3 can be satisfied by replacement implementation evidence, not by deleting old code. Engine/runtime defects A/C/G/REN/PH/AS/AG remain independent and must not be hidden by editor rewrite.

## 9. Rebuild order (proposed work packages, not new official GDX numbering)

R0 Baseline/contract: inventory old entry points and supported workflows; freeze fixtures, schema migration and regression tests; decide replacement branch and ADRs. Keep existing editor as reference until cutover. Fix upstream GPU/resource, asset/cook, scheduler and save/runtime P1 contracts where required, parallelize independent headless authoring.
R1 Headless foundation: project/session, document store/identity, resolver, command registry, atomic journal, undo/recovery, tests. No UI.
R2 Domain services: scene/world/prefab/terrain/material/asset/feature commands; shared overlay; CLI; dependency graph and validation.
R3 Studio shell: focus/advanced, inspector/hierarchy/asset browser, offscreen viewport/gizmo, input routing; manual authoring complete.
R4 Runtime: external PlaySession/IPC, real Blank/Pong, build/cook/package, independent game launch; runtime isolation.
R5 Intelligence: offline profile, structured proposal/preview/subset/commit/review, cancellation, provider adapter/MCP, source patches and QA. AI protocol design begins at R1; real Play and AI can develop in parallel after R2.
R6 Vertical slice and cutover: TRIALS scene+feature+UI+save+export, manual and AI equivalent authoring, docs/migration, old editor removal only after acceptance. Preserve current E3–E10 goals, update dependency/order through separate accepted ADR.

## 10. Acceptance gates and non-goals

Headless tests for all authoring commands, deterministic snapshots, failure injection/recovery at each I/O stage, identity/reference migration, stale revisions, selected subset, permission negative tests, malformed input budgets, cancellation and process tree cleanup. Integration: clean project→import→author→save→reopen→Play→Stop→build→package; existing scenes/worlds open without silent data loss; at least 20 Play/Stop cycles plus crash; packaged binary isolated from Studio/AI. Real UI captures and DPI/input tests Windows/Linux; GPU validation and independent game test after environment available. Benchmark startup, viewport latency, import, Play startup, cancel responsiveness, memory and p95 frame time; thresholds decided from measured baseline, not fabricated. No PASS or verified-fixed without logs and SHA. User asked no builds for audit, none performed.

Non-goals: rewrite engine, force cloud/AI subscription, full IDE clone, unapproved autonomous source mutation, plugins/multi-user collaboration in first cut, switch UI toolkit without prototype, unsupported animation/vehicle/networking marketed as implemented. Open ADR decisions: exact module split and toolkit, project schema/identity migration, journal format/durability, IPC transport, provider process isolation per OS, binary/source undo retention, performance budgets. This is the reference architecture for a later implementation, not implementation authorization.
