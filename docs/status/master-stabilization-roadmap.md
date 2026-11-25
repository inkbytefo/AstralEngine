# Master Stabilization Roadmap (MSR)

**25 Kasım 2026 · Plan v1 · Referans taban:** [21a17b6fe217219f37b557e6dcaaf69802a534d1](https://github.com/inkbytefo/AstralEngine/commit/21a17b6fe217219f37b557e6dcaaf69802a534d1)
**Durum:** Planlı, bütün bulgular açık; bu commit düzeltme veya test sonucu değildir.

## Amaç ve kapsam

Yedi denetim raporundaki 75 kaynak kusuru, 38 koşullu risk, 18 geliştirme önerisi, 5 tarihsel teknik borç ve 1 doğrulama engelini tek merkezi iş listesine bağlamak. Toplam 137 takip kaydı, 136 motor/ürün kaydı + V01; 75 kaynak kusuru dışında kalan kayıtlar bug olarak sayılmaz. Statik doğrulama runtime kanıtı değildir. Raporlar ayrıntılı kanıt ve test tasarımlarının kaynağıdır. Bu roadmap önceki [stabilizasyon backlog'unu](master-stabilization-backlog.md) iş sırası açısından genişletir; eski ID'ler ve motor/GDX fazları korunur.

**S0 notu:** Kullanıcı GitHub Actions engelinin hesap/faturalama kaynaklı olduğunu bildirdi; bağımsız log doğrulaması yok. S0 şu an external-blocked, kullanıcı isteğiyle derleme kapsam dışında. Statik planlama ve fix tasarımı devam edebilir; verified-fixed/release kabulü için gerçek test kanıtı daha sonra gerekir. CI failure motor kusuru sayılmaz.

**Editor/AI:** Kullanıcı mevcut implementasyonun ayrıntılı denetimini ve yamalanmasını erteledi. S2/S3 [Studio & Intelligence rebuild](../editor/studio-intelligence-target-architecture.md) üzerinden kabul testlerine taşınır. Önceki kusurlar kapatılmış sayılmaz; motor tarafındaki ortak root resolver, asset, runtime ve RHI düzeltmeleri ertelenmez.

## Revalidation — 25 Kasım 2026, HEAD 6a46d34

Kayıtlar kaynak koda karşı yeniden doğrulandı (statik okuma; build/test bu adımda yapılmadı).

- **Rapor SHA'sından beri değişmeyen modüller:** `gpu`, `render`, `world`, `core`, `assets`, `shader`, `physics`, `script`, `platform`, `audio`. Bu modüllerdeki kayıtların satır referansları hâlâ geçerli; örneklenen kayıtlar (A01, A11, A12, G01, G02, G04–G09, REN02, REN03, REN05–REN08, C02–C04, AS04, AS05, PH09, AG01, AG05, AG07, AG08, AG10–AG12) kaynakta doğrulandı.
- **Değişen modül:** yalnız `modules/app` (6a46d34 refactor). `modules/app` dosyalarına referans veren kayıtlar tek tek kontrol edildi:
  - Zaten düzeltilmiş, regression testi var (`modules/app/tests/refactor_regression_test.cpp`): **AG02** (tmp + rename), **AG03** (raw edge aggregation), **AG04** (tam key name tablosu), **AG06** (action layer composition), **AG13** (doğrulama `close()` öncesi).
  - Zaten düzeltilmiş: **AS12** (`MaterialLibrary::retire_shader` kullanılan shader'ı material rebind edilene kadar tutuyor).
  - Hâlâ geçerli: **A09** (`map_runtime.cpp:199–202`), **A10** (`audio_bridge.cpp:71`), **C01** (ScopedTick yalnız `system_scheduler.cpp:44` üst job'da), **PH03** (`PhysicsBridge::shift_origin` bekleyen komutları taşımıyor), **PH04** (`physics_bridge.cpp:571` `time_.elapsed`), **PH05**, **PH08**, **AS01/AS02** (asıl kusur `gltf.cpp`'de).
- Kalan kayıtlar değişmeyen kodda olduğu için `open` kalır; fix sırasında failing test ile ayrıca kanıtlanır.

## Dalga 1 uygulama durumu — 25 Kasım 2026, branch `fix/msr-revalidation`

Doğrulama: Windows 11, MSVC 19.51 Debug, RTX 3060. `ctest --test-dir build/debug -j4`: 91 testten 84'ü geçti. Kalan 7 `golden_hw_*` hatası master'da (6a46d34) da aynen düşüyor; bu dalgadan bağımsız. Release (`ASTRAL_WARNINGS_AS_ERRORS=ON`), TSan, ASan/UBSan ve Linux/lavapipe golden **çalıştırılmadı**.

- **fixed, local tests:** kaynak düzeltmesi ve `*/tests/msr_regression_test.cpp` (ya da ilgili modül testi) içinde regression testi var, test geçiyor. Testler düzeltmeyle aynı anda yazıldı; düzeltme öncesi kırmızı oldukları koşturularak gösterilmedi. Bazı UB kusurları (AS04, C03, C04 cast'leri, G01) MSVC Debug'da gözlenebilir hata üretmez; bunlar için test sözleşmeyi sabitler, UB'nin yokluğunu UBSan kanıtlar.
- **implemented, no fault test:** düzeltme kaynakta var ama hata yolu (Vulkan create/submit hatası, Lua ref sızıntısı, transient allocation hatası) testte üretilemiyor; fault-injection altyapısı S0/S1.1 kapsamında.

Bu dalgada **açık bırakılanlar** ve nedeni:

- **G03:** bırakılan listenin layout rollback'i, sonradan kayıt yapan listelerin barrier'larını geçersiz kılıyor. Çözüm tek-aktif-kayıt politikası ya da submit-zamanı layout fix-up; public `CommandList` sözleşmesini değiştirdiği için önce ADR gerekiyor.
- **C05:** `Json` sayıları yalnız `double` tutuyor; 2^53 üstü 64-bit tamsayılar kayıplı. Json storage/format değişikliği ve migration gerektiriyor; önce ADR.
- **AG01, AG09, AG10, AS06–AS10, AS13, REN01, REN04, REN09, REN10, PH01–PH06, PH08, G10:** transactional staging, identity/generation, fixed-clock veya cook manifest tasarımı istiyor (roadmap "Ortak kök nedenler" 1–3); sonraki dalgalarda ADR ile.
- **S2/S3 (editor/AI):** kullanıcı kararıyla deferred replacement.

## İş paketleri

| Paket | Kapsam | Ön koşul | Kayıt sayısı | Durum |
|---|---|---|---:|---|
| S0 | Verification infrastructure | Bağımsız | 3 | External-blocked |
| S1.1 | GPU submission / ownership | Bağımsız | 9 | Planned |
| S1.2 | GPU native command and RT safety | S1.1 | 10 | Planned |
| S1.3 | Renderer transactional frame | S1.1 | 11 | Planned |
| S1.4 | Rendering correctness / quality | S1.2, S1.3, S4.1, S4.2 | 6 | Planned |
| S2 | Studio authoring replacement | S4.1, S4.4 | 6 | Deferred replacement |
| S3 | Intelligence replacement | S2 | 6 | Deferred replacement |
| S4.1 | Numeric and typed ABI | Bağımsız | 7 | Planned |
| S4.2 | Parser and data validation | S4.1 | 12 | Planned |
| S4.3 | Transactional asset publication | S1.1, S4.1, S4.2 | 5 | Planned |
| S4.4 | Dependency graph / cook / package | S4.1, S4.2 | 6 | Planned |
| S5.1 | ECS scheduler and lifecycle | Bağımsız | 12 | Planned |
| S5.2 | Physics identity / deterministic step | S5.1, S4.1, S4.2 | 17 | Planned |
| S5.3 | Runtime world/save/audio integrity | S5.1, S4.2, S4.4 | 6 | Planned |
| S5.4 | Gameplay input and feature contract | S5.1 | 10 | Planned |
| S5.5 | Luau lifecycle and reload | S5.1, S4.1 | 8 | Planned |
| S6 | Consumer, docs and server readiness | S4.4, S5.3, S5.4, S5.5 | 3 | Planned |

Bağımlılıklar paket kapanış sırasını ifade eder; independent tests, ADR ve risk araştırmaları önce başlayabilir. S2 headless foundation S4.4 tamamlanmasını beklemeden prototiplenebilir; final asset integration için dependency gerekir. S0 diğer paketlerin geliştirmesini bloke etmez, yalnız kanıt kapısını etkiler. S6 D05 N0 planıyla koordinelidir; N0'ı erken tamamlanmış saymaz.

## Uygulama sırası

**Dalga 1 (temel):** S1.1 GPU lifetime, S4.1 checked numeric/schema, S5.1 ECS scheduler paralel modül sahipliğiyle. Bunlar diğer kusurların kök sözleşmelerini belirler. S0 erişilebilir olunca bağımsız doğrulama.
**Dalga 2 (tüketiciler):** S1.2 native GPU/RT, S1.3 renderer frame, S4.2 parsing, S5.4 input, S5.5 Luau. Çakışan public API/ABI değişiklikleri önce ADR ile uzlaştırılır.
**Dalga 3 (entegrasyon):** S4.3 atomic asset publish, S4.4 dependency/cook, S5.2 physics clock and identity, S1.4 rendering correctness. S5.3 runtime save/world/audio, streaming+physics ownership ile birlikte. S6 consumer/docs.
**Dalga 4 (sonraki ürün çalışması):** S2/S3 replacement editor/AI, GDX E3–E10 ve TRIALS. Mevcut editor'u silmeden yeni authoring core ve tests; cutover yalnız kabul sonrası. Optimizations ayrı measured tasks, P1 doğruluğun önüne geçmez.

## Paket uygulama ve kabul kapıları

### S0 — Verification infrastructure

- Kayıtlar: `V01`, `D01`, `D02`.
- Çalışma: CI account/infrastructure user-reported; do not block static work on billing; restore local/CI verification when available. Historical golden/sanitizer baseline remains open.
- Kabul: Exact SHA/toolchain, real logs, debug/release/TSan/ASan/UBSan and golden baseline; no disabled tests.

### S1.1 — GPU submission / ownership

- Kayıtlar: `A01`, `G01`, `G02`, `G03`, `G07`, `G08`, `G09`, `GR04`, `GP01`.
- Çalışma: Explicit result, command lifecycle, recording dependencies, resource retirement, WSI and device ownership. Decide single active recording vs tracked multi-list in ADR.
- Kabul: Fault-injected submit/create/drop/resize; no stale history, double release or premature resource retire; Vulkan validation.

### S1.2 — GPU native command and RT safety

- Kayıtlar: `G04`, `G05`, `G06`, `G10`, `GR01`, `GR02`, `GR03`, `GR05`, `GP02`, `GP03`.
- Çalışma: RAII AS backing resources, copy footprint/range, capability and stage checks. Upload/cache optimization only after profiling.
- Kabul: BLAS/TLAS lifecycle, BC copies, queries, descriptor limit fixtures, stage validation.

### S1.3 — Renderer transactional frame

- Kayıtlar: `REN01`, `REN02`, `REN03`, `REN04`, `REN09`, `RR01`, `RR03`, `RR04`, `RR06`, `RP01`, `RP02`.
- Çalışma: Record vs committed history, atomic allocation, render graph resource validation, frame ring and per-view identity.
- Kabul: Dropped/failed frame, allocation retry, independent view histories, GPU timing provenance.

### S1.4 — Rendering correctness / quality

- Kayıtlar: `REN06`, `REN07`, `REN10`, `RR02`, `RR05`, `RP03`.
- Çalışma: Mip area filter, denoiser and DDGI limits, M14 primary depth/normal, HDR particle path review.
- Kabul: Odd texture, bounded dispatch, SDF particle parity, PBR/temporal/fog reference scenes; measured budgets.

### S2 — Studio authoring replacement

- Kayıtlar: `A02`, `A03`, `A04`, `A06`, `A13`, `A14`.
- Çalışma: Defer current editor patching per user; implement new headless journal, overlay, document lifecycle per Studio target architecture. R0/R1/R2 contracts first; headless portions can start independently.
- Kabul: Fault injection/recovery, undo all assets, dirty switch, preview/live/play same revision.

### S3 — Intelligence replacement

- Kayıtlar: `A05`, `A07`, `A08`, `R01`, `R02`, `R03`.
- Çalışma: New orchestrator and same command service; subset review, nonblocking process, root containment, permissions. R02 shared with S4.4; engine-side resolver not deferred.
- Kabul: Exact digest review, cancellation, offline parity, negative permission/path tests. No provider bypass.

### S4.1 — Numeric and typed ABI

- Kayıtlar: `C03`, `C04`, `C05`, `AS04`, `AS13`, `ASR04`, `ASP03`.
- Çalışma: Shared checked conversion, lossless integer policy, schema migration, shader typed words and compatibility ADR. Cache optimization separate.
- Kabul: Boundary/roundtrip tests, migration, UBSan, typed uniform CPU/GPU ABI.

### S4.2 — Parser and data validation

- Kayıtlar: `A11`, `A12`, `REN05`, `REN08`, `AS01`, `AS02`, `AS03`, `AS05`, `AS11`, `ASR02`, `ASR05`, `PHR03`.
- Çalışma: Checked binary/JSON/gltf/model/shape inputs, hierarchy, semantic texture variants, budgets, shader limits.
- Kabul: Malformed data rejection before allocation or publish, fuzz/sanitizers, mirrored matrix, role-specific texture channels.

### S4.3 — Transactional asset publication

- Kayıtlar: `AS06`, `AS07`, `AS12`, `ASR06`, `ASP02`.
- Çalışma: Stage and validate model/shader/material generations, preserve last good, retry stale input.
- Kabul: Failure injection, dependency revision race and old resource retention.

### S4.4 — Dependency graph / cook / package

- Kayıtlar: `AS08`, `AS09`, `AS10`, `ASR01`, `ASR03`, `ASP01`.
- Çalışma: Canonical asset identity and root resolver with S3 R02, dependency edges, generated output manifests, transactional cook and release closure.
- Kabul: Shared texture references, deleted cell recook, failed cook preserves last good, clean APAK smoke.

### S5.1 — ECS scheduler and lifecycle

- Kayıtlar: `C01`, `C02`, `C06`, `CR01`, `CR02`, `CR03`, `CR04`, `CR05`, `CR06`, `CP01`, `CP02`, `CP03`.
- Çalışma: ScopedTick propagation, stable event batches, deterministic command producer ordering, component lifecycle, thread-affinity. Optimize only measured bottlenecks.
- Kabul: Serial/parallel equivalence, reentrant events, move lifecycle, tick wrap and TSan.

### S5.2 — Physics identity / deterministic step

- Kayıtlar: `PH01`, `PH02`, `PH03`, `PH04`, `PH05`, `PH06`, `PH07`, `PH08`, `PH09`, `PHR01`, `PHR02`, `PHR04`, `PHR05`, `PHR06`, `PHP01`, `PHP02`, `PHP03`.
- Çalışma: Owner registry, fixed clock, origin epoch, pose snapshots, contact aggregation, joint semantics.
- Kabul: Character query/trigger enter/exit, FPS partition, origin shifts, deterministic setter ordering, hull/capsule, sleep.

### S5.3 — Runtime world/save/audio integrity

- Kayıtlar: `A09`, `A10`, `AG01`, `AG02`, `AG13`, `AGP02`.
- Çalışma: Cell failure retry, voice ownership, atomic saves and staged world transitions; coordinate physics identity for streaming cleanup.
- Kabul: Bad save/world retains prior state, looping voice cleanup, cell retry, interrupted save.

### S5.4 — Gameplay input and feature contract

- Kayıtlar: `AG03`, `AG04`, `AG05`, `AG06`, `AGR01`, `AGR02`, `AGR03`, `AGR05`, `AGR06`, `AGP01`.
- Çalışma: Input transition and tick buffers, feature-owned action maps, SDL lifecycle, explicit runtime errors; coordinate S5.2 simulation clock.
- Kabul: Fast taps, 0/1/N fixed steps, multi-feature maps, gamepad and feature failure tests.

### S5.5 — Luau lifecycle and reload

- Kayıtlar: `AG07`, `AG08`, `AG09`, `AG10`, `AG11`, `AG12`, `AGR04`, `AGP03`.
- Çalışma: Immutable callback registration, registry ownership, staged reload, behaviour instance state, bounded conversion/timers.
- Kabul: Failing reload no world mutation, callback registration safe, cyclic table rejected, timers/ref cleanup.

### S6 — Consumer, docs and server readiness

- Kayıtlar: `D03`, `D04`, `D05`.
- Çalışma: Installed consumer script type check, capability/version drift, N0 dedicated server dependency contract.
- Kabul: Consumer compile/package, actual type check, truthful capability matrix; server build tracked in N0.


## Ortak kök nedenler ve ADR kararları

1. Explicit operation results ve prepare/commit/retire: GPU, renderer, assets, world, save, authoring. Domain-specific ownership; tek dev global transaction yazma.
2. Stable identity ve generation: ECS handles vs document/map ids, GPU owner and submission, physics native BodyID, asset revisions, script behaviour instances. Persisted IDs ile ephemeral handles karıştırılmaz.
3. Fixed clock ve deterministic event/command ordering: C01/C02/CR04, PH03–05, AGR01–02. Frame vs simulation vs presentation ayrımı; replay testleri.
4. Checked numeric conversions, budgets, immutable schema: C03–05/AS04/AS13, binary, shader, geometry ve Lua inputs. Format değişikliği migration ve compatible reader ile.
5. Failure isolation: old-good state, staged candidate, stale generation rejection, retry diagnostics; S1/S4/S5/S2/S3 ortak kabul prensibi.
6. Toolchain verification: compile/test/runtime evidence, no invented speedup, no silent golden update, no feature claims without capability proof.

ADR önce: submission/record state, numeric+shader ABI migration, scheduler/event/access domain, simulation clock/pose/physics owner, asset identity/cook manifest, save/world transaction, Studio rebuild contracts. Bir ADR birden fazla pakete referans verebilir, ancak implementasyonu küçük ilişkili commit'lere böl.

## Release / phase gates

- P1 kusurlar: önce regression design, ardından failing test, fix, passing test; ortam yoksa pending verification. P1 risk: koşulu doğrula veya kanıtla reddet, gerekli fix ve test. P2/P3 planlı; defer rationale and residual risk.
- Başlamadan önce current master ile pinned finding source'larını karşılaştır. Kod değiştiyse finding'i revalidate et; kayıp satır referansı otomatik fix sayılmaz.
- Fix PR: ID, root cause, changed contract, compatibility, regression, test commands/outputs, SHA, platform, review. Status open → in progress → implemented/unverified → verified fixed; risk additionally disproved/deferred with rationale. Source-only change verified fixed değildir.
- S0 erişildiğinde full Debug/Release, headless fixed tick, installed consumer, golden, Vulkan validation, ASan/UBSan/TSan, packaging ve representative hardware. Önceki CI engeli nedeniyle test sonucu bilinmiyor.
- M21/M23/N0 gibi gelişmiş simülasyon fazları S5.1/S5.2/S5.4 sözleşmelerini; E4/E9 S4.3/S4.4'ü; gerçek PIE S1.1/S1.3/S5.3/S5.5'i; editor cutover S2/S3'ü kullanır. Motor milestone sırası otomatik değiştirilmez, kapsam/bağımlılık ADR ile değerlendirilir.
- Progress counts yalnız doğrulanan kapanışlardan türetilir, yeni findings için ID ekle; öneri/teknik borç/risk sayısını kusurla karıştırma.

## Merkezi kayıt listesi

Aşağıdaki 137 satır yedi rapordan ID bazında çıkarıldı. Her kayıt yalnız bir primary pakete atandı (cross-dependency metinde). Ayrıntılı kaynak satırı, risk gerekçesi, öneri ve regresyon tasarımı ilgili rapordadır.

| ID | Tür | Öncelik | Paket | Durum | Konu / kaynak |
|---|---|---|---|---|---|
| V01 | verification | P1 | S0 | external-blocked | [Master CI kırmızı](master-code-audit-2026-11-25.md) |
| D01 | debt | P1 | S0 | open | [F0-TRACK-01](master-code-audit-2026-11-25.md) |
| D02 | debt | P1 | S0 | open | [F0-TRACK-02: ASan CI ve Linux TSan kanıtı](master-code-audit-2026-11-25.md) |
| A01 | defect | P1 | S1.1 | implemented, no fault test | [Başarısız GPU submit başarı gibi yorumlanıyor](master-code-audit-2026-11-25.md) |
| G01 | defect | P1 | S1.1 | fixed, local tests | [CommandList move assignment nesne yaşam süresini ihlal ediyor](gpu-rhi-deep-audit-2026-11-25.md) |
| G02 | defect | P1 | S1.1 | fixed, local tests | [Birden fazla kayıtlı command list için retirement ticket yetersiz](gpu-rhi-deep-audit-2026-11-25.md) |
| G03 | defect | P1 | S1.1 | open | [Önceki kayıt iptal edilince sonraki listelerin layout varsayımları geçersiz kalıyor](gpu-rhi-deep-audit-2026-11-25.md) |
| G07 | defect | P1 | S1.1 | implemented, no fault test | [Swapchain recreate failure eski retired swapchain'i yeniden kullanabiliyor](gpu-rhi-deep-audit-2026-11-25.md) |
| G08 | defect | P1 | S1.1 | implemented, no fault test | [Image view allocation başarısızlığı valid texture olarak yayımlanıyor](gpu-rhi-deep-audit-2026-11-25.md) |
| G09 | defect | P1 | S1.1 | fixed, local tests | [Yanlış Device'a CommandList submit ownership kontrolü yok](gpu-rhi-deep-audit-2026-11-25.md) |
| GR04 | risk | P2 | S1.1 | open | [Handle provenance / device lifetime:](gpu-rhi-deep-audit-2026-11-25.md) |
| GP01 | proposal | P3 | S1.1 | open | [Deferred garbage last-use tracking ve idle/drain;](gpu-rhi-deep-audit-2026-11-25.md) |
| G04 | defect | P1 | S1.2 | implemented, no fault test | [Acceleration structure creation hata yolunda backing buffer double destroy](gpu-rhi-deep-audit-2026-11-25.md) |
| G05 | defect | P1 | S1.2 | fixed, local tests | [BC5/BC7 command copy buffer kapasitesi yanlış kontrol ediliyor](gpu-rhi-deep-audit-2026-11-25.md) |
| G06 | defect | P2 | S1.2 | fixed, local tests | [Timestamp query range toplama taşması](gpu-rhi-deep-audit-2026-11-25.md) |
| G10 | defect | P2 | S1.2 | open | [Bindless kapasitesi per-stage ve toplam descriptor limitlerine göre hesaplanmıyor](gpu-rhi-deep-audit-2026-11-25.md) |
| GR01 | risk | P1 | S1.2 | open | [Shader stage synchronization:](gpu-rhi-deep-audit-2026-11-25.md) |
| GR02 | risk | P2 | S1.2 | open | [Native command validation:](gpu-rhi-deep-audit-2026-11-25.md) |
| GR03 | risk | P2 | S1.2 | open | [Capability feature drift:](gpu-rhi-deep-audit-2026-11-25.md) |
| GR05 | risk | P2 | S1.2 | open | [BLAS/TLAS reference ownership:](gpu-rhi-deep-audit-2026-11-25.md) |
| GP02 | proposal | P3 | S1.2 | open | [Blocking upload/readback](gpu-rhi-deep-audit-2026-11-25.md) |
| GP03 | proposal | P3 | S1.2 | open | [(modules/gpu/src/pipelines.cpp:73–94)(https://github.com/inkbytefo/AstralEngine/blob/b860c8b2dffda9081132d82919fc087f536488d2/modules/gpu/src/pipeline](gpu-rhi-deep-audit-2026-11-25.md) |
| REN01 | defect | P1 | S1.3 | open | [Temporal history submit öncesi ilerletiliyor](rendering-deep-audit-2026-11-25.md) |
| REN02 | defect | P1 | S1.3 | implemented, no fault test | [RenderGraph başarısız transient allocation'ı cache'liyor](rendering-deep-audit-2026-11-25.md) |
| REN03 | defect | P1 | S1.3 | fixed, local tests | [GrowBuffer başarısız büyümede eski buffer'ı kaybediyor ve retry yapmıyor](rendering-deep-audit-2026-11-25.md) |
| REN04 | defect | P1 | S1.3 | open | [History resize başarısızlığında geçersiz texture'lar kalıcı oluyor](rendering-deep-audit-2026-11-25.md) |
| REN09 | defect | P2 | S1.3 | open | [GPU timing ring, submit edilmeyen kareyi pending kabul ediyor](rendering-deep-audit-2026-11-25.md) |
| RR01 | risk | P2 | S1.3 | open | [Debug view değişimi](rendering-deep-audit-2026-11-25.md) |
| RR03 | risk | P2 | S1.3 | open | [ReSTIR history identity](rendering-deep-audit-2026-11-25.md) |
| RR04 | risk | P1 | S1.3 | open | [FrameRing pending kayıt](rendering-deep-audit-2026-11-25.md) |
| RR06 | risk | P2 | S1.3 | open | [Custom RT shader rebuild](rendering-deep-audit-2026-11-25.md) |
| RP01 | proposal | P3 | S1.3 | open | [RenderGraph compile/validate stage](rendering-deep-audit-2026-11-25.md) |
| RP02 | proposal | P3 | S1.3 | open | [Per-view RenderHistory ve quality budget](rendering-deep-audit-2026-11-25.md) |
| REN06 | defect | P2 | S1.4 | fixed, local tests | [Odd-size mip üretiminde kenar texel'leri kayboluyor](rendering-deep-audit-2026-11-25.md) |
| REN07 | defect | P1 | S1.4 | fixed, local tests | [À-trous iteration sınırı ve shift UB](rendering-deep-audit-2026-11-25.md) |
| REN10 | defect | P2 | S1.4 | open | [SDF depth merge particle collision normal seçimini bozuyor](rendering-deep-audit-2026-11-25.md) |
| RR02 | risk | P1 | S1.4 | open | [DDGI grid limits](rendering-deep-audit-2026-11-25.md) |
| RR05 | risk | P2 | S1.4 | open | [M14 particle compositing](rendering-deep-audit-2026-11-25.md) |
| RP03 | proposal | P3 | S1.4 | open | [Görsel ve fiziksel doğrulama programı](rendering-deep-audit-2026-11-25.md) |
| A02 | defect | P1 | S2 | deferred replacement | [Çok dosyalı transaction rollback yeni prefab dosyasını geri almıyor](master-code-audit-2026-11-25.md) |
| A03 | defect | P1 | S2 | deferred replacement | [Undo/Redo prefab değişikliklerini kapsamıyor](master-code-audit-2026-11-25.md) |
| A04 | defect | P1 | S2 | deferred replacement | [Autosave yalnız sahneyi kurtarıyor, staged prefab verisini kaybediyor](master-code-audit-2026-11-25.md) |
| A06 | defect | P1 | S2 | deferred replacement | [ManualSave prefab overlay, Live ve Play yükleyicilerine ulaşmıyor](master-code-audit-2026-11-25.md) |
| A13 | defect | P1 | S2 | deferred replacement | [Belge değiştirme giriş noktası dirty değişiklikleri doğrudan kaybedebiliyor](master-code-audit-2026-11-25.md) |
| A14 | defect | P2 | S2 | deferred replacement | [External conflict, dosya yok veya okunamıyor olduğunda false dönüyor](master-code-audit-2026-11-25.md) |
| A05 | defect | P1 | S3 | deferred replacement | [Seçili AI komutları, seçili alt küme olarak yeniden önizlenmiyor](master-code-audit-2026-11-25.md) |
| A07 | defect | P1 | S3 | deferred replacement | [AI Cancel UI thread'i bloke edebiliyor](master-code-audit-2026-11-25.md) |
| A08 | defect | P2 | S3 | deferred replacement | [Offline agent'ın +1 m taşıma komutu gerçekte mutlak konuma atıyor](master-code-audit-2026-11-25.md) |
| R01 | risk | P1 | S3 | deferred replacement | [AI policy enforcement:](master-code-audit-2026-11-25.md) |
| R02 | risk | P1 | S3 | deferred replacement | [Asset root containment:](master-code-audit-2026-11-25.md) |
| R03 | risk | P2 | S3 | deferred replacement | [Harici provider yaşam döngüsü ve scratch dosyası:](master-code-audit-2026-11-25.md) |
| C03 | defect | P1 | S4.1 | fixed, local tests | [Schema version doğrulamasından önce daraltıcı dönüşüm](core-ecs-deep-audit-2026-11-25.md) |
| C04 | defect | P1 | S4.1 | fixed, local tests | [Typed reflection sayı/enum yüklemesi aralık kontrolü yapmıyor](core-ecs-deep-audit-2026-11-25.md) |
| C05 | defect | P1 | S4.1 | open | [64-bit integer serialization ve reflection clamp tam aralığı korumuyor](core-ecs-deep-audit-2026-11-25.md) |
| AS04 | defect | P1 | S4.1 | fixed, local tests | [Opsiyonel glTF indekslerinde negatif floating-point sentinel unsigned'a cast ediliyor](assets-shader-deep-audit-2026-11-25.md) |
| AS13 | defect | P2 | S4.1 | open | [ashader int uniform float32 üzerinden taşındığı için tam int32 aralığı korunmuyor](assets-shader-deep-audit-2026-11-25.md) |
| ASR04 | risk | P2 | S4.1 | open | [Shader parser grammar/type validation:](assets-shader-deep-audit-2026-11-25.md) |
| ASP03 | proposal | P3 | S4.1 | open | [Shader ABI + build cache](assets-shader-deep-audit-2026-11-25.md) |
| A11 | defect | P1 | S4.2 | fixed, local tests | [Cooked model index bounds kontrolünde unsigned çarpım taşması](master-code-audit-2026-11-25.md) |
| A12 | defect | P1 | S4.2 | fixed, local tests | [AMDL count limit'i dosya boyutu ve makul bellek bütçesine bağlı değil](master-code-audit-2026-11-25.md) |
| REN05 | defect | P1 | S4.2 | fixed, local tests | [Material sampler offset toplama taşması / sınır dışı yazma](rendering-deep-audit-2026-11-25.md) |
| REN08 | defect | P1 | S4.2 | fixed, local tests | [MeshStore index içerik aralığı kontrol edilmiyor](rendering-deep-audit-2026-11-25.md) |
| AS01 | defect | P1 | S4.2 | fixed, local tests | [glTF ve AMDL hiyerarşisinde cycle/multi-parent doğrulaması yok](assets-shader-deep-audit-2026-11-25.md) |
| AS02 | defect | P2 | S4.2 | fixed, local tests | [Matrix ile gelen negatif scale işareti kayboluyor](assets-shader-deep-audit-2026-11-25.md) |
| AS03 | defect | P1 | S4.2 | fixed, local tests | [glTF accessor ve bufferView checked arithmetic eksik](assets-shader-deep-audit-2026-11-25.md) |
| AS05 | defect | P2 | S4.2 | fixed, local tests | [Malformed URI parse hatası public load_gltf error sözleşmesini aşıyor](assets-shader-deep-audit-2026-11-25.md) |
| AS11 | defect | P1 | S4.2 | open | [Aynı lineer image farklı texture kullanım rollerinde yanlış sıkıştırılabiliyor](assets-shader-deep-audit-2026-11-25.md) |
| ASR02 | risk | P1 | S4.2 | open | [Input/output budgets:](assets-shader-deep-audit-2026-11-25.md) |
| ASR05 | risk | P2 | S4.2 | open | [Shader compile limits / diagnostic provenance:](assets-shader-deep-audit-2026-11-25.md) |
| PHR03 | risk | P1 | S4.2 | open | [Cooked ASSH integrity](physics-jolt-deep-audit-2026-11-25.md) |
| AS06 | defect | P1 | S4.3 | open | [GPU upload başarısız model reload'da son çalışan sürüm korunmuyor](assets-shader-deep-audit-2026-11-25.md) |
| AS07 | defect | P2 | S4.3 | open | [Async model hot reload yükleme sırasında gerçekleşen değişikliği kaçırabiliyor](assets-shader-deep-audit-2026-11-25.md) |
| AS12 | defect | P1 | S4.3 | already-fixed (6a46d34) | [Shader hot reload eski pipeline'ı material rebuild başarılı olmadan yok ediyor](assets-shader-deep-audit-2026-11-25.md) |
| ASR06 | risk | P2 | S4.3 | open | [Imported shader cache and sampler parity:](assets-shader-deep-audit-2026-11-25.md) |
| ASP02 | proposal | P3 | S4.3 | open | [Transactional Asset Publishing](assets-shader-deep-audit-2026-11-25.md) |
| AS08 | defect | P1 | S4.4 | open | [Cooker embedded dependency ile bağımsız runtime dependency'yi ayırmıyor](assets-shader-deep-audit-2026-11-25.md) |
| AS09 | defect | P1 | S4.4 | open | [Failed cook çıktıları stale cleanup'ta yanlışlıkla siliyor](assets-shader-deep-audit-2026-11-25.md) |
| AS10 | defect | P1 | S4.4 | open | [Incremental world cook eksik generated cell'leri kontrol etmiyor](assets-shader-deep-audit-2026-11-25.md) |
| ASR01 | risk | P1 | S4.4 | open | [Asset path identity:](assets-shader-deep-audit-2026-11-25.md) |
| ASR03 | risk | P2 | S4.4 | open | [Cook snapshot/cache identity:](assets-shader-deep-audit-2026-11-25.md) |
| ASP01 | proposal | P3 | S4.4 | open | [Asset Registry + typed dependency graph and cook manifest](assets-shader-deep-audit-2026-11-25.md) |
| C01 | defect | P1 | S5.1 | fixed, local tests | [ScopedTick alt parallel_each job'larına taşınmıyor](core-ecs-deep-audit-2026-11-25.md) |
| C02 | defect | P1 | S5.1 | fixed, local tests | [Event callback sırasında aynı event kuyruğuna send, iterator/reference geçerliliğini bozabilir](core-ecs-deep-audit-2026-11-25.md) |
| C06 | defect | P2 | S5.1 | fixed, local tests | [Component move sözleşmesi ile World::add<T> uyumsuz](core-ecs-deep-audit-2026-11-25.md) |
| CR01 | risk | P1 | S5.1 | open | [Event/resource concurrency:](core-ecs-deep-audit-2026-11-25.md) |
| CR02 | risk | P2 | S5.1 | open | [Tick wrap:](core-ecs-deep-audit-2026-11-25.md) |
| CR03 | risk | P2 | S5.1 | open | [Exception safety:](core-ecs-deep-audit-2026-11-25.md) |
| CR04 | risk | P2 | S5.1 | open | [Deferred command order:](core-ecs-deep-audit-2026-11-25.md) |
| CR05 | risk | P2 | S5.1 | open | [EventReader world affinity:](core-ecs-deep-audit-2026-11-25.md) |
| CR06 | risk | P2 | S5.1 | open | [Scheduler service/thread affinity:](core-ecs-deep-audit-2026-11-25.md) |
| CP01 | proposal | P3 | S5.1 | open | [Query plan: `find_query`](core-ecs-deep-audit-2026-11-25.md) |
| CP02 | proposal | P3 | S5.1 | open | [Transform hierarchy](core-ecs-deep-audit-2026-11-25.md) |
| CP03 | proposal | P3 | S5.1 | open | [`JobSystem`](core-ecs-deep-audit-2026-11-25.md) |
| PH01 | defect | P1 | S5.2 | open | [Contact End kimliği karakterlerde ve silinmiş gövdelerde kayboluyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH02 | defect | P1 | S5.2 | open | [Overlap queries karakterleri atlıyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH03 | defect | P1 | S5.2 | open | [Floating origin bekleyen teleport komutlarını yeni koordinat sistemine taşımıyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH04 | defect | P1 | S5.2 | open | [Zaman bağımlı fizik alanları fixed-step yerine render frame zamanını kullanıyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH05 | defect | P1 | S5.2 | open | [Aynı entity'nin paralel noncommutative komutları deterministik sıralanmıyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH06 | defect | P2 | S5.2 | open | [Convex hull ön doğrulaması bazı geçerli nokta dizilerini reddediyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH07 | defect | P2 | S5.2 | fixed, local tests | [Field noise sayısal dönüşüm sınırları korunmuyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH08 | defect | P2 | S5.2 | open | [Değişmemiş joint motor ayarları her fixed step'te gövdeyi yeniden uyandırıyor](physics-jolt-deep-audit-2026-11-25.md) |
| PH09 | defect | P2 | S5.2 | fixed, local tests | [CharacterDesc toplam kapsül yüksekliği sözleşmesi bozulabiliyor](physics-jolt-deep-audit-2026-11-25.md) |
| PHR01 | risk | P1 | S5.2 | open | [Contact subshape ve sleep semantics](physics-jolt-deep-audit-2026-11-25.md) |
| PHR02 | risk | P1 | S5.2 | open | [FixedUpdate ECS snapshot](physics-jolt-deep-audit-2026-11-25.md) |
| PHR04 | risk | P1 | S5.2 | open | [Floating origin world-anchor rest pose](physics-jolt-deep-audit-2026-11-25.md) |
| PHR05 | risk | P1 | S5.2 | open | [Step/input failure policy](physics-jolt-deep-audit-2026-11-25.md) |
| PHR06 | risk | P2 | S5.2 | open | [Registry/constraint lifetime](physics-jolt-deep-audit-2026-11-25.md) |
| PHP01 | proposal | P3 | S5.2 | open | [PhysicsObjectRegistry + contact/query normalization](physics-jolt-deep-audit-2026-11-25.md) |
| PHP02 | proposal | P3 | S5.2 | open | [FixedSimulationClock + pose snapshots](physics-jolt-deep-audit-2026-11-25.md) |
| PHP03 | proposal | P3 | S5.2 | open | [Physics profiling/verification suite](physics-jolt-deep-audit-2026-11-25.md) |
| A09 | defect | P1 | S5.3 | fixed, local tests | [Başarısız cell load, streaming tarafından Attached kabul ediliyor](master-code-audit-2026-11-25.md) |
| A10 | defect | P1 | S5.3 | fixed, local tests | [Entity silinince looping sesin ownership'i kayboluyor](master-code-audit-2026-11-25.md) |
| AG01 | defect | P1 | S5.3 | open | [Save load doğrulamadan önce mevcut entity'leri yok ediyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG02 | defect | P1 | S5.3 | already-fixed (6a46d34) | [Save slot üzerine doğrudan truncate ile yazılıyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG13 | defect | P1 | S5.3 | already-fixed (6a46d34) | [Başarısız world open eski dünyayı önce kapatıyor](app-gameplay-deep-audit-2026-11-25.md) |
| AGP02 | proposal | P3 | S5.3 | open | [Transactional Game State: validated save schema with version migrations, atomic storage/recovery, prepared scene/world transitions, entity reference r](app-gameplay-deep-audit-2026-11-25.md) |
| AG03 | defect | P1 | S5.4 | already-fixed (6a46d34) | [Named button action aynı karede basılıp bırakılan input'u kaybediyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG04 | defect | P2 | S5.4 | already-fixed (6a46d34) | [Key enum genişletilmiş ama action name tablosu eksik](app-gameplay-deep-audit-2026-11-25.md) |
| AG05 | defect | P2 | S5.4 | fixed, local tests | [Desteklenmeyen mouse button Left click'e dönüştürülüyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG06 | defect | P1 | S5.4 | already-fixed (6a46d34) | [FeatureSet her action dosyasında önceki feature action'larını siliyor](app-gameplay-deep-audit-2026-11-25.md) |
| AGR01 | risk | P1 | S5.4 | open | [Fixed input consumption:](app-gameplay-deep-audit-2026-11-25.md) |
| AGR02 | risk | P2 | S5.4 | open | [Fixed script clock:](app-gameplay-deep-audit-2026-11-25.md) |
| AGR03 | risk | P2 | S5.4 | open | [Feature install failure & retry:](app-gameplay-deep-audit-2026-11-25.md) |
| AGR05 | risk | P2 | S5.4 | open | [SDL/gamepad lifecycle:](app-gameplay-deep-audit-2026-11-25.md) |
| AGR06 | risk | P2 | S5.4 | open | [Runtime error propagation:](app-gameplay-deep-audit-2026-11-25.md) |
| AGP01 | proposal | P3 | S5.4 | open | [Gameplay Runtime Contract: frame vs simulation clocks, tick input queue, deterministic event/command boundaries, explicit system and feature ownership](app-gameplay-deep-audit-2026-11-25.md) |
| AG07 | defect | P1 | S5.5 | fixed, local tests | [Luau runtime'da listener/system kaydı aktif iterasyonu geçersizleştirebilir](app-gameplay-deep-audit-2026-11-25.md) |
| AG08 | defect | P2 | S5.5 | implemented, no fault test | [Hatalı registration Lua registry ref'lerini sahiplik kaydından önce bırakıyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG09 | defect | P1 | S5.5 | open | [Failed script reload live world ve registry yan etkilerini geri almıyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG10 | defect | P2 | S5.5 | open | [Behaviour on_start durumu script path/generation yerine yalnız entity ile takip ediliyor](app-gameplay-deep-audit-2026-11-25.md) |
| AG11 | defect | P2 | S5.5 | fixed, local tests | [Aynı timer batch'inde module failure sonrası diğer callback'ler çalışmaya devam edebilir](app-gameplay-deep-audit-2026-11-25.md) |
| AG12 | defect | P1 | S5.5 | fixed, local tests | [Luau cyclic table serialization için recursion guard yok](app-gameplay-deep-audit-2026-11-25.md) |
| AGR04 | risk | P2 | S5.5 | open | [Timer budgets/lifecycle:](app-gameplay-deep-audit-2026-11-25.md) |
| AGP03 | proposal | P3 | S5.5 | open | [Luau Runtime Reliability: module generations, load-only declarations or staged side effects, behaviour instance lifecycle, callback ref ownership, bou](app-gameplay-deep-audit-2026-11-25.md) |
| D03 | debt | P2 | S6 | open | [F0-TRACK-03: standalone template script type check](master-code-audit-2026-11-25.md) |
| D04 | debt | P2 | S6 | open | [F0-TRACK-04: sürüm ve capability drift](master-code-audit-2026-11-25.md) |
| D05 | debt | P2 | S6 | open | [F0-TRACK-05: headless çalışma ile headless derleme farklı](master-code-audit-2026-11-25.md) |

## Kapanış kaydı şablonu

ID / status / finding source SHA / revalidation SHA / fix PR & SHA / ADR / failing and passing test log / platform / residual risk / reviewer. Kanıt yoksa unknown veya pending. Öneriler benchmark sonucu ve karar ile kapatılır, zorunlu rewrite talimatı değildir.
