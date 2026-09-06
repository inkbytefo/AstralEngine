# A4 SDF Deferred Quality — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deferred ana render yolunu SDF gölge/AO, ortak geometri tanımı ve değişen yüzeye duyarlı temporal geçmiş yönetimiyle tamamlamak; A5 öncesinde görsel kabulü kapatmak.

**Architecture:** Şekil parametreleri transform'dan ayrılır; CPU ve GPU ortak primitive/CSG çekirdeğinden ve aynı sıralı sahne tanımından beslenir. Sabit yüzey kimlikleri, önceki render snapshot'ı ve sınırlı değişim bölgeleri temporal güvene veri sağlar. Deferred aydınlatmanın gölge/AO değişiklikleri de geçmiş reddine katılır.

**Tech Stack:** Mevcut C++20, GLM, Vulkan compute/GLSL, VMA, CMake ve EngineTests. Yeni çalışma zamanı bağımlılığı veya genel amaçlı shader dili/derleyicisi eklenmez.

**Spec:** Kullanıcının onayladığı beş başlık aşağıdaki “Onaylı kapsam” bölümünde somutlaştırılmıştır. Mevcut temel: `docs/A4_VISUAL_QUALITY.md`.

## Onaylı kapsam ve mevcut durum

1. Deferred ana yol; yönlü/noktasal SDF gölgeleri ve AO eklenir. Forward karşılaştırma yolu olarak korunur.
2. SDF'ye uygun temporal güven: kimlik, derinlik, normal, geometri değişimi ve CSG attribution birlikte değerlendirilir.
3. Değişimin etkilediği bölgelerin geçmişi yerel olarak reddedilir; gölge/AO üzerindeki dolaylı etkiler kapsanır.
4. Şeklin boyutları ile nesnenin TRS transform'u ayrı düzenlenir ve saklanır.
5. Render, gölge, AO, seçim, grid ve fizik aynı primitive/CSG sözleşmesine dayanır.

Kod incelemesinde görülen başlangıç noktaları:

- `AppConfig::useGBuffer` zaten `true`; ana yol kararı yalnızca varsayılan bayrağı değiştirmek değildir.
- CPU `SDFWorldQuery` yerel transform ve doğrudan visibility kullanırken render extraction dünya transform'u ve ebeveyn visibility kullanıyor. Hiyerarşili sahnelerde eşdeğerlik bugün garanti değil.
- CSG sırası transform pool dolaşımından geliyor. Pool elemanı silinmesi/taşınması, non-commutative CSG sonucunu değiştirmemeli.
- `SDFComponent` şekil boyutlarını taşımıyor; `TransformComponent::scale` farklı primitive'lerde farklı anlamlara geliyor.
- Render extraction önceki transform'u güncelliyor. Snapshot hazırlama ile başarılı render sonrası history ilerletme ayrılmalı.
- TAA'da depth rejection ve clamping var; kalıcı yüzey kimliği, normal geçmişi, geometry revision ve yerel değişim maskesi yok.
- Deferred gölge/AO yok. Gölge karşılaştırması için mevcut forward kullanılabilir; iki yolun renklerinin birebir eşit olması beklenmez.

## Global Constraints

- A5 optimizasyonlarına geçmeden bu planın görsel kabul kapısı kapatılır. Süre/bellek ölçümü yapılır; kaliteyi düşüren kestirmeler “optimizasyon” diye sunulmaz.
- Mevcut kullanıcı değişiklikleri korunur; adımlar küçük, bağımsız doğrulanabilir değişiklik grupları olarak uygulanır.
- Mevcut sahneler sessizce yeniden ölçeklenmez veya yeniden sıralanmaz. Eski format desteği ve anlamı açıkça test edilir.
- GPU identity/revision değerleri float veya RGBA8 material indeksine kodlanmaz; integer alanlar kullanılır.
- ECS adresleri GPU'ya/history'ye taşınmaz. Kimlik en az scene instance + generational entity identity kapsamındadır.
- Fixed simulation geçmişi ve son başarıyla render edilen kare geçmişi birbirinden ayrıdır.
- Hot-path sorguları immutable snapshot üzerinden, allocation/registry lookup olmadan çalışır. Snapshot kurucusunun buffer kapasitesi tekrar kullanılır.
- Varsayılan kaliteye ait render geçişleri, kaynak biçimleri ve bellek maliyeti belgelenir. Push constant boyutu Vulkan'ın mevcut 128-byte alt sınırını aşmaz.
- Sürekli buffer upload, yeni descriptor ve görüntüler için ABI, layout transition ve synchronization validation zorunludur.
- Geçersiz/tekil transform için deterministik teşhis ve render/fizikte aynı dışlama kararı uygulanır; NaN üretimine izin verilmez.
- Zaman tahmini benchmark gibi kesin sunulmaz. Her aşamanın çıkış kapısı, sonraki aşamaya geçiş koşuludur.

