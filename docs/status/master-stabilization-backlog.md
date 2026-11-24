# Master Stabilization Backlog

**25 Kasım 2026 — açık işler**  
Kaynak ve kanıt: [Master Code Audit](master-code-audit-2026-11-25.md). Bu backlog resmi motor/GDX fazlarının yerine geçmez. Tüm A/R/V kayıtları açık; D kayıtlarında F0'nun güncel kapanış kanıtı aranır.

## Sıralama

| Paket | Kapsam | Çıkış kapısı |
|---|---|---|
| S0 | V01, D01, D02 | İlk kırmızı CI step belirlenmiş, yeniden üretilebilir log; sabit tabanda clean test baseline. |
| S1 | A01 | Açık submit success/failure API, injected hata testi, GPU validation/present/resize. |
| S2 | A02, A03, A04, A06, A13, A14 | Shared journal + asset overlay, atomic recovery, dirty-close ve conflict. Disk/root/overlay/Live/Play eşitliği; fault injection. |
| S3 | A05, A07, A08, R01–R03 | Review alt küme parity, nonblocking cancel, offline transform, izinler, resolver, process lifecycle. |
| S4 | A11, A12 | Checked decode ve asset budget; fuzz ve sanitizer. |
| S5 | A09, A10 | Cell retry/state ve voice ownership, headless integration + runtime smoke. |
| S6 | D03–D05 | Capability/sürüm drift, installed consumer, N0 bağımlılık notu. |

S1, S2, S4, S5 modül sahipliği ayrılarak paralel yürütülebilir; S3'ün authoring bölümü S2 journal sözleşmesine bağlı. S0 önce başlatılmalı; bağımsız ünite testleri S0 altyapı sorunu sürse bile yazılabilir, kapanış için doğrulama gerekir.

## Uygulama ve kabul

- S0: CI metadata değil job logu; exact SHA/toolchain/GPU, runner ve failure origin. CI fail üzerine disable/skip ekleme. F0 ile yeni baseline'ı ayrı kaydet.
- S1: submit başarısını eski ticket ile temsil etme; error result ve WSI semafor recovery contract; test önce failure sonra pass.
- S2: Editor_core tek journal; mevcut manual/immediate policy ve filesystem fault injection. Sahne+prefab transaction, autosave, undo/redo, dirty/project switch. E3 ProjectContext'e bağlanacak root resolver; E3 kapsamı büyütülmeden önce ownership kararı.
- S3: agent proposal dependency validation, seçili diff/preview, process cancellation/timeouts ve izin kontrolleri. Offline ve harici sağlayıcıyı aynı command registry ile doğrula.
- S4: reader kalan byte, checked size, total budget; fuzz seed corpus ve max allocation guard; kötü dosyanın temiz hata dönüşü.
- S5: CellStreamer attach sonucunu taşı; retry/backoff/cancel; AudioBridge voice registry ve generational Entity; streaming unload ve VFX stop testleri.
- S6: API/doc/versiyon ve installed consumer kanıtı; gerçek dedicated server build ayrı N0 kararı.

## Kapanış kayıt şablonu

Her ID için: **durum (open / in progress / verified fixed / deferred)**, kaynak commit, düzeltme commit, failing/passing test adı ve logu, platform, bağımsız review, residual risk ve gerekirse ADR. Önerilen test henüz çalıştırılmadıysa PASS yazılmaz. GDX E3 kapatılmadan önce S2 ve ilgili S3 riskleri kapanmalı veya gerekçeli ertelenmeli; motor sürümü için S0, S1 ve parser/runtime P1 bulguları kritik kabul kapısıdır.

## İkinci denetim (derleme ortamı erişilebilir olduğunda)

1. Tam source checkout, pinned SHA, graphify varsa graphify query; compiler warnings, clang-tidy/clang static analyzer, include/layer/isolation checks. Bütün TODO/FIXME/HACK/XXX comment'lerini grep ile ayır.
2. Debug/Release CTest, ASan/UBSan, TSan, fixed-tick deterministic replay; headless null audio; malformed asset fuzz; clean standalone Blank/Pong.
3. Vulkan validation, swapchain resize/minimize/device lost, RT vs raster, M14 particles/volume/weather-related capability boundaries, lavapipe golden ve donanım görsel doğrulama.
4. Terrain/world streaming pressure, cell failure recovery, asset reload, ECS parallel scheduler and transform ownership; profiling p50/p95/p99, memory, GPU; leak/long-session soak.
5. Editor workflow: dirty project switch, fault injection, save/recovery, AI selected review/cancel, Play/Stop 20 cycle; clean package outside source tree.

## Ek denetim: Core / ECS

[Core & ECS Deep Audit](core-ecs-deep-audit-2026-11-25.md): C01–C06 kaynak kusurları, CR01–CR06 riskler, CP01–CP03 öneriler. C01–C05 yüksek öncelikli çalışma kuyruğuna alınır; mevcut S0–S6 paketleri korunur. CI durumu kullanıcı tarafından altyapı engeli olarak bildirilmiştir; bağımsız kök neden logu yoktur. Yerel derleme bu inceleme kapsamı dışındadır.

## Ek denetim: GPU / Vulkan RHI

[GPU & Vulkan RHI Deep Audit](gpu-rhi-deep-audit-2026-11-25.md): G01–G10 kaynak kusurları, GR01–GR05 riskler, GP01–GP03 öneriler. S1 kapsamı A01, G01–G10 ile genişler; G01–G05 ve G07–G09 P1. A01 immediate submit takip notu yeni bulgu sayılmaz. GPU validation ve fault injection ileride, bu faz sadece statik analiz. Mevcut motor/GDX fazları değişmedi.

## Ek denetim: Rendering

[Rendering Deep Audit](rendering-deep-audit-2026-11-25.md): REN01–REN10 kaynak kusurları, RR01–RR06 koşullu riskler, RP01–RP03 öneriler. REN01–05, REN07–08 P1; öncelik A01/G01–G03 ile ortak submit/resource/history sözleşmesi, sonra input validation ve M14 pass ordering. S1/S4 ve rendering stabilizasyon görevleriyle koordine et; GDX ve motor fazları değişmedi. Statik inceleme, runtime testleri açık.

## Ek denetim: Physics / Jolt

[Physics & Jolt Deep Audit](physics-jolt-deep-audit-2026-11-25.md): PH01–PH09 kaynak kusurları, PHR01–PHR06 koşullu riskler, PHP01–PHP03 öneriler. PH01–05 P1; S5 runtime reliability, C/CR scheduler ve S4 parser hardening ile koordine. Contact/character ownership, fixed-step clock, origin ve deterministic producer order öncelikli; M21/M23 öncesi joint/pose sözleşmesi doğrulansın. Derleme ve runtime testleri kapsam dışı, bütün kayıtlar açık.
