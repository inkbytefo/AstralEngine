# Yol Haritası

Durum: ✅ bitti · 🚧 sürüyor · 🔜 sıradaki · ⬜ planlı

| Faz | Kapsam | Durum |
|---|---|---|
| M0 | İskelet, `Astral::Core` (log, handle, clock, jobs), kurulum, CI | ✅ |
| M1 | `Astral::Platform` (SDL3 pencere/input), `Astral::Gpu` (Vulkan RHI), core math | ✅ |
| M2 | `Astral::World` (kendi ECS'imiz), `Astral::App` (Game + System), basit mesh render | ✅ |
| M3 | `Astral::Render`: render graph, deferred/hibrit mesh + SDF (eski v2'den taşıma) | ✅ |
| M4 | `Astral::Physics` (Jolt) | ✅ |
| M3.5 | Fotogerçekçi render: RT (ray query), DDGI, RT yansıma/gölge, alan ışıkları, denoise, TAA, bloom | ✅ |
| M3.6 | Render API v2: materyal handle'ları, nesne başına hareket vektörü, çalışma anında ayarlar | ✅ |
| M5a | ECS tamamlama: reflection, `without`/`added`/`changed`, olaylar, serileştirme | ✅ |
| M5b | `Astral::Assets`: AssetId, async yükleme, hot-reload, glTF, texture'lı materyal | 🔜 |
| M5c | Sahne + prefab (JSON yazım, ikili çalışma zamanı) | ⬜ |
| M6 | Paketleme: `astral-cook`, `.apak`, `astral_add_game` | ⬜ |
| M7 | İlk oyun dikey dilimi (kılıç arenası prototipi) | ⬜ |

Oyun nasıl yazılır: [ADR-0006](decisions/0006-app-layer-for-games.md).

Sıra ve gerekçe: [ADR-0012](decisions/0012-render-api-contract.md).

**RAGE araştırmasından gelen sistemler** ([research/rage.md](research/rage.md)) — sıra: render (M3.5) →
asset'ler (M5) → kılıç arenası + kendi aktif ragdoll'umuz (M6) → atmosfer (dünya ölçekli bulut/sis/hava
haritaları, fiziksel gökyüzü) → animasyon (parça havuzu / motion matching) → AI (node navigasyonu, rutinler,
hafıza) → dünya streaming + görünmez LOD.

## M0 — İskelet ve Core ✅

**Sonuç:** Modüler CMake sistemi (`astral_add_module`), `Astral::Core` modülü, 25 test,
ThreadSanitizer temiz, `find_package(Astral COMPONENTS Core)` ile kullanılabilir,
GCC/Clang/MSVC + TSan CI. Bağımsız inceleme bir kilitlenme hatası buldu; düzeltildi.
Ayrıntı: [modules/core.md](modules/core.md).

**Devreden borç:** [core.md → Bilinen sınırlar](modules/core.md#bilinen-sınırlar).

## M1 — Platform ve GPU ✅

**Sonuç:** Vulkan 1.3 RHI (handle, bindless, BDA, timeline, gecikmeli silme, swapchain), SDL3
pencere/input, kendi matematik kütüphanemiz, `hello_triangle`. GPU testleri lavapipe ile
headless, pencere+present testi Xvfb ile koşuyor; validation hatası yok. Public header kuralı
artık otomatik test. Ayrıntı: [gpu.md](modules/gpu.md), [platform.md](modules/platform.md).

**Bitiş kriterleri (karşılandı):** `examples/hello_triangle` pencerede üçgen çizer; GPU modülü
pencere olmadan (headless/offscreen) da çalışır ve testleri CI'da yazılımsal Vulkan
(lavapipe) ile koşar; public header'larda Vulkan/SDL tipi yok.

## M2 — Dünya ve App ✅

**Sonuç:** Kendi archetype ECS'imiz (cache dostu SoA, paralel sorgu, ertelenmiş komutlar, hiyerarşi),
instanced forward renderer, `App` + `Game` + `System`; `spinning_cubes` 576 küpü component/system
olarak yazılmış oyun koduyla çiziyor. M1 incelemesinin 1 kritik + 9 önemli, M2 incelemesinin 2 kritik + 3 önemli bulgusu testle düzeltildi;
tüm suite ASan/LSan, TSan ve `-Werror` (GCC/Clang) altında temiz.
Rehber: [writing-a-game.md](guides/writing-a-game.md).

**Bitiş kriterleri (karşılandı):** `examples/spinning_cubes`: oyun kodu yalnızca `Game` + component + system
yazar; ECS sorguları JobSystem ile paralel; system'ler headless test edilir.

## M3 — Render ✅

**Sonuç:** Render graph (otomatik bariyer, havuz, ayıklama) ve üzerinde hibrit deferred renderer:
gölge haritası, mesh G-buffer, SDF raymarch (CSG + yumuşak birleşim), PBR aydınlatma, SDF yumuşak
gölgeleri, nokta ışıklar, ACES. `examples/sdf_scene`. İncelemenin 6 önemli bulgusu testle düzeltildi. Brick bake, TAA, IBL,
cascaded gölgeler sonraki alt fazlara kaldı ([render.md](modules/render.md)).

**Bitiş kriterleri (karşılandı):** render graph pass ekleme/çıkarma mevcut pass'lere dokunmadan yapılır;
eski v2'deki deferred + SDF teknikleri bu mimariye temiz yeniden yazılmış olur.

## M4 — Fizik ✅

**Sonuç:** Jolt üzerine `Astral::Physics` (body'ler, şekiller, sensörler, CCD, raycast, temas olayları,
karakter kontrolcüsü); App'te `RigidBody`/`CharacterController`, sabit 60 Hz adım. `examples/physics_demo`.
İncelemenin 7 önemli bulgusu testle düzeltildi; fizik TSan altında temiz.
Ayrıntı: [physics.md](modules/physics.md).

**Bitiş kriterleri (karşılandı):** `physics_demo` (düşen kutular, raycast); fizik renderer olmadan test edilir.

## M3.5 — Fotogerçekçi render ✅

**Sonuç:** RHI'de ray tracing (BLAS/TLAS, adresle ray query); renderer'da ray-traced yol: RT gölgeli doğrudan
ışık + alan ışıkları, DDGI + yakın alan RTGI, GGX yansımalar, SVGF tarzı denoiser, TAA, bloom
([ADR-0011](decisions/0011-hybrid-ray-traced-lighting.md)). `examples/rt_showcase`. Raster yolu yedek olarak
korunuyor. Shader include bağımlılık hatası bulundu ve düzeltildi.
**Bağımsız inceleme bu fazda henüz yapılmadı** — sonraki adımlardan biri.

![RT showcase](images/m3.5-rt-showcase.png)

**Bitiş kriterleri (karşılandı):** RT destekli cihazda (lavapipe dahil) gölge, alan ışığı, ayna yansıması,
renk taşması ve SDF katılımı testlerle doğrulanır; validation temiz; RT yoksa raster yoluna düşer.

## M3.6 — Render API v2 ✅

**Sonuç:** Kaynak/kare ayrımı netleşti: mesh ve materyal paylaşılan handle'lar, kare verisi saf `RenderScene`.
Materyal tablosu raster ve RT yollarında ortak. G-buffer hız hedefi; App transform geçmişini kendisi tutar.
`set_settings` ile çalışma anında teknik değişimi. 27 render testi (materyal, ayar, hareket dahil).

## M5a — ECS tamamlama ✅

**Sonuç:** ECS yeniden yazılmadı, üstü tamamlandı: değişiklik takibi (satır başına tick, sistem başına referans),
filtreli sorgular, olaylar, reflection ve JSON ile dünya kaydet/yükle (entity referansları yeniden eşlenir).
Core'a bağımlılıksız `Json` ve `ASTRAL_REFLECT`. ThreadSanitizer temiz. Ayrıntı: [world.md](modules/world.md).

**Bitiş kriterleri (karşılandı):** bir dünya JSON'a kaydedilip başka bir dünyaya yüklenir ve yeniden kaydedildiğinde
bayt bayt aynı belgeyi verir; App sistemleri `changed<T>()` ile yalnız kendi görmedikleri değişiklikleri görür.

## M5 — Asset'ler

**Bitiş kriterleri:** glTF sahnesi PBR ile yüklenir; asset'ler handle ile tutulur, diskte değişince
yeniden yüklenir.