## Sıralama ve teslimatlar

| Aşama | İş | Ön koşul | İncelenebilir teslimat |
|---|---|---|---|
| P0 | Referanslar ve test altyapısı | Mevcut A4 | Tekrarlanabilir mevcut kalite profili |
| P1 | Şekil/transform ayrımı ve sürümlü veri geçişi | P0 | Ayrı boyut/ölçek kontrolleri; eski sahneler korunur |
| P2 | Ortak kernel, snapshot, CSG sırası/kimliği | P1 | CPU/GPU geometri eşdeğerliği |
| P3 | Deferred gölge ve AO | P2 | Ana yolda tamamlanmış temel aydınlatma |
| P4 | SDF temporal güven | P2 + P3 | Yüzey ve aydınlatma değişimine duyarlı resolve |
| P5 | Yerel değişim takibi | P4 | Oyma/değiştirme için bölgesel history rejection |
| P6 | Editör, debug ve yaşam döngüsü | P3–P5 | İzlenebilir, kullanılabilir bütünleşik sistem |
| P7 | A4 kabul | P6 | Görüntü metrikleri ve hareketli kayıt incelemesi |

P1/P2 önce gelir: yeni temporal sistem eski packed scale anlamlarına ve pool indekslerine bağımlı kurulmayacak. Her aşama sonunda build/test geçer; commit başlıkları aşağıda yalnızca öneridir.

## Ortak arayüz taslağı

Aşağıdaki isimler planlanan yeni arayüzlerdir; mevcut kodda var oldukları varsayılmamalı.

```cpp
// include/Astral/Geometry/SDFShape.hpp
enum class SDFShapeEncoding : uint32_t { LegacyPackedScale, ExplicitShape };
struct SDFShapeParameters {
    glm::vec4 dimensions{1.0f};
};
// Sphere: x=radius; Box: xyz=half extents; Torus: xy=major/minor radius;
// Capsule: xy=radius/segment length (+Y, origin at segment start);
// Cylinder: xy=radius/half height; Plane: x=local Y offset.

// include/Astral/Geometry/SDFSceneSnapshot.hpp
struct SDFSurfaceKey {
    uint64_t sceneInstance;
    uint64_t entityIdentity;
};
struct SDFSample {
    float distance;
    SDFSurfaceKey owner;
    float attributionConfidence;
};
struct SDFPrimitiveRecord {
    glm::mat4 worldFromLocal;
    glm::mat4 localFromWorld;
    SDFShapeParameters shape;
    SDFSurfaceKey key;
    uint64_t geometryRevision;
    uint64_t materialRevision;
    uint32_t primitiveType;
    uint32_t operation;
    float blendDistance; // world metres
    float conservativeDistanceScale;
};
struct SDFSceneSnapshot {
    uint64_t sceneInstance;
    uint64_t revision;
    std::vector<SDFPrimitiveRecord> primitives; // owned, explicit CSG order
};
SDFSample EvaluateSDF(const SDFSceneSnapshot&, glm::vec3 worldPoint) noexcept;
```

GPU layout ayrı packed adapter'dır; C++ mat4 struct'ının ham belleği körlemesine serialize edilmez. Material ve temporal metadata ayrı buffer'larda tutulabilir; son boyut `sizeof/offsetof` testleri ve shader ABI testiyle sabitlenir.

