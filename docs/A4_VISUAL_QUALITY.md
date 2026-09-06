# A4 — Görsel kalite ve doğrulama sözleşmesi

## 6 Eylül 2026 Nihai A4 Doğrulama Sonucu (Genişletilmiş Kapsam P1–P7)

Release / NVIDIA GeForce RTX 3060 / Vulkan 1.4.351 üzerinde:

- **CPU Testleri:** 19/19 test grubu, 672/672 assertion %100 geçti (`ctest -L CPU` 2.26s).
  - Şekil/Transform ayrımı (`SDFShapeTests`): 169/169 assertion geçti.
  - Ortak Geometri Çekirdeği (`SDFContractTests`): 10.000 noktalı Lipschitz ve analitik testler 0 ihlalle geçti.
  - Deferred Gölge & AO (`DeferredLightingTests`): 21/21 assertion geçti.
  - Temporal Güven (`SDFTemporalTests`): 18/18 assertion geçti.
  - Yerel Değişim Seti & Invalidation (`SDFChangeSetTests`): 19/19 assertion geçti (kapı oyma ROI reddi, uzak geçmiş korunumu ≥ %99).
- **GPU Testleri:** 3/3 GPU testi %100 geçti (`ctest -R "EngineTests\.GPU"` 43.34s).
  - `EngineTests.GPU.Smoke`: 5 kare sorunsuz render, 0 validation hatası, negatif hata sözleşmesi doğrulandı.
  - `EngineTests.GPU.VisualQuality`: 6 SDF varyantı, grid on/off, camera cut, exposure sıfır ve 6 hareket senaryosu (pan, thin, metal, fast, disocclusion, resize) geçti.
  - `EngineTests.GPU.Camera`: Sandbox ileri/deferred render, boş sahne, kamera kaldırma, hareket ve geniş FOV regresyonları başarıyla doğrulandı.
- **IBL ve Radyometrik Doğruluk:**
  - Sabit HDR tüm mip/yüzler: maksimum hata **0**.
  - BRDF LUT bağımsız referans: maksimum hata **0.00551628** (tolerans ≤ 0.025).
  - Yönlü HDR analitik diffuse referansı: maksimum hata **0.00431317** (tolerans ≤ 0.015).
- **Grid Doğruluğu:** Tüm fixture'lar geçti; forward MAE **0.0415755**, deferred MAE **0.019592**, aykırı kanal oranı ≤ **%0.1** (kabul sınırı ≤ %1.0).
- **Kamera Kesimi:** Her iki render yolunda MAE **0**, birebir deterministik eşleşme.
- **12 Hareket Dizisi / 384 Kare:** `artifacts/a4/captures/` altında kayıpsız PNG'ler, animated GIF'ler, contact sheet'ler ve `index.html` oynatıcı üretildi.

## Uygulanan Değişiklikler ve Mimari (P1–P6)

1. **Şekil Parametreleri / Transform Ayrımı (P1):**
   - Şekil boyutları `SDFShapeParameters` bileşeninde toplanarak `TransformComponent::scale` uzamsal dünya çarpanından ayrıldı.
   - Geriye dönük tam uyumluluk sağlandı: Eski sahneler otomatik `LegacyPackedScale` olarak okunur, yeni authoring `ExplicitShape` kullanır.
2. **Ortak Geometri Çekirdeği (P2):**
   - `include/Astral/Geometry/SDFKernel.inl` başlığı CPU (C++20) ve GPU (Vulkan GLSL) tarafından birebir paylaşılan Lipschitz-uyumlu (\|grad\| ≤ 1) primitive ve CSG denklemlerini içerir.
   - Deterministik CSG sıralaması ve `SDFSurfaceKey` yüzey kimliği garantilendi.
3. **Deferred SDF Soft Shadow ve Multi-Tap AO (P3):**
   - Deferred ana render yoluna cone-traced analitik SDF yumuşak gölgeler (`shadowMaxSteps=96`, `shadowMaxDistance=50m`) ve normal yönlü çoklu örneklemeli AO (`aoSamples=8`, `aoRadius=1m`) eklendi.
   - Aydınlatma sözleşmesi kesinleştirildi: Doğrudan ve dolaylı bileşenler ayrıldı; IBL ortam ışığı gölgelerden etkilenmez, AO ortam ışığını maskeler.
4. **SDF Temporal Geçmiş ve Reaksiyon (P4):**
   - `SDFTemporalHistory` modülü ile render geçmişi (history render commit) bağımsızlaştırıldı.
   - Aydınlatma ve derinlik delta sinyali ile TAA geçmiş ağırlığı dinamik olarak uyarlandı.
