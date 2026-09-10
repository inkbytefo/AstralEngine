# Render mimarisi incelemesi ve refaktör tasarımı

Tarih: 2026-09-11. Durum: tasarım önerisi; uygulama henüz başlamadı.

Uçtan uca görevler, API sözleşmeleri, test adımları ve başka AI ajanına devir metni: [RENDER_REFACTOR_IMPLEMENTATION_PLAN.md](RENDER_REFACTOR_IMPLEMENTATION_PLAN.md). Kullanıcının son talebi doğrultusunda bu aşamada yalnız dokümantasyon hazırlanmıştır.

## Mevcut akış ve bulgular

Application, RenderExtractionSubsystem üzerinden SDFSceneSnapshot alıyor; kamera ve jitter ayarlıyor; SDFRenderer üzerinden primitive ve önceki dönüşüm verilerini GPU'ya yüklüyor. Renderer GBuffer → deferred lighting veya debug composite → TAA/tonemap sırasını kaydediyor. VulkanContext submit, sunum ve fence beklemesini yönetiyor. Editör RenderContext üzerinden ek çizim yapıyor.

- src/Renderer/SDFRenderer.cpp yaklaşık 1.750 satır. Görüntü oluşturma/yıkma, descriptor kurulumu, dört pipeline, sahne yükleme, picking, kamera ve temporal durum aynı sınıfta. Render fonksiyonu tek başına yaklaşık 430 satır.
- Shader yol çözümü dört kez tekrarlanıyor. CreateTexture ve CreateGBufferTexture benzer görüntü oluşturma işlemleri içeriyor. Pipeline ve descriptor kurulumları renderer'a gömülü.
- ComputePipeline.hpp hem pipeline sınıfı hem kamera, ışık, seçim ve dört shader aşamasının GPU veri sözleşmelerini içeriyor. Veri yerleşimi ile Vulkan nesne kurulumu farklı sorumluluklar.
- VulkanContext, hem EndAndSubmitFrameCommand hem EndFramePresent sonunda fence bekliyor. Bu, kareler arasında CPU/GPU örtüşmesini sınırlıyor. Beklemeyi kaldırmak tek başına güvenli değil: upload tamponları, komut tamponu, picking okuması ve temporal hedeflerin kullanım ömrü birlikte ele alınmalı.
- UpdateEdits her dolu güncellemede önceki matrisler için vektör oluşturuyor, her primitive için ters matris hesaplıyor ve tüm primitive aralığını yüklüyor. Snapshot kopyası da tutuluyor. Bunlar ölçülecek CPU ve aktarım maliyeti adayları; henüz performans kazancı ölçülmedi.
- BrickGrid zaten değişim temelli hücre güncellemesi yapıyor; bu özellik korunmalı. UploadGridBuffer geçici byte vektörü oluşturup tüm grid tamponunu yüklüyor.
- ResetTemporalHistory, Resize ve kamerasız render farklı durum kümelerini sıfırlıyor. Geçmişin geçersizleştirilmesi tek bir politika altında toplanmalı.
- Render için qualitySettings parametresi ve sınıfta m_QualitySettings birlikte bulunuyor. Deferred aşamasında shadowMaxSteps üzerinden fallback seçilirken diğer parametreler doğrudan kullanılıyor. Etkin ayarlar kare başında bir kez çözülmeli.
- IUpscaler arayüzü bulunuyor ancak SDFRenderer'ın TAA kaynakları ve dispatch akışı kendi içinde. Yeni bir harici upscaler eklemek bu refaktörün zorunlu parçası değil.
- CPU testleri, GPU smoke, görsel kalite, kamera ve SDF GPU sözleşme testleri mevcut. Başlangıçta mevcut Release çıktıları üzerinden 20 CPU CTest kaydı geçti. Kaynak derlemesi ve GPU sonuçları ayrıca raporlanacak.

## Yaklaşım seçenekleri

1. Önerilen: mevcut Vulkan ve deferred SDF algoritmalarını koruyarak bileşim yoluyla mimariyi yeniden kurmak. SRP/DRY sınırları gerçek sahiplikle ayrılır; her aşama doğrulanabilir.
2. Genel amaçlı dinamik render graph ve çoklu backend: daha geniş genişletilebilirlik sağlar; mevcut dört aşamalı akış için ek karmaşıklık ve doğrulama yükü getirir.
3. Renderer ve shader algoritmalarını birlikte sıfırdan yazmak: görsel karşılaştırmayı ve hata izolasyonunu zorlaştırır. Önce mimari ve ölçüm altyapısı kurulmalı.

## Önerilen modüller