Revision sözleşmesi: `geometryRevision` shape/primitive/CSG anlamı değiştiğinde artar; yalnız rigid transform hareketinde artmaz. Transform hareketi current/previous matrix ile izlenir. `materialRevision` materyal değişiminde artar ve renk geçmişinin güvenini azaltır. Parent visibility, silme ve CSG sıra değişimi ayrıca change set'e girer. Snapshot revision herhangi bir içerik değişimini işaretleyebilir; tek başına tüm görüntünün history'sini reddetme sebebi değildir.

## P0 — Referans ve çalışma zemini

**Dosyalar:** `Tests/EngineTests/src/VisualQualityGpuTests.cpp`, `Tests/EngineTests/CMakeLists.txt`, `docs/A4_VISUAL_QUALITY.md`, `tools/visual_quality_report.py`.

- [ ] Mevcut CPU ve GPU paketlerini çalıştır; GPU/driver, çözünürlük, ışıklar, HDR fixture, exposure, jitter ve seed bilgisini yeni çalışmaya ait manifest'e yaz.
- [ ] Mevcut A4 referanslarını salt-okunur girdiler olarak kullan; yeni koşu çıktıları ayrı klasöre yazılsın. Baseline yenilemesi otomatik test yan etkisi olmasın.
- [ ] Deterministik frame capture yardımcısını ayır; test sahnesi seçimi ve frame index üzerinden tekrar oynatma sağla.
- [ ] Boş sahne, kamera kesimi, resize ve HDR reference testlerinin başlangıçta geçtiğini kaydet.

```text
cmake --build build-release --target EngineTests VisualQualityGpuTests CameraGpuTests
ctest --test-dir build-release --output-on-failure -R "EngineTests\.(AllCpu|GPU)"
```

**Çıkış:** Önceki başarılı sayılar yeni HEAD için varsayılmaz; ölçüm manifest'i ve gerçek test sonucu vardır. Önerilen commit: `test: establish deferred quality reference fixtures`.

## P1 — Şekil boyutlarını transform'dan ayır

**Dosyalar:** Yeni `include/Astral/Geometry/SDFShape.hpp`; değişen `include/Astral/Core/Components.hpp`, `src/Scene/SceneSerializer.cpp`, `src/Scene/Scene.cpp`, `include/Astral/Scene/SceneCommands.hpp`, `tools/AstralEditor/src/Panels/Inspector.cpp`, `Projects/Sandbox/src/DemoScene.hpp`, `Projects/Sandbox/src/PuzzleGameSubsystem.cpp`; test `Tests/EngineTests/src/SerializationTests.cpp`, yeni `SDFShapeTests.cpp`.

**Arayüz:** `SDFComponent` içine `SDFShapeParameters shape`, `SDFShapeEncoding encoding` ve kalıcı `uint64_t csgOrder` eklenir. Yeni oluşturulan içerik `ExplicitShape` kullanır. Transform yalnızca konum/rotation/scale taşır.

- [ ] Test yaz: bir sphere radius=2 ve scale=(1,1,1) iken radius düzenlemesi transform'u değiştirmez; scale düzenlemesi radius'u değiştirmez. Box, torus, capsule, cylinder ve plane için boyut alanlarını ayrı sınayacak assertion'lar ekle.
- [ ] Eski binary fixture'ları yükleyen başarısız uyumluluk testleri yaz: yüklendikten sonra dünya yüzeyi, parent/child pozları ve CSG sırası değişmemeli.
- [ ] SDF chunk sürümünü artır; eski reader yolunu koru. Boyutlar little-endian alan bazında yazılsın; bilinmeyen enum, negatif shape radius, NaN ve eksik payload yüklemeyi atomik olarak başarısız kılsın.
- [ ] Eski sahneleri `LegacyPackedScale` olarak işaretle. Parent ölçeğiyle bağlı torus/plane gibi içerikte “shape içine scale kopyala, transform'u 1 yap” dönüşümü **uygulanmasın**; bu çocukları ve eski anlamı bozabilir. Uyumluluk adapter'ı eski dünya geometrisini yeni snapshot'a dönüştürsün. Kalıcı dönüştürme ayrı ve görüntüyle doğrulanan editör işlemi olsun.
- [ ] Inspector'da “Şekil boyutları” ve “Transform ölçeği” ayrı alanlar olsun. Undo/redo, duplicate, scene clone ve save/load yeni alanları taşısın. Legacy encoding görünür biçimde belirtilecek.
- [ ] Sandbox'ın yeni authoring örneklerini explicit shape kullanacak şekilde güncelle; eski fixture dosyalarını koru.