5. **Yerel Değişim Seti ve Invalidation (P5):**
   - `SDFChangeSet` ardışık sahneler arasındaki farkı çıkararak primitive dünya sınırlarını ve ekran UV diktörtgenlerini (maksimum 4 rect) hesaplar.
   - TAA resolve aşaması değişen bölgelerde geçmişi sert olarak reddeder (hard rejection); uzak kontrol bölgelerindeki geçerli piksellerin ≥ %99'u korunur.
6. **Kullanılabilirlik ve Teşhis (P6):**
   - `QualitySettings` merkezi yapılandırıldı.
   - 8 debug modu (`SDFDebugComposite.glsl`) hem editör menüsüne hem de Sandbox CLI argümanlarına bağlandı:
     0: Final Shaded, 1: Surface Identity, 2: Geometry Revision, 3: Temporal Confidence,
     4: Rejection Reason, 5: Changed Region Mask, 6: Shadow Visibility, 7: Ambient Occlusion.

## Aydınlatma Özellik Matrisi

| Özellik | Forward (`useGBuffer=false`) | Deferred (`useGBuffer=true` — Ana Yol) |
|---|---|---|
| Materyal | Eski diffuse/specular yaklaşımı | Cook–Torrance GGX, roughness/metallic |
| Yönlü ışık | Shader'da sabit yön/renk | `LightGPU`, `position.w=0`; `direction.xyz` yön, `color` |
| Noktasal ışık | Yok | `LightGPU`, `position.w=1`; `color.w` menzil |
| Noktasal zayıflama | Yok | `1 / (1 + distance² / range²)` |
| SDF Gölge | Sabit yönlü ışık için SDF soft shadow | Cone-traced SDF soft shadow (yönlü & noktasal, 96 adım) |
| SDF AO | SDF tabanlı | Multi-tap normal yönlü SDF ambient occlusion (8 örnek) |
| IBL / Environment | Kullanılmaz | Diffuse convolution + GGX prefilter + BRDF LUT |
| Temporal veri | TAA açıkken G-buffer ön geçişinden | G-Buffer + SDF Temporal History |
| Değişim Rejeksiyonu | Yok | `SDFChangeSet` ekran-uzayı yerel invalidation maskesi |
| Exposure / Tonemap / sRGB | Ortak resolve | Ortak resolve (ACES Fitted + IEC 61966-2-1 sRGB transfer) |

`optShadow` gölgeyi kapatan bir kalite seçeneği değildir; forward gölge hesaplamasının erken çıkış/back-face optimizasyonudur. Deferred yol bu seçeneği kullanmaz. İki yolun süreleri **eşdeğer kalite performansı** olarak sunulmamalıdır. TAA kapalı ölçümler, `no-AA` kalite profiliyle etiketlenmelidir.

## HDR ve IBL

`SDFRenderer::LoadEnvironment(path)` render/main thread üzerinde, kare komutu kaydedilmezken çağrılır. Yeni kaynaklar hazırlandıktan sonra cihaz beklenir, kaynaklar değiştirilir ve descriptor/history güncellenir. Dosya veya çözümleme hatası mevcut ortamı değiştirmez. Bu senkron başlangıç/import yoludur; kare içi streaming değildir.

Desteklenen giriş: Radiance RGBE `.hdr`, `-Y +X` equirectangular yönelimi, modern scanline RLE veya düz RGBE pikseller. EXR ve diğer eksen sıraları desteklenmez. Linear radiance korunur; import sırasında exposure veya tonemapping uygulanmaz. RGBA16F aralığını aşan veriler sessizce kırpılmak yerine reddedilir.

- BRDF LUT: 256×256 RG16F, 1024 GGX importance sample. Eski 128 örnekli LUT bağımsız referans toleransını aşmıştır; yeni önbellek adı `brdf_lut_256_ggx1024_v2.bin`'dir.
- Diffuse cube: 32×32, cosine-weighted hemisphere convolution, 512 örnek. Shader sözleşmesi gereği irradiance/π depolanır.
- Specular cube: 128×128, 5 mip, roughness `mip / 4`, 512 GGX örnek. Mip 0 doğrudan ortamı örnekler. Önceki “sharp ile diffuse rengi karıştır” yaklaşımı kaldırılmıştır.
- Cubemap yüzlerinin dikey koordinat işareti düzeltilmiştir. Prosedürel varsayılan ortam da aynı konvolüsyon yolunu kullanır.

