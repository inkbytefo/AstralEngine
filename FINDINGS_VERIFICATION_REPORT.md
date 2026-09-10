# AstralEngine — Sistemik Kod İnceleme Bulguları Doğrulama Raporu

**Tarih:** 07 Eylül 2026
**Doğrulama Bazı:** `3adfa0b` (`main`, "feat: implement SDF BrickGrid acceleration structure...")
**Yöntem:** Sağlanan bulgu listesindeki her iddia, mevcut kaynak kod (`src/`, `include/`), shader kaynakları (`shaders/`) ve yardımcı araçlar (`tools/`) üzerinde satır satır doğrulandı. Her bulgu için **kanıt** (dosya + satır), **analiz/nüans** ve **öncelik revizyonu** verilmiştir.
**Kapsam:** Bulguların doğruluğu araştırıldı; okuyucunun istediği üzere **hiçbir kod değişikliği yapılmadı**.

---

## 1. Yönetici Özeti — Doğrulama Sonuçları

| Bulgu | Öncelik (Kaynak rapor) | Doğrulama Sonucu | Öncelik Revizyonu Önerisi |
| :--- | :--- | :--- | :--- |
| 1.1 BrickGrid: global smoothExpansion + yanlış AABB | KRİTİK | ✅ **Doğrulandı** (her iki kod parçası da birebir mevcut) | KRİTİK (korunur) |
| 1.2 VulkanContext: kare başı bloklayıcı fence bekleme | KRİTİK | ✅ **Doğrulandı** (zincirleme 3 noktada bloklama) | KRİTİK (korunur) |
| 1.3 Application/Window: resize + swapchain blit atlanması | KRİTİK | ⚠️ **Kısmen doğrulandı** (kod parçaları doğru; etki değerlendirmesi editor için geçersiz) | YÜKSEK |
| 1.4 SDFRenderer: `(void)spvPath;` shader yolu yutulması | KRİTİK | ⚠️ **Kısmen doğrulandı** (`(void)spvPath;` mevcut; ancak yol dizin türetmede kullanılıyor, etki dar) | ORTA |
| 2.1 Push constant dizilim tutarsızlığı | YÜKSEK | ✅ **Doğrulandı** — ancak **aktif bug değil**, mimari/bakım riski | ORTA |
| 2.2 Exposure çiftleme riski | YÜKSEK | ⚠️ **Kısmen doğrulandı** (`SDFLighting.hpp` diye bir dosya yok; latent risk mevcut) | ORTA |
| 2.3 G-Buffer `RGBA32UI` + tüm imajlarda `eGeneral` | YÜKSEK | ✅ **Doğrulandı** (format ve layout kullanımı doğru; ek yorum tutarsızlığı da bulundu) | YÜKSEK |
| 2.4 `m_PrevWorldTransforms` bellek sızıntısı / kimlik çakışması | YÜKSEK | ✅ **Doğrulandı** | YÜKSEK |
| 2.5 `sceneInstanceId = 1` hardcode | YÜKSEK | ✅ **Doğrulandı** (3 ayrı çağrı noktasında) | YÜKSEK |
| 2.6 IBL: tek iş parçacıklı CPU cubemap konvolüsyonu | YÜKSEK | ✅ **Doğrulandı** (BRDF LUT çoklu iş parçacıklı + cache'li; cubemap'ler tek iş parçacıklı + cache'siz) | YÜKSEK |
| 3.1 TransformSystem: her kare başına heap alloc | ORTA | ✅ **Doğrulandı** | ORTA |
| 3.2 Debug modları üç farklı header'da çelişkili | ORTA | ✅ **Doğrulandı** (siyah çıktı iddiası kısmen abartılı) | ORTA |
| 3.3 JobSystem: timeout'ta work-helping devre dışı | ORTA | ✅ **Doğrulandı** | ORTA |
| 3.4 Shadow adım `clamp(h, 0.015, 0.5)` | ORTA | ✅ **Kod doğrulandı** (etki tartışmalı) | DÜŞÜK-ORTA |
| 4.1 `WorldTransformComponent` ölü alanları | DÜŞÜK | ✅ **Doğrulandı** (ancak kaldırma önerisi eksik; legacy kod kırılır) | DÜŞÜK |
| 4.2 `IUpscaler` ölü arayüz | DÜŞÜK | ✅ **Doğrulandı** | DÜŞÜK |

**Önemli bulgu:** Sağlanan rapordaki tüm **kod alıntıları** birebir mevcut kaynak koddan doğrulandı (tek istisna: `SDFLighting.hpp` dosyası yok; ilgili fonksiyonlar `SDFVisibility.hpp` içinde). Raporun çoğu tespiti **teknik olarak doğrudur**; asıl farklar **etki değerlendirmeleri** ve bazı **öncelik seviyelerinde** ortaya çıkmaktadır (özellikle 1.3, 1.4, 2.1, 2.2).

---
## 2. Kritik Seviye Bulguları Doğrulama

### [Bulgu 1.1] BrickGrid.cpp — Global smoothExpansion ve Yetersiz AABB Hesabı

**Sonuç: ✅ DOĞRULANDI (her iki iddia da koddan doğrulanmıştır)**

**Kanıt:**

- `src/Renderer/BrickGrid.cpp` içindeki `FullRebuild` ve `Build` fonksiyonlarında blend faktörlerinin **tüm kayıtlar üzerinde toplanması** birebir mevcuttur:

```cpp
// BrickGrid.cpp, satır ~191, ~218, ~226, ~246 (FullRebuild ve Build içinde)
float smoothExpansion = 0.0f;
for (const auto& rec : records) {
    if (rec.operation == 3 || rec.operation == 4)
        smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
}
```

- Global olarak tüm hücrelere düşülmesi doğrulandı:

```cpp
m_CellDistances[cIdx] = std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f); // satır ~246 ve ~420
```

- AABB hesabının `dimensions.xyz`'yi **her primitif tipi için yarı-boyutlar** olarak kullanması doğrulandı. `ComputeDynamicBounds` ve `PrecomputeRecordBounds` içinde `localExtents = glm::abs(glm::vec3(rec.dimensions))` biçiminde kullanılmaktadır. `SDFShapeParameters` spesifikasyonuna göre (`include/Astral/Geometry/SDFShape.hpp`, satır 13–21):
  - Torus: `x = R (major)`, `y = r (minor)`, `z = 0` (Z kalınlığı için kullanılmaz)
  - Capsule: `x = radius`, `y = segmentLength`, `z = 0`
  - Cylinder: `x = radius`, `y = halfHeight`, `z = 0`
  - Bu yüzden torus/cylinder/capsule kayıtlarında Z ekseni yarı-boyutu `0` kalır ve hatalı (sıfır kalınlık / eksik) AABB üretilir.

**Analiz / Nüans:**

1. **Global toplama etkisi:** `blendFactor=0.5` olan ~20 smooth nesnede `smoothExpansion ≈ 20 × 0.5 × 0.25 = 2.5 m` olur ve tüm hücrelerin `minDist - halfDiag - smoothExpansion` değeri `0`'a çekilir; boş uzay atlama (PR-6) devre dışı kalır. Sayısal örnek doğrudur.
2. **Proje zaten doğru analitik AABB üreten bir yardımcıya sahiptir:** `SDFChangeSet.cpp::GetLocalBounds` (satır 8–61). Bu fonksiyon torus için `xz = R + r, y = r`, kapsül için `y = h + r`, silindir için `xy/z = r` vb. **primitif tipi bilinçli** AABB döndürür. Yani raporun önerdiği "BrickGrid mantığını `SDFChangeSet.cpp::GetLocalBounds` ile birleştirin" önerisi **uygulanabilir ve düşük maliyetlidir** — helper hazırdır, sadece BrickGrid tarafından kullanılmamaktadır.
3. Smooth-blend'in *lokal* olması doğru bir gözlemdir; önerilen "hücreye etki eden komşu çiftler için lokal değerlendirme" makuldür.

**Öncelik revizyonu:** KRİTİK (korunur).

---

### [Bulgu 1.2] VulkanContext.cpp — Kare Başı Bloklayıcı Fence Bekleme (Lock-Step)

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `src/Renderer/VulkanContext.cpp`, satır 679–681 (`EndFramePresent`):

```cpp
// 4. Fence bekle ve GPU zamanini oku
auto res = m_Device->waitForFences(1, &m_FrameFence.get(), VK_TRUE, UINT64_MAX);
(void)res;
```

- Tek bir command buffer (`m_CommandBuffer`, header satır 111) ve **tek bir fence** (`m_FrameFence`, header satır 112) kullanılıyor. `submitInfo` satır 656'da `m_FrameFence.get()` ile submit ediliyor.
- Ayrıca `VulkanContext.cpp` satır 683–692 (`getQueryPoolResults`, `VK_QUERY_RESULT_WAIT_BIT`) ile **GPU zamanı koddan aynı karenin sonunda** senkron okunuyor.
- `m_ImageAvailableSemaphores` / `m_RenderFinishedSemaphores` vektörleri (header 120–121) "double-buffer" görünümündedir; ancak `m_CurrentFrame` yalnızca semaphore dönüşümünde kullanılır, command buffer/fence aynı kalır. `m_FrameFence` tek olduğundan GPU, sunumdan sonra tamamlanana kadar CPU kilitlenir; bir sonraki kare ancak önceki kare tamamlandıktan sonra başlar.

**Analiz / Nüans:**

1. İddia edilen "lock-step execution" doğrudur: CPU, `waitForFences(UINT64_MAX)` ile her karenin sonunda GPU'yu bekler.
2. Ek nokta: `getQueryPoolResults` timestamp okuması da aynı fence beklemesinin hemen ardından aynı (eski) karenin sorgusu üzerinde yapıldığından bu okuma zaten senkron ve bloklayıcıdır; raporun "timestamp okumasını önceki kareye taşıma" önerisi tutarlıdır.
3. Raporun "çift/üçlü tamponlama paralelliği sıfırlanır" etki ifadesi doğru; bu yapıda CPU-GPU overlap fiilen yoktur.

**Öncelik revizyonu:** KRİTİK (korunur).

---
### [Bulgu 1.3] Application.cpp & Window.cpp — Resize Eksikliği ve Swapchain Blit Atlanması

**Sonuç: ⚠️ KISMEN DOĞRULANDI**

**Kanıt (doğru kısımlar):**

1. `Window.cpp` satır 54–60: `glfwSetFramebufferSizeCallback` yalnızca `m_Width`/`m_Height`'i günceller.
2. `Application::Run` ana döngüsünde (`src/Core/Application.cpp`, satır 164–406 arası) **`m_SDFRenderer->Resize()` hiç çağrılmaz** — doğrulandı.
3. `Application.cpp` içindeki `HasRenderSubsystem` dalı:

```cpp
if (m_SystemManager.HasRenderSubsystem()) {
    m_VulkanContext->PrepareSwapchainImage();
} else {
    m_VulkanContext->EndFrameBlit(m_SDFRenderer->GetStorageImage(), ...);
}
```

`PrepareSwapchainImage()` yalnızca layout geçişi yapar; içerik temizlenmez, `m_StorageImage` blit edilmez — **doğrulandı**.

**Analiz / Nüans (etki değerlendirmesindeki sapmalar):**

1. **"Renderer 1280x720'de donup kalır" etkisi yalnızca editor'süz istemciler için geçerlidir.** AstralEditor'de `tools/AstralEditor/src/EditorUISubsystem.cpp` satır 142–148, `ViewportPanel::HasPendingResize()` olduğunda `m_App.GetRenderer()->Resize(...)` çağrısını **zaten yapıyor**. Bu yüzden editor üzerinde resize sonrası renderer takılması fiilen yaşanmaz; eksiklik doğrudan `Application`'dan türeyen (editor içermeyen) projelerde geçerlidir.
2. **"UI aktifken sahne hiç çizilmez / garbage görünür" etkisi kısmen yanlıştır.** `ViewportPanel.cpp` satır 82, `m_Renderer->GetStorageImageView()`'ı ImGui dokusu olarak kullanır; 3D sahne swapchain'e blit edilmese bile pencere içinde **ImGui dokusu olarak çizilir**.
3. Raporun aksine swapchain yenilemesi `Window` içinden değil, `VulkanContext::EndFramePresent` (satır 670–677) içinden `eSuboptimalKHR`/`OutOfDateKHR` hatalarında `RecreateSwapchain()` ile tetiklenir.
4. Raporda önerilen "önce blit, sonra color attachment geçişi" önerisi mimari olarak doğrudur.

**Öncelik revizyonu:** KRİTİK → **YÜKSEK**

---

### [Bulgu 1.4] SDFRenderer.cpp — `(void)spvPath;` (Shader Yolu Ezilmesi)

**Sonuç: ⚠️ KISMEN DOĞRULANDI (kod mevcut, etki dar)**

**Kanıt:**

- `src/Renderer/SDFRenderer.cpp` satır 98:

```cpp
(void)spvPath;
```

- Constructor satır 34–46: `m_GBufferSpvPath(gbufferSpvPath)` — GBuffer shader yolu **yalnızca** `gbufferSpvPath` parametresinden gelir.
- `Application.cpp` satır 109–114: `spvPath` (yani `m_Config.shaderPath`) SDFRenderer constructor'ına geçilir; ancak yalnızca diğer shader'ların varsayılan yollarını türetmek için kullanılır (satır 52–96).

**Analiz / Nüans:**

1. `spvPath` **tamamen** çöpe atılmaz: satır 48 `std::filesystem::path p(spvPath);` ile dizin bilgisi TAA/DebugComposite/DeferredLighting shader yollarının türetilmesinde kullanılır (satır 52, 64, 76, 88).
2. Raporun asıl iddiası doğrudur: **kullanıcının kendi GBuffer SPIR-V dosyasına işaret eden tam yol asla `m_GBufferSpvPath` olarak atanmaz**; her zaman `gbufferSpvPath` parametresi veya `SDFGBuffer.spv` varsayılanı denenir (satır 63–72). Özel isimli GBuffer shader'ı ile çalıştırmak mümkün değildir.
3. Etki raporun iddia ettiğinden dardır: yalnızca GBuffer shader yolunun bireysel olarak atanması eksiktir; CI testleri varsayılan dosya adıyla çalışır, bu yüzden "CI test shader'ı geçemez" fiili bir kırılma oluşturmaz.
4. **Önerilen düzeltme geçerli:** `m_GBufferSpvPath = spvPath;` ataması `if (m_GBufferSpvPath.empty())` bloğundan (satır 63) önce yapılabilir.

**Öncelik revizyonu:** KRİTİK → **ORTA**

---
## 3. Yüksek Seviye Bulguları Doğrulama

### [Bulgu 2.1] Push Constant Düzeni — `SDFPushConstants` vs `DeferredLightingPushConstants`

**Sonuç: ✅ DOĞRULANDI (fark mevcut; ancak AKTİF bir bug değil)**

**Kanıt:**

- `include/Astral/Renderer/ComputePipeline.hpp` satır 26–36 (`SDFPushConstants`):
  - `screenRes.z = editCount`, `screenRes.w = useGrid`
  - `cameraRight.w = focalLength`
  - `cameraUp.w = farClip`, `camPos.w = nearClip`, `camDir.w = normalMode`
- Aynı dosya satır 78–88 (`DeferredLightingPushConstants`):
  - `camPos.w = maxMipLevel`, `camDir.w = exposure`
  - `screenRes.z = iblIntensity` **(`editCount` burada `w`'ye taşınmış)**
  - `cameraRight.w = focalLength`, `cameraUp.w = useGrid` **(`useGrid` burada `cameraUp.w`'ye taşınmış)**
- Bu tanımlar raporla **birebir** örtüşmektedir.

**Analiz / Nüans (önemli ayrım):**

1. **Her pass kendi push constant yapısını kendi shader'ıyla eşleştirmektedir:**
   - `SDFGBuffer.glsl` satır 74–83 bloğu `SDFPushConstants` ile (camUp.w = far clip, screenRes.z = editCount, screenRes.w = useGrid) **tam eşleşir**.
   - `DeferredLighting.glsl` satır 66–75 bloğu `DeferredLightingPushConstants` ile (camDir.w = exposure, screenRes.z = iblIntensity, screenRes.w = editCount, cameraUp.w = useGrid) **tam eşleşir**.
   - CPU tarafı da ilgili yapıya aynı sırada doldurur: `SDFRenderer.cpp` satır 1313–1318 (GBuffer) ve 1464–1471 (Deferred).
2. Bu yüzden raporun **"ışıklandırma adımında useGrid yerine farClip okunur → sahne aydınlatması kırılır" etkisi şu an gerçekleşmemektedir.** Ortaya çıkan sorun *kavramsal/kurallıdır*: iki pass arasında aynı anlama gelen parametrelerin farklı slotlarda olması, gelecekte shader/push constant değişikliklerinde sessiz veri karışması riski yaratır.
3. Raporun önerisi (ortak taban layout / tek UBO) makul bir mimari önlemdir.

**Öncelik revizyonu:** YÜKSEK → **ORTA** (aktif bug yok; bakım/kuralsal risk)

---

### [Bulgu 2.2] Exposure Çiftleme Riski

**Sonuç: ⚠️ KISMEN DOĞRULANDI (dosya adı yanlış, risk latent)**

**Kanıt:**

- `SDFLighting.hpp` **diye bir dosya yoktur.** İlgili CPU fonksiyonu `include/Astral/Geometry/SDFVisibility.hpp` içindedir (satır 209–279):

```cpp
// SDFVisibility.hpp, satır 278
return (directLo + ambient) * exposure;
```

- GPU Deferred pass: `SDFRenderer.cpp` satır 1465:

```cpp
defPush.camDir = glm::vec4(camDir, 1.0f); // xyz: dir, w: exposure = 1.0 hardcoded
```

- GPU shader: `DeferredLighting.glsl` satır 272:

```glsl
vec3 finalLinearHDR = (Lo + ambient) * camDir.w; // camDir.w = exposure
```

- TAA pası: `SDFRenderer.cpp` satır 1578 `taaPush.colorParams.x = m_Exposure;` ve `TAAResolve.glsl` satır 280 `acesTonemap(blendedHDR * colorParams.x);`.

**Analiz / Nüans:**

1. **Şu anda çift exposure uygulanmamaktadır** — GPU deferred push'ta `camDir.w = 1.0` hardcoded olduğundan exposure yalnızca TAA/tonemap pasında uygulanır. Raporun "sahneye iki defa exposure uygulanır" iddiası latent (ileride `camDir.w`'ye değer verilirse doğacak) bir risktir.
2. CPU test fonksiyonu ile GPU deferred matematik arasında 1:1 uyumsuzluk **şu an yoktur**: `DeferredLightingTests.cpp` (satır 396, 413) `EvaluateDeferredLighting`'i default `exposure = 1.0` ile çağırır; GPU tarafı da 1.0 kullanır.
3. Raporun **mimari tutarsızlık tespiti doğrudur**: exposure çarpanının nerede uygulanacağı net bir sözleşmeye bağlanmamıştır; `camDir.w` alanı zaten shader'da exposure olarak kullanılmakta, TAA pasında ayrıca `m_Exposure` uygulanmaktadır. "DeferredLighting içindeki exposure çarpanını kaldırın / tek pas tanımlayın" önerisi yerindedir.

**Öncelik revizyonu:** YÜKSEK → **ORTA**

---
### [Bulgu 2.3] G-Buffer Bellek İsrafı ve `VK_IMAGE_LAYOUT_GENERAL` Kullanımı

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `SDFRenderer.cpp` satır 288: `m_GBufMaterial` → `vk::Format::eR32G32B32A32Uint` (16 bayt/piksel) — rapor iddiası doğrudur.
- satır 286: `m_GBufNormal` → `eR16G16B16A16Sfloat` (8 bayt/piksel).
- satır 295–330: tüm G-Buffer + tarihçe görüntüleri `ExecuteImmediate` içinde `eGeneral`'a alınıyor:

```cpp
b.oldLayout = vk::ImageLayout::eUndefined;
b.newLayout = vk::ImageLayout::eGeneral;
```

- Render döngüsünde `SDFRenderer.cpp` satır 1282–1297 (G-Buffer yazım barrier'ı `eUndefined→eGeneral`), 1373–1412 (okuma barrier'ı `eGeneral→eGeneral`), TAA barrier'ları 1502–1565 (`eGeneral→eGeneral`) ve descriptor set'leri 826–837 / 903–908 → tüm doku erişimleri `eGeneral` üzerinden. Pipeline içinde `eShaderReadOnlyOptimal`/`eColorAttachmentOptimal` kullanılmıyor.

**Analiz / Nüans:**

1. Bellek hesabı doğrudur: 1080p'de `16 B × 1920 × 1080 ≈ 33.18 MB`; 4K'da `≈ 132.7 MB` (yalnızca materyal hedefi).
2. "DCC kaybı" iddiası dikkatli okunmalıdır: storage image olarak `eGeneral` kullanımında DCC bazı donanımlarda hâlâ çalışabilir; ancak genel kural olarak `eGeneral` render-target'lar için en verimli layout değildir. Raporun "memory bandwidth yükü" argümanı yön olarak doğrudur; kazanç donanıma göre değişir.
3. Raporun önerileri kısmen tartışmalıdır: **Vulkan storage image kısıtları** nedeniyle `R16G16_SNORM` veya `R8G8B8A8_UNORM` her GPU'da `imageStore` hedefi olmayabilir (compute yazımı için tam `VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT` desteği gerekir). Proje zaten `m_GBufAlbedo`'yu `rgba8` storage image olarak kullanıyor (destekli olduğu varsayımıyla), ancak bu varsayım genelleştirilemez; öneri uygulanacaksa önce `vkGetPhysicalDeviceFormatProperties` ile storage bitleri doğrulanmalıdır.
4. Ek tutarsızlık (raporda yer almayan): `SDFRenderer.hpp` satır 182–184'te `m_GBufMaterial; // VK_FORMAT_R8G8B8A8_UNORM` yorumu yazıyor; gerçek format `R32G32B32A32_UINT`'tır (`SDFRenderer.cpp:288`). Yanıltıcı bir yorum.

**Öncelik revizyonu:** YÜKSEK (korunur)

---

### [Bulgu 2.4] `m_PrevWorldTransforms` Bellek Sızıntısı / Kimlik Çakışması

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `src/Renderer/SDFRenderer.cpp` satır 1162–1180 (`UpdateEdits`):

```cpp
uint32_t key = (rec.surfaceId != 0u) ? rec.surfaceId : (0x80000000u | static_cast<uint32_t>(i));
auto it = m_PrevWorldTransforms.find(key);
if (it != m_PrevWorldTransforms.end()) {
    prevMatrices[i] = it->second;
} else {
    prevMatrices[i] = currWorldTransform;
}
m_PrevWorldTransforms[key] = currWorldTransform;
...
} else {
    m_PrevWorldTransforms.clear();
}
```

- Harita yalnızca `records.empty()` olduğunda temizlenir (satır 1179–1181). Sahneden silinen ama başka kayıtlarla dolu olan varlıkların `surfaceId` anahtarları haritada kalır.

**Analiz / Nüans:**

1. **Sızıntı iddiası doğrudur:** dinamik oluşup silinen nesnelerin (mermi, parçacık) surfaceId'leri haritada birikir ve `m_PrevWorldTransforms` sınırsız büyür.
2. **Kimlik çakışması iddiası doğrudur:** `SDFSurfaceKey::Hash()` (`SDFSceneSnapshot.hpp` satır 20–33) generational handle + sahne instance hash'idir; silinen bir varlığın slotu yeniden kullanıldığında `generation` artar ve hash değişir — bu durumda *aynı* anahtar üzerinden eski matris okunmaz, tersine yeni surfaceId çarpışması olasılığı (uint32 hash) düşüktür. Asıl risk, **kullanıcı raporunun ikinci iddiasıyla uyumlu olarak**, fallback anahtar `(0x80000000u | i)` olan kayıtlarda (ham test kaydı `surfaceId==0`), entity sıralaması değiştiğinde `i` indeksinin farklı bir nesneye kaymasıdır → yanlış önceki matris → dev motion vector. Rapor, bu iki durumu tek başlıkta harmanlamıştır ve her ikisi de doğrudur.
3. **Ek uyarı:** Fallback anahtarı `0x80000000u` bit setli olduğundan gerçek surfaceId (FNV hash) ile uzaysal çakışma ihtimali ihmal edilebilir düzeydedir.
4. `UpdateEdits(snapshot)` yolu (satır 1188–1202) aynı `UpdateEdits(records)` kodunu çağırdığı için bu sorun runtime'da kullanılan ana yolu da etkiler.

**Öncelik revizyonu:** YÜKSEK (korunur)

---
### [Bulgu 2.5] `sceneInstanceId` — Sabit `1` Hardcode

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `src/Core/Systems/RenderExtractionSubsystem.cpp` satır 10:

```cpp
m_Snapshot = SDFSceneSnapshot::Extract(context.registry, 1);
```

- `src/Core/RenderExtractionSystem.cpp` satır 97 ve 168: yine `SDFSceneSnapshot::Extract(registry, 1)`.
- `Scene` sınıfı her örnek için benzersiz `m_InstanceId` üretir (`Scene.hpp` satır 55; `Scene.cpp` satır 60–62 `GenerateInstanceId()`), ancak bu ID snapshot extraction'a aktarılmaz.
- `SDFSurfaceKey::Hash()` (`SDFSceneSnapshot.hpp` satır 20–33) `sceneInstanceId` kelimesini hash'e dahil eder; sabit `1` geçildiğinde farklı sahneler aynı `(entityIndex, generation, 1, subPrimitive)` anahtarına denk gelebilir.

**Analiz / Nüans:**

1. **Etki değerlendirmesi: doğrudur** — editor→runtime geçişinde yeni bir `Scene` örneği oluşur (`Scene::Copy/Clone`, `Scene.cpp:75-80` yeni instance ID üretir), ancak extraction `1` verdiği için surfaceId hash'leri değişmez; `SDFTemporalHistory` bu geçişi kamera tarafındaki `sceneInstance` değişiminden (`Application.cpp` `camera->sceneInstance = activeScene->GetInstanceId()`) dolayı **kısmen** kurtarabilir (kamera karşılaştırması muhafazakâr bir tetikleyicidir), fakat primitif yüzey hash'lerinin sahne değişimini yansıtamaması bir tutarsızlıktır.
2. Ek gözlem: `SDFRenderer::SetCamera` (`SDFRenderer.cpp` satır 1707–1725) `TemporalHistory->BeginFrame(... camera->sceneInstance)` çağrısında gerçek instance ID'yi kullanır; yani **kamera tarafı doğru, extraction tarafı sabit `1`** — bu ikinci ikili (düzensizlik) raporun iddiasını güçlendirir.
3. Raporun önerisi (aktif sahne instance ID'sini `Extract`'e iletmek) doğru ve uygulanabilir. Küçük uyarı: `Scene::GetInstanceId()` `uint64_t` döndürür, `Extract` `uint32_t` alır; otomatik dönüşüm öncesi açık cast gerekecektir.

**Öncelik revizyonu:** YÜKSEK (korunur)

---

### [Bulgu 2.6] IBLManager — Tek İş Parçacıklı CPU Cubemap Konvolüsyonu

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- BRDF LUT üretimi (`src/Renderer/IBLManager.cpp` satır 217–263):
  - Satır 217: `const std::filesystem::path cachePath = "assets/cache/brdf_lut_256_ggx1024_v2.bin";` — **disk önbelleği mevcut** ✓
  - Satır 230–233: "Eger onbellek yoksa **coklu is parcacigi (multi-threading) ile hesapla**" ve `std::thread::hardware_concurrency()` — **çoklu iş parçacığı mevcut** ✓
- Procedural cubemap üretimi (aynı dosya, `GenerateProceduralCubemaps` ~satır 285–652):
  - `Convolve` (satır ~372) sabit `samples = 512`.
  - İrradiance 32×32, prefilter 128×128 + 5 mip; **`std::thread` yalnızca BRDF LUT bloğunda kullanılır** (grep sonucunda satır 232 dışında thread kullanımı yok), cubemap konvolüsyon döngüleri tek iş parçacığındadır.
  - Diske önbelleğe alma cubemap'ler için yoktur (cachePath yalnızca BRDF LUT içindir).

**Analiz / Nüans:**

1. Raporun "BRDF LUT çoklu iş parçacığı + disk önbelleği; cubemap'ler tek iş parçacığı + önbelleksiz" ayrımı **birebir doğrudur**.
2. **İşlem sayısı: raporun "~20 milyon" ifadesi biraz eksiktir.** 512 örnek ile:
   - İrradiance: 6 yüz × 32×32 × 512 ≈ **3,1 M**
   - Prefiltered: 128×128×6×512 ≈ 50,3 M; 64×64×6×512 ≈ 12,6 M; 32×32×6×512 ≈ 3,1 M; 16×16×6×512 ≈ 0,8 M; 8×8×6×512 ≈ 0,2 M
   - Toplam **~70 milyon** `Convolve` örneği (her biri trigonometri içeren `SampleRadiance`/`EvaluateSkyRadiance` çağrısı). Sayısal yük sanılandan ~3,5 kat fazladır; ana tez (tek iş parçacığında bloklama, 4–8 sn) güçlenir.
3. `LoadEnvironment` (`SDFRenderer.cpp` satır 1693–1699) ana iş parçacığında çalışır ve `m_Device.waitIdle()` + `IBLManager` swap yapar; `IBLManager` constructor'ı içinde procedurals hesaplandığı için motor başlatılırken veya çalışma sırasında `LoadEnvironment` çağrısı ana iş parçacığını bloklar — iddia doğrudur.
4. Raporun önerileri (ParallelFor, disk önbelleği, GPU compute pass) geçerlidir; GPU path'i en yüksek kazanımlıdır.

**Öncelik revizyonu:** YÜKSEK (korunur)

---
## 4. Orta Seviye Bulguları Doğrulama

### [Bulgu 3.1] TransformSystem — `std::unordered_set` ile Her Entity İçin Heap Alloc

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `src/Core/TransformSystem.cpp` satır 49–52:

```cpp
glm::mat4 GetWorldTransformMatrix(const Registry& registry, EntityHandle entity) {
    std::unordered_set<EntityHandle> visiting;
    return GetWorldTransformMatrixRecursive(registry, entity, visiting);
}
```

- `UpdateWorldTransforms` (satır 54–65) her `TransformComponent` için `GetWorldTransformMatrix` çağırır. Ayrıca `SDFSceneSnapshot::Extract` (satır 57) her entity için aynı fonksiyonu çağırır.

**Analiz / Nüans:**

1. **Heap alloc iddiası doğrudur:** `std::unordered_set<EntityHandle>` her çağrıda ayrı tahsis yapar.
2. **Karmaşıklık iddiası (O(N·D)) doğrudur:** memoization olmadığından aynı üst düğüm birçok çocuk için yeniden özyinelemeli hesaplanır.
3. Öneri (topological order, `parent.world * child.local`, sıfır heap alloc) doğru ve standart bir DOD düzelmesidir. Küçük not: cycle koruması için `visiting` kümesi gerekli olduğundan, düz dizide topolojik sıralama yapılırsa bu koruma derinlik sırasıyla otomatik sağlanır (ekstra set gerekmez).

**Öncelik revizyonu:** ORTA (korunur)

---

### [Bulgu 3.2] Debug Modları — Üç Farklı Header'da Çelişkili Tanımlar

**Sonuç: ✅ DOĞRULANDI**

**Kanıt (farklı tanımlar):**

1. `include/Astral/Renderer/QualitySettings.hpp` satır 53–62: `0 Shaded, 1 Surface Identity, 2 Geometry Revision, 3 Temporal Confidence, 4 Rejection Reason, 5 Changed Region Mask, 6 Shadow Visibility, 7 Ambient Occlusion` (8 yok).
2. `include/Astral/Core/Application.hpp` satır 60: `0=Shaded, 1=SurfaceId, 2=PrimitiveIndex, 3=TemporalConf, 4=RejectionReason, 5=ChangedRegion, 6=Shadows, 7=AO, 8=Material`.
3. `include/Astral/Renderer/SDFRenderer.hpp` satır 105 (`SetDebugMode` yorumu): `0: Shaded, 1: Albedo, 2: Normal, 3: Depth, 4: Motion, 5: Material`.
4. `shaders/SDFDebugComposite.glsl` satır 11–20: `0..8` (Final Shaded, Surface Identity, Geometry Primitive Index, Temporal Confidence, Rejection Reason, Changed Region Mask, Shadow Visibility, Ambient Occlusion, Material Parameters).

**Analiz / Nüans:**

1. **Gerçek shader 0–8 arası modları destekler** ve `Application::SetDebugMode` (`Application.cpp:443-449`) `m_Config.debugMode` ile `SDFRenderer::SetDebugMode`'u besler; `SDFRenderer::Render` bu değeri doğrudan `DebugCompositePushConstants.screenRes.z`'ye yazar (satır 1430–1435). Yani **shader doğru modları üretir; sorun belgelenen API yorumlarının çelişmesidir**.
2. Raporun **"AO/Shadow/Temporal Confidence dokuları DebugComposite descriptor set'inde yok"** iddiası doğrudur: `UpdateDebugCompositeDescriptorSets` (`SDFRenderer.cpp` satır 980–1026) yalnızca Albedo, Normal, Material, Depth, Motion bağlar. Ancak **"siyah/belirsiz çıktı" etkisi abartılıdır**: `SDFDebugComposite.glsl` mod 6/7/3 için G-Buffer'den türetilmiş yaklaşık gösterim üretir (mod 6: `dot(N, L)`; mod 7: `1/(1+depth*0.1)`; mod 3: `1 - length(motion)*20`). Yani ekranda siyah çıkmaz; **gerçek analiz değerleri değil, yanlış anlaşılabilecek yaklaşık değerler** gösterilir.
3. `SetDebugMode` yorumunun (SDFRenderer.hpp) shader mod sözleşmesiyle (GLSL 0–8) uyumsuz olması yanıltıcıdır.

**Öncelik revizyonu:** ORTA (korunur) — ancak gerekçe "siyah ekran bug" değil "dokümantasyon/API güvenilirlik" olarak güncellenmeli.

---
### [Bulgu 3.3] JobSystem — Timeout Durumunda Work-Helping Devre Dışı

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `src/Core/Threading/JobSystem.cpp` satır 88–104:

```cpp
while (handle->count.load(std::memory_order_acquire) > 0) {
    if (hasTimeout) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime) >= timeout) {
            return false;
        }
        std::this_thread::yield();
    } else {
        // Work-Helping: Bekleyen thread bosta durmaz, kuyruktan is calistirir
        if (!ExecuteOneJob()) {
            std::this_thread::yield();
        }
    }
}
```

- Timeout verildiğinde (ki `TaskGraph::Wait` varsayılan olarak `10000ms` kullanır — `TaskGraph.hpp` satır 43) `ExecuteOneJob()` **çağrılmaz**, sadece `yield()` yapılır.

**Analiz / Nüans:**

1. İddia doğrudur: ana iş parçacığı timeout durumunda work-helping yapmaz.
2. **Nüans:** `yield()` CPU yakmaz (spin-yield değil; yalnızca zaman dilimi devri); raporun "CPU zamanını boş yere tüketir" ifadesi yalnızca düşük çekirdekli sistemlerde işçi thread'lerin tek başına yetersiz kaldığı durumlarda gecikme olarak açığa çıkar. Öneri (timeout durumunda da `ExecuteOneJob()`) doğrudur.
3. Küçük detay: timeout aşılırsa `Wait` false döner ve iş kuyrukta kalmış olabilir; istemcinin bu durumu yönetip yönetmediği ayrı bir inceleme konusudur.

**Öncelik revizyonu:** ORTA (korunur)

---

### [Bulgu 3.4] `EvaluateSDFSoftShadow` — Adım Sınırı `clamp(h, 0.015, 0.5)`

**Sonuç: ✅ KOD DOĞRULANDI (etki tartışmalı)**

**Kanıt:**

- CPU: `include/Astral/Geometry/SDFVisibility.hpp` satır ~149:

```cpp
t += std::clamp(h, 0.015f, 0.5f);
```

- GPU: `shaders/SDFVisibility.glsl` satır 101: aynı `t += clamp(h, 0.015, 0.5);`.

**Analiz / Nüans:**

1. Kod iddiası birebir doğrudur: minimum adım mesafesi 1.5 cm'dir.
2. **Etki değerlendirmesi iki yönlüdür:**
   - `0.015` alt sınırı `surfaceBias = 0.015` ile tutarlıdır (gölge ışını yüzeyden bu kadar yukarıda başlar). Alt sınırı `0.001`'e düşürmek ince ayrıntılı temas gölgesi üretebilir; ancak daha küçük adımlar aynı `maxSteps=96` içinde daha kısa menzil ve daha fazla hesaplama demektir. Ayrıca `0.001` seviyesinde `k*h/t` oranı yüzey gürültüsüne duyarlı hale gelir.
   - Işık sızıntısı/ince duvar atlama riski gerçektir, fakat yalnızca bu sabiti değiştirmek gözlemlenebilir iyileşme sağlamayabilir (penumbra kalitesi daha çok `k` ve `maxSteps` ile belirlenir).
3. Raporun önerisi makuldür; benchmark ile doğrulanarak uygulanması önerilir.

**Öncelik revizyonu:** ORTA → **DÜŞÜK-ORTA** (hassas-ayar kategorisi)

---
## 5. Düşük Seviye Bulguları Doğrulama

### [Bulgu 4.1] `WorldTransformComponent` Bellek Fazlalığı (Ölü Alanlar)

**Sonuç: ✅ DOĞRULANDI (ancak "kaldırın" önerisi eksiktir)**

**Kanıt:**

- `include/Astral/Core/Components.hpp` satır 43–49:

```cpp
struct WorldTransformComponent {
    glm::mat4 matrix{1.0f};
    glm::vec3 renderedPosition{0.0f};
    glm::vec4 renderedRotation{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec3 renderedScale{1.0f};
    bool hasRenderHistory = false;
};
```

- Bu alanların yazıldığı/okunduğu **tek** yer: `src/Core/RenderExtractionSystem.cpp` satır 138–146 (Legacy `ExtractRenderData(..., std::vector<LegacySDFEdit> ...)` yolu).
- Runtime'da asıl kullanılan yol `SDFSceneSnapshot::Extract` (`RenderExtractionSubsystem.cpp:10` → `SDFRenderer::UpdateEdits(snapshot)`) olup bu yol `LegacySDFEdit` / `SDFEditGPU` prototipini kullanmaz; `renderedPosition` vb. alanlar **runtime'da okunmaz/yazılmaz** — iddia doğrudur.

**Analiz / Nüans:**

1. Alan başına 48 byte fazlalık (3×12 + 16 + 4) ve L1 cache kirletme iddiası nicelik olarak doğrudur (matris 64 B + 48 B).
2. **Nüans:** Kaldırma önerisi tek başına uygulanırsa `RenderExtractionSystem.cpp` içindeki legacy overload'lar derlenmeyecek (`hasRenderHistory` vb. kullanımlar kırılır). Doğru sıralama: önce legacy `ExtractRenderData`+`LegacySDFEdit`'in kullanılmayan yollarını kaldırmak, ardından alanları silmek. Ayrıca `m_PrevTransformBuffer`/Snapshots tarafındaki önceki kare transform'u `m_PrevWorldTransforms` ile ayrıca tutulduğundan bu alanlar gerçekten gereksizdir.
3. Raporun ana tespiti geçerlidir; öneri sıralaması netleştirilmelidir.

**Öncelik revizyonu:** DÜŞÜK (korunur)

---

### [Bulgu 4.2] `IUpscaler` Ölü Arayüz

**Sonuç: ✅ DOĞRULANDI**

**Kanıt:**

- `include/Astral/Renderer/IUpscaler.hpp` tamamen durur; `git grep` sonucunda `IUpscaler`/`UpscalerType`/`UpscalerParameters` referansları **yalnızca bu header içinde** geçiyor; `src/`, `tools/`, `Tests/` içinde hiçbir türetilmiş sınıf, `Evaluate` çağrısı veya enjeksiyon noktası yok.
- `SDFRenderer`'da upscaler entegrasyonu yok (yalnızca TAA + storage image blit hattı).

**Analiz / Nüans:**

1. İddia doğrudur: FSR2/DLSS için tanımlanan arayüz somutlaştırılmamıştır.
2. Raporun önerisi (Faz 3'e kadar kaldırın veya TODO/DRAFT modülüne taşıyın) makuldür; ancak yol haritasında Faz 3 hedeflendiğinden header'ın `IUpScaler` olarak korunması ama `DRAFT` işaretlenmesi daha az yıkıcı olabilir.

**Öncelik revizyonu:** DÜŞÜK (korunur)

---
## 6. Hybrid Mesh Mimarisine Geçiş Engellerinin Doğrulanması

Raporun III. bölümündeki dört engel koda karşı doğrulanmıştır:

1. **G-Buffer rasterization entegrasyon noktası yok — ✅ DOĞRU.** `SDFRenderer` içinde tek bir `vkCmdBeginRendering` / dynamic rendering raster pass'i bulunmamaktadır; G-Buffer tamamen `SDFGBuffer.glsl` compute shader'ının `imageStore`'u ile üretilir (`SDFRenderer.cpp:1300-1345`). Swapchain üzerine tek çizim curve'tı blit/vulkan-renkte ImGui dokusudur.
2. **Derinlik tamponu uyumsuzluğu — ✅ DOĞRU.** Derinlik `VK_FORMAT_R32_SFLOAT` renk görüntüsü olarak tutuluyor (`CreateGBufferTexture` ile `SDFRenderer.cpp:290-291`), donanımsal `DepthStencil` attachment yok. Mesh rasterizer'ın early-Z entegrasyonu için gerçek bir `VkFormat::eD32Sfloat` depth attachment gerekecektir.
3. **Kamera View-Projection ve NDC karmaşası — ✅ DOĞRU (sınırlı kapsamda).** `SetCameraMatrices` (`SDFRenderer.cpp:1727-1733`) Vulkan NDC için `proj[1][1] *= -1` yapar; `RenderExtractionSystem.cpp::ExtractActiveCamera` ise OpenGL konvansiyonunda `glm::perspective` üretir. Aradaki dönüşüm yalnızca `SetCameraMatrices` içinde merkezlenmiştir (tek noktada) — raporun "CPU extraction tarafındaki bazı yardımcılar OpenGL convention kullanmaktadır" ifadesi doğrudur. Ayrıca `SDFGBuffer.glsl` satır 176 `uv.y = -uv.y` ile ekran uzayını çevirir; raster/NDC konsolidasyonu gereklidir.
4. **Surface ID sözleşmesi yok — ✅ DOĞRU.** `SDFSurfaceKey::Hash()` yalnızca ECS tabanlı primitive kimliği üretir; meshlet/drawcall instance ID'leri için aynı 32-bit alana yazılabilecek ortak bir kimlik sözleşmesi (`SDFPrimitiveRecord.surfaceId`, 124. offset) mevcut değildir.

**Genel değerlendirme:** Hybrid mesh geçişi için saptanan engeller gerçektir; ancak bunlardan **1, 2, 4** aslında aynı mimari kararın parçasıdır (pure-compute G-Buffer → hybrid raster+compute). En önemli ilk adım, kamera/screen-space karmaşasının (madde 3) tek standarda konsolide edilmesidir.

---

## 7. Ek Tespitler (Sağlanan Raporda Yer Almayan)

Doğrulama sırasında aşağıdaki ek bulgular kaydedilmiştir:

1. **`SDFChangeSet.cpp::GetLocalBounds` hazırdır ama BrickGrid tarafından kullanılmaz.** Bulgu 1.1'in düzeltmesi neredeyse sıfır maliyetlidir (dosyalarda ayrı ayrı duran iki mantık birleştirilmeli).
2. **`SDFRenderer.hpp` satır 182–184 yanıltıcı yorum:** `m_GBufMaterial; // VK_FORMAT_R8G8B8A8_UNORM` — gerçek format `R32G32B32A32_UINT`. (Bulgu 2.3 ile ilgili.)
3. **`Application.cpp:282-294` artık snapshot tabanlı yolu kullanıyor** (`m_RenderExtractionSubsystem->GetLastSnapshot()` → `m_SDFRenderer->UpdateEdits(snapshot)`). Bu, kök dizindeki eski `ARCHITECTURE_ANALYSIS_REPORT.md` (06.09.2026) içindeki "Dual Representation Drift — runtime eski yolda, `surfaceId = i+1` sahte" iddialarıyla **çelişir**; o doküman güncel koddan önceki durumu anlatmaktadır. `surfaceId = i+1` sahteliği yalnızca `BrickGrid::Build(std::span<const LegacySDFEdit>)` (satır 57-64) gibi legacy girişlerinde kalmıştır.
4. **`QualitySettings` artık push constant'lara aktarılıyor** (`SDFRenderer.cpp:1472-1482` — `qs` seçimi ve `defPush.rayParams/shadowAOParams/qualityParams`). Eski rapordaki "QualitySettings hardcoded" iddiası güncel değildir.
5. **`RenderContext.hpp` (satır 12-20) `activeScene` içerir** ancak `FrameContext` ile `RenderContext` ayrık durmaktadır; sahne/snapshot farkı `RenderExtractionSubsystem` üzerinden köprülenir — bu köprüde `sceneInstanceId` kaybı (Bulgu 2.5) bu yüzden sinsidir.

---

## 8. Evrişmiş Öncelik Matrisi (Doğrulama Sonrası Öneri)

| Öncelik | Bulgu | Durum |
| :--- | :--- | :--- |
| **KRİTİK** | 1.1 BrickGrid (smoothExpansion + AABB) | ✅ Onaylandı — PR-6'yı devre dışı bırakır, ciddi görsel hatalar üretebilir |
| **KRİTİK** | 1.2 VulkanContext lock-step fence bekleme | ✅ Onaylandı — CPU-GPU paralelliği sıfır |
| **YÜKSEK** | 2.3 G-Buffer format/layout israfı | ✅ Onaylandı — bellek + aktarım yükü |
| **YÜKSEK** | 2.4 PrevWorldTransforms sızıntısı/kimlik kayması | ✅ Onaylandı |
| **YÜKSEK** | 2.5 sceneInstanceId hardcode | ✅ Onaylandı — sahne geçişlerinde ghosting riski |
| **YÜKSEK** | 2.6 IBL tek iş parçacıklı konvolüsyon | ✅ Onaylandı (~70M örnek) |
| **YÜKSEK** | 1.3 Resize / swapchain blit | ⚠️ Editor'de kısmen çalışıyor; standalone istemciler için eksik |
| **ORTA** | 1.4 spvPath yutulması | ⚠️ Kod var, etki dar |
| **ORTA** | 2.1 Push constant dizilim farkı | Aktif bug değil; bakım riski |
| **ORTA** | 2.2 Exposure çiftleme riski | Latent risk; sözleşme netleşmeli |
| **ORTA** | 3.1 TransformSystem heap alloc / O(N·D) | ✅ Onaylandı |
| **ORTA** | 3.2 Debug modu tanım çelişkileri | ✅ Onaylandı (dokümantasyon/API) |
| **ORTA** | 3.3 JobSystem timeout work-helping | ✅ Onaylandı |
| **DÜŞÜK-ORTA** | 3.4 Shadow adım alt sınırı | Kod doğru; etki bağlama bağlı |
| **DÜŞÜK** | 4.1 WorldTransform ölü alanlar | ✅ Onaylandı (kaldırma sıralamasına dikkat) |
| **DÜŞÜK** | 4.2 IUpscaler ölü arayüz | ✅ Onaylandı |

---

## 9. Sonuç

Sağlanan inceleme raporundaki **tüm kod alıntıları gerçek koddan doğrulanmıştır**; tespitlerin hiçbiri uydurma değildir. Ana düzeltmeler:

1. Raporun "dosya adı" nitelikleri bazen yanlıştır (`SDFLighting.hpp` yok → `SDFVisibility.hpp`).
2. Etki değerlendirmelerinin bir kısmı bağlamı genişletmektedir:
   - 1.3 (resize/blit) editor'de kısmen çalışıyor,
   - 1.4 (spvPath) yalnızca GBuffer shader'ını etkiliyor,
   - 2.1 (push constant) mevcut kodda **aktif** bir kırılmaya yol açmıyor,
   - 2.2 (exposure) şu an çift uygulanmıyor (latent),
   - 3.2 (debug modları) siyah ekran değil, yaklaşık/değişken gösterim riski.
3. En kritik ve acil onarımlar sırası: **1.1 + 1.2 → 2.4 + 2.5 → 2.3 + 2.6 → 1.3.** Kalan bulgular bakım ve sağlamlaştırma kapsamındadır.

*Bu rapor hiçbir kod değişikliği içermez; yalnızca mevcut kaynak kodun doğrulanmış durumunu belgeler.*

---