```cpp
SDFShapeParameters sphere{glm::vec4(2.0f, 0.0f, 0.0f, 0.0f)};
TransformComponent pose;
const auto before = pose.scale;
sphere.dimensions.x = 3.0f;
assert(pose.scale == before);
```

**Çıkış:** Yeni içerikte altı primitive için şekil/transform bağımsızdır. Eski sahnelerden hiçbirine sessiz geometri dönüşümü yapılmaz. Önerilen commit: `feat: separate SDF shape parameters from transforms`.

## P2 — Ortak geometri kernel'i ve sıralı snapshot

**Dosyalar:** Yeni `include/Astral/Geometry/SDFKernel.inl`, `SDFSceneSnapshot.hpp`, `src/Geometry/SDFSceneSnapshot.cpp`, `shaders/SDFScene.glsl`; değişen `src/Core/RenderExtractionSystem.cpp`, `include/Astral/Renderer/SDFEdit.hpp`, `src/Scene/SDFWorldQuery.cpp`, `src/Renderer/BrickGrid.cpp`, `shaders/SDFCompute.glsl`, `shaders/SDFGBuffer.glsl`, `AstralEngine/CMakeLists.txt`; yeni `Tests/EngineTests/src/SDFContractTests.cpp` ve `SDFContractGpuTests.cpp`.

**Arayüz:** Yukarıdaki `SDFSceneSnapshot`, `SDFPrimitiveRecord`, `EvaluateSDF`. Snapshot builder sahneye aittir; global static cache yoktur. CPU query mevcut overload'larını korur, snapshot overload'ları eklenir.

- [ ] Önce CPU/GPU karşılaştırma testini kur: aynı sıralı kayıtlar, sabit seed'li 10.000 dünya noktası, altı primitive ve beş CSG operation. GPU mesafeleri readback ile karşılaştırılsın; yalnız görüntü benzerliğine dayanılmasın.
- [ ] Primitive/CSG denklemlerini C++ ve GLSL'nin dar adapter macro'larıyla dahil edeceği tek `SDFKernel.inl` içine taşı. Kaynak dosyayı elle kopyalayan iki implementasyon bırakma. Shader include değişikliğinin SPIR-V'yi yeniden derlemesini CMake `DEPENDS`/include bağımlılıklarıyla doğrula.
- [ ] World matrix, parent visibility, explicit CSG order ve generational identity için tek snapshot builder ekle. `csgOrder` eşitliğinde kalıcı deterministik tie-break uygula; pool sıra değişimini geometri sırası olarak kullanma.
- [ ] Yüzey, `localFromWorld * worldPoint` ile değerlendirilir. Ortogonal TRS için minimum mutlak ölçek distance lower bound olur. Shear içeren invertible affine matrix için güvenli alt sınır `1/sqrt(||A^-1||1 * ||A^-1||inf)` kullan; bunun tam Euclidean distance olmadığını belirt. Tekil transform'u her iki tüketicide aynı biçimde dışla.
- [ ] Legacy adapter eski dünya boyutlarını koruyan kayıt üretir. Explicit shape yolu, shape parametresini dünya scale'iyle tekrar çarpmaz.
- [ ] CPU collision/raycast/normal ve GPU visibility/picking yeni kernel'i kullansın. Registry convenience query doğruluğu için güncel snapshot kurabilsin; yoğun fizik çağrıları adım başına güncel snapshot overload'ını kullansın. Collider hareketi sonrası eski snapshot tekrar kullanılmasın.
- [ ] BrickGrid aynı kayıtların conservative bounds bilgisinden üretilsin. Intersect ve smooth CSG için bound birleşim kuralları ayrı test edilsin.
- [ ] Testler: ebeveyn dönüş/ölçek, gizli ebeveyn, mirror, near-singular transform, pool deletion/reinsertion, sıralı subtraction, sıfır blend'in hard operation'a geçişi, CSG cusp'ta finite normal.