## SDF sözleşmesi

Sphere artık üç yarıçaplı ellipsoid olarak değerlendirilir; mesafe, minimum eksenle ölçeklenmiş muhafazakâr distance estimator'dır. Box üç half-extent kullanır. Torus, capsule, cylinder ve plane mevcut şekil parametresi sözleşmelerini korur: torus `scale.xy = major/minor radius`, capsule/cylinder `scale.xy = radius/height`, plane `scale.y = local offset`. Bunlar genel affine deformasyon desteği anlamına gelmez; özellikle shear veya torusun üçüncü eksende deformasyonu bu çalışmanın garantisi değildir.

Grid dışında gerçek SDF değerlendirilir. Grid sınırı dünya sınırı değildir. Hücre mesafeleri muhafazakâr primitive sınırları, döndürülmüş plane, hücre yarı köşegeni ve smooth-union genişleme payını içerir. Uzak hücreler 1 metreyle sınırlandırılır; böylece eski/yeni nesne çevresinin kısmi güncellemesi uzaktaki bayat mesafelerle güvenli olmayan sıçrama oluşturmaz. Bu doğruluk tercihinin performans etkisi ayrıca ölçülmelidir.

## Renk hattı

`linear materyal/ışık → linear RGBA16F → linear temporal history → exposure → ACES fitted → gerçek parçalı sRGB transfer → RGBA8 UNORM`.

`SetExposure()` linear çarpan alır; EV dönüşümü istemcide `exp2(EV)` ile yapılabilir. Exposure history'ye yazılmaz. Debug composite renk işlemlerini bypass eder. Sunum yolu sRGB-nonlinear renk uzayında RGBA8/BGRA8 **UNORM** surface seçer; sRGB attachment'a ikinci encode yapabilecek sessiz fallback kaldırılmıştır. Yalnız sRGB formatlı surface sunan platform için ayrı bir sunum yolu gerekir.

## Otomatik kabul ve kayıtlar

`VisualQualityGpuTests` sabit fixture ile aşağıdaki kontrolleri yürütür:

| Kontrol | Kabul sınırı |
|---|---|
| Grid açık/kapalı; altı primitive, rotated plane, smooth union/subtraction | RGBA8 MAE ≤ 0.5; farkı 8'den büyük kanallar ≤ %1 |
| Kamera kesimi sonrası temiz kare | Birebir eşleşme |
| Sıfır exposure | RGB tamamen sıfır |
| Sabit HDR'nin diffuse ve tüm specular mip/yüzleri | Linear mutlak hata ≤ 0.003 |
| BRDF LUT, 9 farklı NdotV/roughness noktası, bağımsız 65.536 örnekli solid-angle integral | Kanal başına mutlak hata ≤ 0.025 |
| Yönlü HDR diffuse integrali, analitik `constant + 2/3 × directional` referansı | Mutlak hata ≤ 0.015 |

Kayıtlar: iki render yolu × pan / thin / metal / fast / disocclusion / resize × 32 kare. Normal boyut 320×180; resize sırasında 384×180. Simülasyon 60 Hz; inceleme sayfası yarı hızda 30 fps oynatır. Bunlar kısa tanı kayıtlarıdır; uzun oyun oturumu veya bütün hedef GPU'lar için kalite sertifikası değildir.

Çalıştırma:

```text
cmake --build build-release --target EngineTests VisualQualityGpuTests CameraGpuTests
build-release/EngineTests
build-release/VisualQualityGpuTests artifacts/a4/captures
python tools/visual_quality_report.py artifacts/a4/captures
```

`artifacts/a4/captures/index.html` kayıpsız PNG karelerini yan yana oynatır ve tek kare seçimine izin verir. GIF'ler kolay önizleme içindir; karşılaştırma girdisi orijinal PPM/PNG'dir. `brdf_reference.csv` GPU değerleri ve bağımsız referansı saklar.

Görsel incelemede özellikle 15→16 disocclusion geçişi, hızlı dönen nesnenin arka kenarı, ince sütunun pan sırasındaki sürekliliği ve resize sonrası ilk kare incelenmelidir. Testlerin geçmesi tek başına bütün hareketli içerikte ghosting bulunmadığını kanıtlamaz. Smooth CSG yüzeylerinin attribution değişimi ve çok parlak küçük HDR kaynakları daha geniş üretim regresyon setinde izlenmelidir.