| Modül | Tek sorumluluk | Sahiplik / bağımlılık |
|---|---|---|
| SDFRenderer | Uygulama ve editöre uyumlu dış cephe; kare akışını koordine etme | Aşağıdaki modülleri bileşimle kullanır |
| RenderFrameSettings | Kamera, boyut, kalite, jitter ve debug için tutarlı kare girdisi | GPU nesnesi içermez |
| ShaderInterop | GPU veri yerleşimleri, boyut ve offset kontrolleri | Pipeline sınıfından bağımsız veri tanımları |
| ComputeProgram | SPIR-V yükleme, layout ve compute pipeline yaşam döngüsü | Vulkan device; pass tarafından sahiplenilir |
| RenderTargets | GBuffer, HDR, çıktı ve history görüntülerinin yaşam döngüsü | VMA görüntüleri ve view'ları; resize için tam kaynak paketi |
| SceneGpuData | Primitive, önceki transform, ışık tamponları ve BrickGrid güncelleme | Snapshot girdisi, tekrar kullanılan CPU çalışma tamponları |
| TemporalState | Kamera/sahne değişimi, reset ve history ping-pong politikası | Açık begin/commit/reset sözleşmesi |
| PickingReadback | İstek, GPU sonucu ve tamamlanmış kare üzerinden tüketim | Frame tamamlanma bilgisi ve selection buffer |
| GBufferPass | Geometri dispatch ve kendi binding sözleşmesi | SceneGpuData ve RenderTargets görünümleri |
| DeferredLightingPass / DebugCompositePass | HDR aydınlatma veya debug çıktısı | GBuffer, ışık ve IBL kaynakları |
| TemporalResolvePass | History resolve ve mevcut tonemap davranışı | TemporalState, HDR ve history kaynakları |
| FrameResources | Komut/fence/upload/readback yaşam süresi | VulkanContext tarafından yönetilir |

Pass'ler kaynaklara sahip olan modüllerden açık, dar kapsamlı görünümler alır. Global servis erişimi, tüm renderer'a friend erişimi veya aynı büyük sınıfı yalnızca birkaç dosyaya bölmek kullanılmaz. Aşama sırası sabit tutulur; görüntü erişim ve layout geçişleri ortak yardımcılarla ifade edilir.

## Uygulama sırası

1. Güncel kaynakları derle; CPU/GPU referans sonuçlarını al. Resize, kamerasız kare, TAA aç/kapat, sahne değişimi ve picking tüketimi için eksik davranış testlerini ekle.
2. Shader veri sözleşmesini ayır; ComputeProgram ve shader yol çözüm tekrarlarını kaldır. Binding, format ve push constant uyumluluğunu koru.
3. RenderTargets sahipliğini ayır. Yeni kaynak paketi tamamen kurulmadan çalışan paketi değiştirme; view'ları görüntülerden önce yok et.
4. SceneGpuData, TemporalState ve PickingReadback bileşenlerini ayır. Temporal commit anını başarılı kare gönderimiyle açıkça ilişkilendir; reset nedenlerini test et.
5. Pass'leri ayır; SDFRenderer'ı ince koordinatöre dönüştür. Kamera ve kalite ayarlarını kare başında bir kez çöz. Eski çağrıları geçiş adaptörleriyle koru.
6. Çalışma tamponlarını yeniden kullan; değişmeyen sahne aktarımını azaltırken önceki transform verisinin bir sonraki karede doğru ilerlediğini doğrula. Grid güncelleme ölçümlerine göre aktarım optimizasyonu yap.
7. FrameResources ve tamamlanma sözleşmesini kur; sonra kare sonu zorunlu CPU beklemesini kaldırmayı değerlendir. Temporal görüntü erişimleri ve sunum tüketimi için senkronizasyonu ayrıca doğrula. Picking sonucu tamamlanan kareyle ve o karenin entity eşlemesiyle tüketilir.
8. Editör, Sandbox, EmptyGameTemplate ve test hedeflerini derle; CPU ve GPU regresyonlarını çalıştır; mimari dokümanlarını güncelle.

## Kabul koşulları

- Mevcut SDF/CSG, PBR/IBL, kamera, picking, debug ve TAA davranışları korunur; kasıtlı düzeltmeler ayrı açıklanır.
- CPU testleri ve GPU görsel/kamera/sözleşme testleri geçer. Validation çıktıları incelenir; GPU bulunmazsa doğrulama eksikliği açıkça raporlanır.
- Resize, sahne değişimi, kamera kesimi ve TAA geçişleri eski görüntüyü veya seçim sonucunu yeni kareye taşımaz.
- Shader binding ve veri offset sözleşmeleri test edilir; yalnız dosya metni eşleşmesini kontrol eden testler yeterli sayılmaz.
- Performans aynı donanım, çözünürlük, sahne, kalite ve ısınma koşullarında karşılaştırılır. CPU/GPU süreleri ayrı raporlanır; ölçüm olmadan hızlanma iddiası yapılmaz.
- Kaynak sahipliği açık ve exception durumunda temizlenebilir olur; pass'ler birbirinin özel alanlarına erişmez.

## Doğrulanan başlangıç durumu

- Release derlemesi başarılı. SDFContractTests.cpp:270 üzerinde mevcut kullanılmayan foundChild değişkeni uyarısı var.
- Güncel derlemede 23/23 CTest kaydı geçti: 20 CPU/editor ve 3 GPU (smoke, visual quality, camera).
- Ayrı `EngineTests.exe --contract` çalıştırması: CPU ve GPU sözleşme paketlerinde 43 assertion başarılı.
- GPU sözleşme testi NVIDIA GeForce RTX 3060 üzerinde Vulkan validation layer etkin olarak çalıştı.
- Testin değiştirdiği imgui.ini başlangıç içeriğine geri getirildi.

Bu belge uygulama tasarımıdır; tamamlanmış refaktör veya doğrulanmış performans sonucu değildir. Kullanıcının mevcut WORK_PLAN.md ve FINDINGS_VERIFICATION_REPORT.md değişiklikleri korunur.

> [!NOTE]
> **G00 Tamamlandı (2026-09-11):** Başlangıç referansı [BASELINE.md](render-refactor/BASELINE.md) ve [EXECUTION_LOG.md](render-refactor/EXECUTION_LOG.md) belgeleriyle donduruldu; görsel ve log referans çıktıları `artifacts/render-refactor/baseline/` altında yedeklendi.