```cpp
// Proposed assertion inside the CPU/GPU contract test:
const float tolerance = 1e-4f + 1e-4f * std::abs(cpu.distance);
assert(std::isfinite(gpuDistance));
assert(std::abs(cpu.distance - gpuDistance) <= tolerance);
// Ownership equality is asserted only away from equal-distance / smooth blend boundaries.
```

**Çıkış:** Aynı snapshot üzerindeki render/fizik farkı tanımlı tolerans içindedir; parent visibility ve sıra farkları kapanmıştır. Önerilen commit: `refactor: unify SDF geometry evaluation across CPU and GPU`.

## P3 — Deferred SDF gölgeleri ve AO

**Dosyalar:** Yeni `shaders/SDFVisibility.glsl`; değişen `shaders/DeferredLighting.glsl`, `src/Renderer/SDFRenderer.cpp`, `include/Astral/Renderer/SDFRenderer.hpp`, `include/Astral/Renderer/ComputePipeline.hpp`, `AstralEngine/CMakeLists.txt`; yeni `Tests/EngineTests/src/DeferredLightingTests.cpp`.

**Arayüz:** Kalite profilinde `shadowMaxSteps=96`, `shadowMaxDistance=50 m`, `aoSamples=8`, `aoRadius=1 m` başlangıç değerleri açıkça saklanır. Bunlar referans doğrulamasından sonra sabitlenir; FPS uğruna sessizce düşürülmez.

- [ ] Tek yönlü ışık ve tek noktasal ışık için gölgeleme testlerini önce yaz: blocker önünde/arkasında, ışıkla aynı noktada, uzak ışık, grazing angle, smooth CSG oyuk, non-uniform scale.
- [ ] G-buffer world position/normal ile ortak snapshot/kernel üzerinden visibility hesapla. Yönlü ışık yalnız profil max distance'a kadar; point shadow ışığa kalan mesafeye kadar ilerlesin. Işığın arkasındaki nesne gölge üretmesin.
- [ ] Surface bias'i dünya biriminde tanımla; acne ve peter-panning testinde bias aralığını doğrula. Güvenli bound ile ilerle; tekil/NaN mesafelerde deterministik fail-safe sonuç üret.
- [ ] Gölge yalnız ilgili doğrudan ışık katkısını çarpsın. AO ambient diffuse IBL'e uygulanır; emissive/direct light körlemesine karartılmaz. Specular occlusion ilk sürümde ayrı fiziksel model varmış gibi sunulmaz.
- [ ] TAA için güncel lighting/AO reactivity bilgisi çıkar; çoklu ışıkta salt “ortalama visibility” kullanma. Bir ışığın artıp diğerinin azalması ortalamada kaybolmamalı; ışık katkısı değişimi veya ayrı doğrulanabilir sinyal gerekir.
- [ ] Noktasal attenuation davranışını mevcut modelle koru ve belgede tut; ışık falloff değişikliğini bu gölge taşımasına karıştırma.

```glsl
// Contract of the deferred lighting combination:
direct += evaluateDirectLight(light, surface) * shadowVisibility;
ambientDiffuse *= ambientOcclusion;
// No finalColor *= ambientOcclusion shortcut.
```

**Çıkış:** Deferred, tanımlı yönlü/noktasal gölge ve AO fixture'larını geçer. Forward'ın sabit ışığı yeni özellik geliştirme hedefi değildir. Önerilen commit: `feat: add SDF shadows and AO to deferred lighting`.

## P4 — SDF temporal güven

**Dosyalar:** Yeni `include/Astral/Renderer/SDFTemporalHistory.hpp`, `src/Renderer/SDFTemporalHistory.cpp`; değişen `shaders/SDFGBuffer.glsl`, `shaders/TAAResolve.glsl`, `src/Renderer/SDFRenderer.cpp`, `src/Core/RenderExtractionSystem.cpp`, `include/Astral/Core/Components.hpp`; yeni `Tests/EngineTests/src/SDFTemporalTests.cpp`.

