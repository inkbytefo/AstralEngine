# Proje Yol Haritası

**Sürüm:** 2026-11-26 · **Taban:** `master` `a113b55` · Bu belge eski üç yol haritasının (motor roadmap'i, GDX, Technology Labs) yerini alır; orijinalleri [archive/legacy/](archive/legacy/) altında.

**İşaretler:** ✅ tamamlandı · 🚧 kısmen var · 🔜 sıradaki · ⬜ planlı · ❌ vazgeçildi

Bir şeyin "tamamlandı" sayılması için kodda bulunması, testinin geçmesi ve [status.md](status.md)'de doğrulama kaydının olması gerekir ([process.md](process.md)). Bu belgedeki süre tahminleri, kaynağı yazılı olmadıkça tahmindir, ölçüm değildir.

## 1. Hedef

AstralEngine, C++20 kütüphaneleri ve isteğe bağlı bir `App` katmanından oluşan modüler bir oyun motorudur. Uzun vadeli hedefler:

- **Motor:** açık dünya, fizik tabanlı karakter ve araçlar, yıkım, ağ üzerinden co-op; hibrit ray tracing ile fotogerçekçi render.
- **Oyun yazma deneyimi:** C++ ve Luau ile ECS tabanlı oyun kodu; FeatureSet paketleri; başsız testler; paketleme.
- **Studio ve yapay zekâ:** projeyi açıp düzenleyen, ajan önerilerini fark/önizleme ile onaya sunan masaüstü editör.
- **Technology Labs:** her büyük motor yeteneğini gerçek bir oyun senaryosunda (futbol, sürüş, şehir, rekabetçi FPS…) sınayan oynanabilir örnekler.

Kapsam dışı: VR/XR; ticari oyunların kopyası; "AAA eşdeğeri" iddiası.

## 2. Şu an neredeyiz

Ayrıntı ve kanıtlar: [status.md](status.md).

| Alan | Durum | Kısa not |
|---|---|---|
| Çekirdek, ECS, App kabuğu | ✅ | Paralel sistem zamanlayıcı, deterministik fizik komut sırası, staged script yükleme |
| Render | ✅ / 🚧 | Raster + hibrit RT (DDGI, ReSTIR DI, RT gölge/yansıma, denoiser, TAA) ve path tracer çalışıyor; render scale, SpotLight, DoF yok; RT maliyeti orta sınıf GPU'da yüksek |
| Fizik | ✅ | Jolt v5.7.0; eklemler (SixDOF, motor, kırılma), sorgular, alanlar, karakter |
| Scripting (Luau) | ✅ | Sandbox, hot reload, üretilen tipler, `astral-luau-check` |
| Asset hattı | ✅ / 🚧 | glTF (skin/animasyon yok), cook, `.apak`, SPIR-V önbelleği |
| Ses | 🚧 | Çalma, bus, 3D; filtre, reverb, oklüzyon yok |
| Editör | 🚧 | `examples/world_editor` mevcut; proje yönetimi, gerçek Play, export yok; yeniden yapılanma planlı |
| Animasyon, araç, ağ, navigasyon | ⬜ | Hiç başlanmadı |

## 3. Tamamlananlar

| Faz | İçerik | Kanıt |
|---|---|---|
| M0–M7 | Core, SDL3 platform, Vulkan RHI, ECS, App, render graph ve hibrit RT, Jolt, glTF, sahne/prefab, cook/paketleme, ilk oyun (Pong) | [archive/plans/](archive/plans/), `examples/pong` |
| M8–M12 | Debug araçları, metin/HUD, ses, dünya/harita streaming, shader ve materyal dili, FeatureSet + Luau, fizik alanları ve eklemler | [ADR-0017](decisions/0017-gameplay-scripting.md), [ADR-0022](decisions/0022-shader-and-material-language.md), [ADR-0029](decisions/0029-physics-fields-and-joints.md) |
| M13, M13.5, M13.6 | Gelişmiş PBR, decal, path tracer; modül ayrıştırması ve boyut bütçesi; paralel zamanlayıcı | [ADR-0030](decisions/0030-refactor-rules-and-size-budget.md), [ADR-0032](decisions/0032-scalable-runtime-and-parallel-scheduler.md) |
| F0, F1 | Doğrulama envanteri; gameplay ve fizik adım sözleşmesi | [ADR-0034](decisions/0034-gameplay-api-and-physics-step-contract.md) |
| P0 | Physics API v2 (SixDOF, motorlar, kırılma, batched query, streaming detach) | [ADR-0038](decisions/0038-physics-api-v2.md) |
| E0–E2 | Bağımsız editör çekirdeği, dünya düzenleme ve Level Instance, AI-first editör UX | [ADR-0035](decisions/0035-standalone-ai-assisted-editor-core.md), [ADR-0036](decisions/0036-world-authoring-and-level-instances.md), [ADR-0037](decisions/0037-ai-first-native-editor-v2.md) |
| M15 (temel) | `.aterrain` tile'ları, CPU mesh LOD, heightfield çarpışma, streaming | [ADR-0039](decisions/0039-terrain-foundation.md), `examples/world_showcase` |
| MSR | Master Stabilization: 3 dalgada denetim bulgularının düzeltilmesi (ADR-0040–0044) | [archive/msr/](archive/msr/master-stabilization-roadmap.md), merge `0ebbe13` |
| G1.1 | PAPERWING geri bildiriminden: fizik açısal API + Luau, filtreli sorgular, `ASTRAL_REFLECT` sınır hatası, Atmosphere sanatsal kontrol, particle `LIFE_T`, `AppDesc::input_script`, Ninja derleme havuzu | merge `68dacbd` |
| Açılış performansı | SPIR-V disk önbelleği, paralel shader derleme, hızlı mip üretimi, yükleme sırasında pencere tepkisi; Debug sıcak açılış 19.1 s → 3.95 s | merge `31e2c48` |
| ENGINE_GAPS (kısmi) | Triplanar ±X yön hatası, yansıma pürüzlülük eşiği, path tracer sıfırlama nedeni ve dondurma, `astral-luau-check` kurulumu | merge `31e2c48` |
| Dokümantasyon | Tüm eski dokümanlar arşivlendi, koddan yeniden yazıldı; `docs_check` testi | bu belge |

## 4. Şimdi ve sıradaki (öncelik sırasıyla)

### 4.1 Örnekleri yenileme ✅

Tamamlandı (2026-11-27): 13 eski örnek silindi; yerlerine `physics_showcase`, `lighting_showcase`, `materials_showcase`, `world_showcase`, `vfx_showcase`, `asset_viewer` geldi; `pong` kayıt/gamepad, `hello_headless` girdi senaryosu aldı. Açık kalan: vitrinlerin golden referansları (yalnız CI/lavapipe üretir) ve `paperwing` / `the_architect` örneklerinin nihai API ile yeniden yazılması.

### 4.2 Test ve kalite borcu 🔜

| İş | Neden | Tahmin |
|---|---|---|
| 3 `golden_hw_*` testinin bu makinede düşmesi | Ton farkı (ör. `hello_triangle` köşe RGB −11, SSIM 0.9445 < 0.95). Şüphe: tonemap öncesi gökyüzü/ambient/bloom. İncelenmedi | 1–2 gün |
| `astral_render_tests` DDGI SIGSEGV (paralel yükte, ara sıra) | `modules/render/tests/render_audit_test.cpp` DDGI regresyon testi; seri koşumda geçiyor | 1–2 gün |
| `pong_tests`, `spinning_cubes_tests` Luau 100 ms bütçesi aşımı (paralel yükte) | Bütçe duvar saati ile ölçülüyor; yük altında yanlış alarm | 0.5 gün |
| `AppDesc` ve `App::Impl` 25/25 veri üyesi | Yeni ayar eklenemiyor (shader önbellek yolu bu yüzden ortam değişkeni). Ayarları alt yapılara gruplama | 1 gün |
| Windows'ta tsan/asan yok | Faz kapanış listesindeki tsan maddesi bu makinede doğrulanamıyor; CI (Linux) durumu ayrıca kontrol edilmeli | — |
| Vitrinlerin golden testi yok | Referanslar lavapipe'ta üretilmeli (CI) | 0.5 gün |
| Lavapipe `golden_*` testleri bu GPU'da atlanıyor | Donanımdan bağımsız görüntü doğrulaması yalnız CI'da | — |

### 4.3 Render performansı (ENGINE_GAPS kalan maddeler) 🔜

Ölçüm referansı: `examples/the_architect`, RTX 3060, 1080p. `rt.lighting` varsayılan 10.43 ms; `max_reflection_roughness = 0.35` ile 7.54 ms. Kaynak: rt-perf çalışması (merge `31e2c48`). Tahminler aynı çalışmadan.

| Kod | İş | Beklenen kazanç | Tahmin | Kabul |
|---|---|---|---|---|
| PERF-02 | Render scale / yarım çözünürlük RT: RT geçişleri iç çözünürlükte, TAA veya composite'te upscale, UI doğal çözünürlükte | 0.75 ölçekte `rt.lighting` + denoise ≈ 0.55× | 3–5 gün | Ölçek ayarıyla GPU süresi düşer, UI keskin kalır; golden'lar güncellenir |
| PERF-03 | À-trous denoiser'da shared-memory karo önbelleği (adım 1 ve 2) | ≈ 1 ms | 2 gün | Aynı çıktı (GPU'da bit eşitliği karşılaştırması) |
| PERF-04 | ReSTIR DI öncesi ışık kümeleme (clustered light culling). Eşik ayarı tek başına bir şey kazandırmaz: `restir_di=false` zaten tek kare RIS'e düşüyor | 18 yerel ışıkta temporal+spatial ≤ 3.5 ms hedefi | 4–6 gün | Ölçüm + görüntü testi |
| PERF-05 | Transmission piksellerini ayrı dolaylı dispatch listesine ayırma; ikincil ışınlarda RectLight döngüsünü ön elemeyle kısaltma | p95 sıçramalarının azalması | 4–5 gün | Cam/su ekranda iken p95 − p50 < 1 ms |
| PERF-07 | Path tracer'da ince cam (`thin_walled`) için NEE gölge ışınlarının camdan geçmesi | İç mekân PT gürültüsünde büyük düşüş | 2–3 gün | Referans sahnede yakınsama testi |
| Açılış kalanı | Malzeme başına doku decode + mip'in paralel ön yüklenmesi; DevTools başlatmasının ertelenmesi | Release sıcak açılış 1.76 s'nin altına | 1–2 gün | Ölçüm |
| PERF-06 | Sis için raster gölge haritası yerine RT sorgusu (yalnız sis/duman açıkken çalışıyor, ≈ 0.15 ms) | Küçük | — | Düşük öncelik |