**Arayüz:** Temporal owner, başarılı render sonrası snapshot'ı commit eder; extraction'ın kendisi geçmişi ilerletmez. Geometri buffer'ı ile identity/normal history ping-pong kaynakları açıkça ayrılır.

- [ ] Önce başarısız davranış testleri ekle: aynı derinlikte farklı nesne, entity slot reuse, dönen/scaling nesne, aynı frame'in iki extraction'ı, render edilmeyen update, camera cut, save/load sonrası yeni scene instance.
- [ ] Rigid/deform olmayan yüzey için previous point'i `previousWorldFromLocal * currentLocalFromWorld * hitPoint` ile üret. Shape parametresi/topoloji değişiminde bu eşleşmenin geçerli olduğunu varsayma.
- [ ] Hard rejection: kimlik/generation uyumsuzluğu, depth mismatch, ekran/kamera arkası, geçersiz koordinat, kesim veya geometry revision değişimi.
- [ ] Normal uyumu, smooth-CSG attribution güveni ve aydınlatma değişimi ile kalan history ağırlığını azalt. Başlangıç history üst sınırı mevcut 0.88; güven azaldıkça güncel kare ağırlığı artsın.
- [ ] Smooth/subtractive yüzeyde yalnız dominant primitive indeksine güvenme. Owner değişimi, blend ağırlığı değişimi ve belirsiz contributor durumunda güven azalmalı; “kesin materyal kimliği = kesin yüzey kimliği” kabul edilmesin.
- [ ] Mevcut YCoCg clamp korunur. Önceki renk örneklemesinin footprint'i içindeki uyumsuz depth/identity texel'leri kenarlardan geçmiş sızdırmasın.
- [ ] Önceki kamera/normal/identity/depth ve lighting history resize/kesimde birlikte resetlensin; Vulkan format/descriptor/sync kontrolleri eklensin.

```glsl
float confidence = identityValid && depthValid && geometryValid ? 1.0 : 0.0;
confidence *= normalConfidence * attributionConfidence * lightingConfidence;
float historyWeight = 0.88 * clamp(confidence, 0.0, 1.0);
vec3 resolved = mix(currentColor, clampedHistoryColor, historyWeight);
```

**Çıkış:** Yeni yüzeye eski nesnenin rengi taşınmaz; render edilmeyen frame geçmişi yanlış ilerletmez; stabil bölge gereksiz resetlenmez. Önerilen commit: `feat: add SDF-aware temporal confidence`.

## P5 — Yerel değişim ve geçmiş reddi

**Dosyalar:** Yeni `include/Astral/Geometry/SDFChangeSet.hpp`, `src/Geometry/SDFChangeSet.cpp`, `shaders/SDFHistoryInvalidation.glsl`; değişen `src/Renderer/BrickGrid.cpp`, `src/Renderer/SDFRenderer.cpp`, `shaders/TAAResolve.glsl`; yeni `Tests/EngineTests/src/SDFChangeSetTests.cpp`.

**Arayüz:** Ardışık render snapshot'larından `SDFChangeSet` üretilir. Transform-only motion, shape/CSG change, material change, visibility/removal ve global change ayrı sınıflardır. Direct component mutation olduğu için yalnız editör event'leri yeterli değildir; snapshot karşılaştırması güvenlik ağıdır.

- [ ] Testleri önce yaz: kapı oyma, cutter taşıma, entity silme, parent visibility, smooth radius değişimi, uzak etkilenmeyen nesne ve ekran dışı blocker'ın ekrandaki gölgesi.
- [ ] Son render edilen ve güncel bound'ları birleştir; smooth support genişlemesini CSG zinciri boyunca konservatif olarak yay. Rigid motion için bütün owner'ı resetlemek yerine P4 reprojection çalışsın; açığa çıkan bölgeler depth/identity ile reddedilsin.
- [ ] Geometry change alanlarını current/previous camera ile project et; jitter ve resolve footprint kadar büyüt. Kamera near-plane'ini kesen AABB'nin yalnız sekiz köşesini bölmek güvenli değildir; frustum clipping veya konservatif tam ekran fallback kullan.
- [ ] Yerel maskenin dışındaki gölge/AO alıcılarını P3'ün güncel aydınlatma değişimiyle işaretle. Ekran dışındaki değişimin ekrandaki etkisini kaçırma.
- [ ] Intersect, sonsuz plane, CSG reorder, geçersiz bound ve sınırsız etki için global invalidation uygula. Yerellik kanıtlanamıyorsa yanlış bir küçük rect üretme.
- [ ] Sabit buffer bütçesi aşılırsa bölgeleri konservatif birleştir; hâlâ sığmıyorsa global reset. Hiçbir changed region sessizce düşürülmesin. Revision wrap ve scene switch resetleri test edilsin.
- [ ] Debug çıktısı etkilenen bölge oranını ve reset sebebini gösterir. Lokal oyma fixture'ında uzak kontrol bölgesinin history'si korunur.

```text
Door carve acceptance:
  changed surface pixels -> rejected history
  displaced shadow pixels -> rejected/reduced history
  distant unchanged control region -> history retained
  infinite-plane or CSG reorder -> documented global fallback
```

**Çıkış:** Oyma işlemi tüm görüntüyü gereksiz resetlemez; değişen gölge iz bırakmaz. Önerilen commit: `feat: invalidate temporal history from SDF change regions`.

## P6 — Kullanılabilirlik ve yaşam döngüsü

**Dosyalar:** `include/Astral/Core/Application.hpp`, `Projects/Sandbox/src/main.cpp`, `tools/AstralEditor/src/EditorUI.cpp`, `tools/AstralEditor/src/Panels/Inspector.cpp`, `include/Astral/Renderer/RenderContext.hpp`, `docs/A4_VISUAL_QUALITY.md`.

- [ ] Deferred ana profilinin gölge/AO/TAA varsayılanlarını tek kalite ayarında topla. Debug kullanıcı arayüzü GPU buffer ayrıntıları yerine anlaşılır adlar kullansın.
- [ ] Debug modları ekle: yüzey kimliği, geometry revision, temporal confidence, rejection reason, changed region, shadow visibility ve AO. Debug değiştirmek history kaynaklarını karıştırmasın.
- [ ] Play/Stop/Clone/Load, kamera değişimi, küçültülmüş pencere, resize ve başarılı olmayan frame durumlarında snapshot/history ömrünü test et.
- [ ] Legacy forward görünür biçimde karşılaştırma profili olarak etiketlensin; gölgesiz deferred veya TAA kapalı sonuçlar ana kalite profili adıyla kaydedilmesin.
- [ ] Yeni dokümantasyonda API kullanım sırası ve support matrix güncellensin; mevcut örnek uygulamalar yeni explicit shape authoring'i göstersin.

**Çıkış:** Sistem yalnız test harness'inde değil Sandbox/editör akışında çalışır. Önerilen commit: `feat: integrate SDF quality controls and diagnostics`.

## P7 — A4 kapanış kapısı

**Dosyalar:** `Tests/EngineTests/src/VisualQualityGpuTests.cpp`, `Tests/EngineTests/CMakeLists.txt`, `tools/visual_quality_report.py`, `docs/A4_VISUAL_QUALITY.md`, `ROADMAP_IMPROVEMENTS.md`.

- [ ] Deterministik kayıtları genişlet: 1280×720'de 120 kare; pan, thin, metal, fast, disocclusion, resize, door carve, offscreen shadow caster, parent transform, camera cut. Ayrıca 1920×1080 statik kalite kareleri al.
- [ ] Aynı sahnenin yüksek örnekli spatial referansını üret: statik karede 16 jitter örneği linear HDR'de ortalanır; exposure/tonemap/sRGB bir kez uygulanır. Hareket testinde aynı anın donmuş geometri/kamera snapshot'ı kullanılır.
- [ ] Kabul toleranslarını fixture manifest'ine işle; başarısız olunca sessizce genişletme. Yeni kapsam için önerilen başlangıç sınırları aşağıdadır.
- [ ] GPU validation, CPU geometri sözleşmesi, serialization/migration, editor gameplay ve mevcut A4 testlerini birlikte çalıştır.
- [ ] Kayıtları gerçek zamanlı ve yavaş oynatmada incele; yalnız contact sheet incelemesi “hareketli kabul geçti” sayılmasın. İnceleyen kişi/araç, kayıt hash'i ve sonuç belgelensin.
- [ ] A4 ancak sayısal kontroller ve hareketli inceleme geçince tamamlandı işaretlenir. Sonuç, A5'in değişmez referans kalite profili olur.