### 4.4 G1 kalanları (PAPERWING geri bildirimi) ⬜

| Kod | İş | Not |
|---|---|---|
| G1.2-7 | `WindZone` / `WaterBody` için döndürülebilir ve şekilli alanlar (kutu/küre/silindir, radyal düşüş, swirl); CPU ve GPU aynı fonksiyon | Bugün eksen hizalı, ölçek yok sayılıyor |
| G1.2-8 | `Airfoil` genelleştirme: basınç merkezi, indüklenmiş ve stall sürüklemesi (G1.1'deki `add_force_at` üzerine) | Varsayılanlar bugünkü sonucu birebir vermeli |
| G1.2-9 | `.vfx` `loop`, public `VfxEmitter` component'i, motor seviyesinde ribbon/trail | Trail için ADR gerekir |
| G1.2-10 | `UiCanvas` ilkelleri (çizgi, yay, yuvarlak köşe, gradyan) ve birden çok font | Luau'dan HUD çizimi için ADR gerekir |
| G1.3-12 | Ses: biquad filtreler, ham PCM'den ses, sentez callback'i | miniaudio node graph |

### 4.5 Fonksiyonel motor eksikleri (ENGINE_GAPS) ⬜

| Kod | İş |
|---|---|
| FUNC-01 | Kamera alan derinliği (DoF); tonemap öncesi post aşaması ve post shader'da derinlik dokusu |
| FUNC-02 | Pencereli modda ekran görüntüsü (bugün `App::read_frame` yalnız offscreen) |
| FUNC-03 | SpotLight (koni, cookie); sis içinde ışık hüzmeleri |
| FUNC-04 | Gerçek zamanlı çok sekmeli ayna yansıması (bugün yalnız path tracer'da) |
| FUNC-06 | Ses reverb bölgeleri ve fizik ışınıyla oklüzyon |
| FUNC-07 | GPU bellek bütçesi sorgusu (`VK_EXT_memory_budget`, VMA istatistikleri) |
| FUNC-09 | Entity başına materyal parametre override'ı (`MaterialOverride`), Luau'dan erişim |

## 5. Motor fazları ⬜

| Faz | Kapsam | Ön koşul | Kabul lab'i |
|---|---|---|---|
| M14 | Atmosfer, hacimsel VFX, GPU partikülleri. **Kısmen var:** gün döngüsü, froxel sis, duman hacmi, GPU partikül simülasyonu, `.vfx` (loop'suz). **Eksik:** loop/trail (G1.2-9), hava durumu bağlantısı | F1, M10, M12–M13 | `vfx_lab` |
| M15 (devam) | Arazi materyal katmanları, delikler, düzenleme araçları, ölçüme göre GPU clipmap | M15 temel ✅ | `world_showcase` |
| M16 | Okyanus ve göller (FFT dalga, kıyı, kaldırma sözleşmesi) | M14–M15, P0 | — |
| M17 | Hacimsel bulutlar, hava durumu, yağış, ıslaklık; ileri aerodinamik | M13–M14 | — |
| M18 | Etkileşimli sığ su, nehir, akış/yüzdürme | M16 | — |
| M19 | Seyrek SDF dünya, çalışma anında CSG, yıkım, hücre deltası | M15, P0 | `destruction_lab` |
| M20 / A0 | Animasyon çekirdeği: skeleton/pose/clip import (bugün glTF skin ve animasyon atlanıyor), GPU skinning, blend, retarget, IK, skinned RT | F1, M5b, M10 | `character_lab` |
| M21 / S1 | Pasif ve aktif ragdoll, kalkma | P0, A0 | `character_lab` |
| M22 | HLOD, bitki örtüsü, GPU-driven culling, meshlet LOD | M15, E1 | `city_lab` |
| M23 / V0 | Rigid araç (Jolt VehicleConstraint adayı) | F1, P0 | `driving_lab` |
| M23 / V1 | Node-beam araç deformasyonu (ayrı solver) | V0, P0 | `crash_lab` |
| M23 / S2 | Kumaş, soft body, saç | P0, M12, A0 | `physics_showcase` |
| M23 | Kırılma entegrasyonu | M19, P0 | `destruction_lab` |
| N0 | Sunucu otoriteli iki oyunculu co-op, dedicated server | F1, M9 | `coop_lab` |

## 6. Geliştirici deneyimi ve Studio (GDX) ⬜

Hedef akış: proje oluştur → sahneyi düzenle (elle veya ajanla) → gameplay yaz → gerçek oyun koduyla Play → HUD/menü → paketle. Bugünkü editör (`examples/world_editor`, `Astral::EditorCore`, `Astral::Editor`) sabit bir çalışma alanıyla açılıyor; proje yönetimi, gerçek Play ve export yok. Kullanıcı kararı: mevcut editör derin denetimi ertelendi, Studio/Intelligence katmanı sıfırdan yeniden yapılandırılacak ([eski hedef mimari](archive/legacy/editor/studio-intelligence-target-architecture.md)).

| Faz | Kapsam | Ön koşul |
|---|---|---|
| E3 | Project Manager, sürümlü proje manifesti, şablondan proje üretimi (CLI ve GUI aynı üretici) | E2 |
| E4 | Asset tarayıcı, import, prefab yönetimi, referans güvenli taşıma/silme | E3 |
| E5 | Gameplay authoring: component/feature/script sihirbazı, Inspector ↔ API eşleşmesi | E3, E4 |
| E6 | AI araç sözleşmesi (sürümlü, izinli, önce salt-okuma; MCP adaptörü), staging ve onay | E3–E5 |
| E7 | Gerçek Play-in-Editor (önce ayrı süreç + IPC prototipi) | E3, E5 |
| E8 | Oyun UI/HUD authoring (G1.2-10 `UiCanvas` ilkelleri buna temel) | E5, E7 |
| E9 | Build/cook/package/export (CLI ve Studio aynı hat) | E3, E4, E7 |
| E10 | İlk bitmiş referans oyun: ASTRAL: TRIALS (fizik bulmaca, 3 oda, menü, save/load, paket) | E3–E9 |

## 7. Technology Labs ve Game API ⬜

Her yetenek: public C++ API + uygun Luau yüzeyi + reflection/cook + başsız test + oynanabilir lab dilimi + ikinci bağımsız tüketici. Oyun kuralları (skor, silah dengesi, trafik kanunu) motor çekirdeğine değil FeatureSet paketlerine gider. Lab isimleri hedeftir; klasörleri yoktur.

| Track | Kapsam | Bağlı faz | Lab |
|---|---|---|---|
| BASE0–1 | Sabit saat, girdi geçişleri, save/world transaction'ları; kamera, etkileşim, isteğe bağlı sağlık/yetenek paketleri, runtime UI | E8 | `competitive_lab` |
| AN0–2 | Animasyon import/blend/skinning; IK, retarget, motion warping; motion matching | M20 | `character_lab`, `football_lab` |
| CH0–1 | Karakter kontrolcü takımı; ragdoll ve fiziksel animasyon | M21 | `character_lab` |
| NAV0–1 | Tiled navmesh (Recast/Detour adayı), async yol; kalabalık, algı, davranış ilkelleri | M9, M15 | `city_lab` |
| NET0–2 | Transport + otoriteli oturum; prediction/reconciliation, replay; ilgi yönetimi, büyük dünya | N0 | `coop_lab`, `competitive_lab`, `battlefield_lab` |
| VEH0–1 / DEF0–1 | Araç dinamiği; node-beam deformasyon; kumaş/soft body | M23 | `driving_lab`, `crash_lab` |
| DEST0–1 | Kırılma, SDF arazi değişimi, ağ üzerinden yıkım | M19, M23 | `destruction_lab` |
| WLD0–1 | Streaming sağlamlaştırma, yol/şerit grafiği, popülasyon | M9, M15, M22 | `city_lab` |
| REND0–2 | Görsel temeller ve bütçeler; HLOD/GPU culling; su/hava | M14–M18, M22 | `world_showcase`, `city_lab` |
| AUD0–1 | Ses streaming, oklüzyon, reverb; HRTF | G1.3-12 | `competitive_lab`, `driving_lab` |

Performans hedefleri (ölçüm değil, tasarım hedefi): rekabetçi profil 144–240 FPS, açık dünya 60 FPS. Referans profil RTX 3060 12 GB; düşük donanım profili henüz seçilmedi.

## 8. Açık kararlar (ADR gerektirir)

| Karar | Ne zaman |
|---|---|
| Hedef platformlar, minimum donanım, kalite ve performans bütçeleri | Lab'ler başlamadan |
| Render scale'in RenderGraph ve history'ye etkisi | PERF-02 öncesi |
| Trail render yolu (raster + RT) | G1.2-9 |
| Luau'dan HUD çizimi mi, HUD component'leri mi | G1.2-10 / E8 |
| Proje manifesti adı ve şeması; Play-in-Editor süreç modeli | E3 / E7 |
| Animasyon kütüphanesi (Ozz adayı), navigasyon (Recast/Detour adayı), transport | A0 / NAV0 / N0 |
| Node-beam ↔ Jolt bağlantısı | V1 |
| `AppDesc` / `App::Impl` ayarlarının gruplanması | 4.2 |