| Ölçüm | Kabul |
|---|---|
| CPU/GPU signed distance | `abs(error) <= 1e-4 + 1e-4 * abs(reference)` |
| Grid on/off | Mevcut RGBA8 MAE ≤ 0.5; error > 8 kanal oranı ≤ %1 |
| Camera cut / aynı seed temiz ilk kare | Birebir |
| Statik temporal sonuç / 16 örnek spatial referans | Normalize RGB MAE ≤ 0.01; SSIM ≥ 0.98 |
| Disocclusion/oyma ROI; ilk 3 kare | Normalize RGB MAE ≤ 0.02; error > 0.1 kanal oranı ≤ %1 |
| Sabit uzak kontrol bölgesi, lokal oyma | Geçerli history piksellerinin ≥ %99'u gereksiz invalidation almaz |
| Shadow fixture | Işık arkasındaki blocker sonucu değiştirmez; görünür alıcıda referans visibility mutlak hata ≤ 0.03 |
| Finite output | HDR/normal/motion/depth NaN veya Inf: 0 |
| Yaşam döngüsü | Validation error: 0; yanlış scene history kullanımı: 0 |

SSIM gibi metriklerin seçilen fixture/ROI'leri manifest'te sabitlenir. Bu yeni sınırlar henüz ölçülmüş sonuç değil, planın kabul hedefleridir. Süre/bellek raporlanır; A5 öncesi genel FPS taahhüdü verilmez.

## Riskler ve kararlar

- **Eski scale anlamı:** Legacy adapter ve alan bazlı sürümleme; otomatik toplu dönüşüm yok.
- **CSG kimliği:** Dominant owner tek başına yeterli değil; attribution confidence ve değişim bilgisi gerekiyor.
- **Uzak aydınlatma etkisi:** Sadece nesne rect'i yeterli değil; lighting reactivity zorunlu.
- **Shear altında mesafe:** Seviye kümesi doğru olsa bile mesafe estimator'ı tam Euclidean değildir; CPU sphere collision adımları conservative sınır ve iteratif yakınsama ile sınanır.
- **ECS yazımları:** Revision yalnız setter çağrılarına bağlı olmaz; snapshot farkı kaçırılan doğrudan yazımları yakalar.
- **History belleği:** Yeni identity/normal/lighting kanalları için bayt/piksel ve 720p/1080p bütçesi P4'te kaydedilir; float identity veya düşük hassasiyetli depth ile yer kazanılmaz.
- **Kapsam:** Yeni renderer, genel CSG graph editörü, voxel yeniden inşası, donanım RT veya A5 ölçekleme projesi açılmaz. Mevcut analitik SDF mimarisinin doğruluğu tamamlanır.

## Plan özdenetimi

- [x] Onaylanan beş başlık P1–P5'e eşlendi; deferred gölge/AO ve dolaylı değişim etkisi kapsandı.
- [x] Shape migration, parent visibility, CSG sıra kararlılığı ve render-frame history ownership açıklandı.
- [x] Yeni arayüzlerin öneri olduğu belirtildi; mevcut olmayan API'ler mevcutmuş gibi sunulmadı.
- [x] Her aşama için dosya kapsamı, test davranışı ve çıkış koşulu tanımlandı.
- [x] Önceki kare incelemesi ile tam hareketli görsel kabul ayrıldı.
- [x] Bu dokümanın oluşturulması uygulama adımlarını tamamlandı saymaz; P0–P7 henüz yürütülmedi.
