# AstralEngine — Görev planı ve devam kaydı

Son güncelleme: 2026-09-06 (Europe/Istanbul)

## Amaç ve çalışma kuralları

Kullanıcının son ana isteği: Mimari incelemede doğrulanan eksikleri tamamlamak ve yanlış uygulamaları düzeltmek. Bu dosya sohbetler arası devam kaydıdır. Her anlamlı değişiklik, test sonucu veya engelden sonra güncellenmelidir. Test edilmeyen işler tamamlandı olarak işaretlenmemelidir.

- Mevcut editör tasarımını ve işlevlerini koru; çalışma ağacındaki kullanıcı değişikliklerini geri alma.
- C++20 ve mevcut proje stilini izle. Önce hatayı yeniden üreten test, sonra düzeltme ve ilgili doğrulama.
- Eski veri yolunu, kullanıcıları ve testleri göç etmeden silme. Geniş renderer parçalama çalışmasından önce doğruluk sorunlarını çöz.
- Kullanıcı açıkça istemedikçe commit/push yapma. Kullanıcıya Türkçe, kısa ve somut ilerleme bilgisi ver.
- Bu dosyayı yeni sohbette okuduktan sonra kaynakları ve git diff'i doğrula; aşağıdaki kayıtlar mevcut dosyaların yerine geçmez.

## Önceki çalışmaların durumu

- [x] Modern koyu tema, docking düzeni ve Content Browser/Console/Statistics alt sekmeleri uygulandı.
- [x] Viewport üst araç çubuğu, Play/Pause/Stop, dönüşüm araçları ve dinamik araç ayarları uygulandı.
- [x] Çoklu seçim: ortak SelectionContext, Hierarchy Ctrl+tıklama, toplu silme/çoğaltma ve undo/redo, çoklu Inspector bildirimi, ortak gizmo dünya dönüşüm farkı uygulandı.
- [x] Önceki çoklu seçim çalışmasında Debug/Release EditorSelectionTests ve 5 kare editör smoke testi geçti. Testler gerçek ImGui etkileşimini ve ImGuizmo sürüklemesini de kapsadı.
- [ ] Runtime görsel izolasyonu kullanıcı tarafından çalışmıyor olarak bildirildi; kullanıcı bu konuyu açıkça erteledi. Önceki isPlaying aktarımı uygulanmış olması kullanıcıdaki sorunun çözüldüğü anlamına gelmez.

## Aktif iş: SDF veri yolu ve renderer doğruluğu

### Öncelikli hata: görüntü titremesi (kullanıcının son bildirimi)

- [x] TAA/GBuffer/forward ışın üretimi incelendi: forward renk yarım piksel geriden örnekleniyor; GBuffer hareket vektörüne jitter farkı giriyor.
- [x] Sabit kamerada gerçek GPU motion buffer'ını okuyan regresyon testi eklendi (VisualQualityGpuTests). Temiz baseline, Vulkan doğrulama hatası olmadan 0.416519 piksel sahte hareket yakaladı (39.04 sn, beklenen başarısızlık).
- [x] Jitter hareketten ayrıldı; forward renk/GBuffer derinlik örnek merkezleri eşleştirildi. Halton ofseti yarım piksel aralığına indirildi. Deferred ışıklandırma TAA kapalıyken de GBuffer ile aynı ofseti kullanıyor.
- [x] Düzeltme sonrası GPU testi geçti (49.41 sn): forward/deferred sabit kamerada altı jitter fazında bütün motion değerleri 0.01 piksel toleransında; mevcut grid, hareket, resize ve camera-cut senaryoları da geçti. Vulkan doğrulama hatası yok.
- [x] Debug ve Release AstralEditor derlendi. `git diff --check` geçti.
- [x] Beş kare Debug editör smoke testi 1280x720 Sandbox ile geçti (exit 0, Vulkan doğrulama hatası yok). Bu sahne boş olduğundan görsel titreme kanıtı GPU fixture testidir. Kullanıcının kendi sahnesinde görsel tekrar kontrolü henüz yapılmadı.

Jitter düzeltmesi dosyaları: `shaders/SDFGBuffer.glsl`, `shaders/SDFCompute.glsl`, `src/Core/Application.cpp`, `src/Renderer/SDFRenderer.cpp`, `Tests/EngineTests/src/VisualQualityGpuTests.cpp`. Genel mimari planı halen açık. Eski "transforms bildirimi eksik" kaydı tarihsel: güncel dosyada bildirim var ve iki derleme de başarılı; bu eksikliği yeniden düzeltmeye çalışma.

Teknik referans: AMD GPUOpen temporal reconstruction dokümanı da hareket vektörlerinden jitter'ın çıkarılmasını tanımlar: https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-temporal/ . Burada FSR entegrasyonu yapılmadı; mevcut TAA'nın hareket sözleşmesi düzeltildi.

### 1. Tek ve güvenilir sahne verisi

- [x] Aktif yol incelendi: Application -> RenderExtractionSubsystem -> eski SDFEditGPU -> renderer içi SDFPrimitiveRecord dönüşümü.
- [x] Yeni snapshot yolunda LegacyPackedScale şekillerinin ebeveyn dönüşümünü kaybedebildiği doğrulandı.
- [ ] Snapshot dönüşümünü düzelt: ebeveynler, explicit shape ölçüleri, negatif/nonuniform ölçek, geçersiz matrisler ve gerekiyorsa shear için güvenli mesafe ölçeği.
- [ ] Snapshot kayıtlarıyla aynı sırada EntityHandle eşlemesini tamamla; picking sırasını koru.
- [ ] Aktif render extraction yolunu snapshot'a geçir. Render interpolasyonunu ve sahne instance kimliğini koru.
- [ ] Eski overload'ları güvenli uyumluluk yolu olarak tut; kapasitesiz ham upload yardımcısının kullanıcılarını doğrula ve güvenli hale getir/kaldır.
- [ ] CPU ve GPU testleri: şekiller, ebeveyn dönüşümü, kayıt sırası, picking eşleşmesi ve kalıcı yüzey kimliği.

### 2. Hareket geçmişi ve temporal doğruluk

- [x] Renderer eski dönüşümde surfaceId=i+1 üretiyor; ekleme/silme/sıralama kimliği değiştiriyor.
- [x] GBuffer prevHitPos=hitPos kullanıyor; nesne hareket geçmişi aktif yeni kayıtta kayboluyor.
- [x] Material rgba8 alpha kanalındaki 16-bit normalize yüzey kimliği hassasiyet kaybediyor; GPU TAA bu kimliği tüketmiyor.
- [ ] Önceki nesne dönüşümünü kararlı kimlikle eşleştir; ilk kare, silme/ekleme ve sahne geçişini ele al.
- [ ] Gerçek nesne hareket vektörlerini GPU yoluna bağla.
- [ ] GPU temporal geçmiş reddini güvenilir kimlik/normal/depth verileriyle bağla; format, descriptor ve barrier değişikliklerini birlikte doğrula.
- [ ] SDFChangeSet'in aktif Application yoluna bağlı olup olmadığını doğrula ve eksik entegrasyonu tamamla.
- [ ] Hareketli nesne, yeniden sıralama, disocclusion ve sahne geçişi testleri.

### 3. BrickGrid doğruluğu ve artımlı güncelleme

- [x] Record overload'ının her kare FullRebuild yaptığı doğrulandı.
- [x] Record sınır hesabı affine ölçeği yok sayıyor; hücre başına ters matris hesaplıyor. Plane yolu conservativeScale'i uygulamıyor.
- [ ] Güvenli dünya sınırlarını kayıt başına önceden hesapla; büyük/ölçekli/rotasyonlu şekilleri atlama.
- [ ] Eski ve yeni etki alanlarına göre kirli hücreleri güncelle; değişmeyen sahnede gereksiz rebuild/upload yapma.
- [ ] Incremental sonucu full rebuild ile karşılaştır; grid açık/kapalı GPU görüntü eşitliğini doğrula.

### 4. Kalite ayarları

- [x] QualitySettings mevcut; deferred shadow/AO push değerleri sabit.
- [ ] Kalite ayarlarını gerçek renderer parametrelerine bağla; gölge/AO/TAA açma kapama ve debug davranışlarını tutarlı kıl.
- [ ] Ayarların gerçek GPU çıktısını etkilediğini ve geçersiz değerlerin güvenli işlendiğini doğrula.

### 5. Son doğrulama

- [ ] İlgili CPU testleri.
- [ ] SDFContractGpuTests ve VisualQualityGpuTests dahil ilgili GPU testleri.
- [ ] Debug ve Release derlemesi; editör smoke testi ve çoklu seçim regresyonları.
- [ ] Son diff kontrolü; tamamlananlar, kalanlar ve gerçek test sonuçlarını bu dosyaya ve kullanıcıya bildir.

## Tam kaldığımız yer / sonraki somut adım

2026-09-06: Kullanıcının devam kaydı isteği üzerine bu dosya oluşturuldu. Aktif renderer düzeltmeleri henüz doğrulanmış değil.

Çalışma ağacında `SDFSceneSnapshot.hpp/.cpp` içinde kayıt/entity eşlemesi için başlamış değişiklik var: GetEntities(), m_Entities ve RecordEntityPair eklenmiş. **Dikkat: cpp içinde `transforms` bildirimi kaldırılmış fakat kullanımları duruyor; mevcut kısmi değişiklik derlenebilir değil.** Önce bu parçayı tamamla ve kaynağını mevcut diff üzerinden doğrula. Bu durum 2026-09-06 git diff incelemesinde görüldü; henüz test çalıştırılmadı.

Sonraki okumalar:

1. `src/Core/Systems/RenderExtractionSubsystem.cpp` ve `include/Astral/Core/Systems/RenderExtractionSubsystem.hpp`: FrameContext, interpolasyon ve aktif extraction akışı.
2. `src/Geometry/SDFSceneSnapshot.cpp`: dönüşüm hatası için hedefli regresyon testini yaz ve düzelt.
3. `src/Core/Application.cpp`: yaklaşık 270. satırdaki upload ve picking eşlemesi.
4. `src/Renderer/BrickGrid.cpp`: mevcut legacy incremental uygulamayı incele; yeni yol için güvenli sınırlar tasarla.
5. `src/Renderer/SDFRenderer.cpp/.hpp`, `shaders/SDFGBuffer.glsl`, `shaders/SDFTAA.glsl` (dosya adını rg ile doğrula): geçmiş verisinin GPU taşıma biçimini belirle.

## Teknik referanslar

- SDFPrimitiveRecord mevcut CPU/GLSL sözleşmesi: alignas(16), 128 byte. Layout değişirse bütün shader, buffer kapasitesi ve sözleşme testlerini birlikte güncelle.
- Snapshot Extract şu an recursive world transform kullanıyor; eski extraction cached WorldTransformComponent üzerinden render interpolasyonunu tüketiyor. Göç sırasında bunu kaybetme.
- Eski extraction WorldTransformComponent.renderedPosition/renderedRotation/renderedScale geçmişini güncelliyor.
- Snapshot kimliği entity index + generation + sceneInstanceId + subPrimitiveIndex hash'i. CSG sırası uint64 kaynaktan uint32 kayda daralıyor; sıralama semantiğini doğrula.
- BrickGrid boyutu 32x16x32; mesafeler [0,1] doygun. Smooth CSG etki alanı kirli sınır hesabına dahil edilmeli.
- VisualQualityGpuTests zaten gerçek görüntü karşılaştırması yapıyor; 'görsel test yok' iddiası doğru değil. Mevcut testleri genişlet.
- Renderer monolitik fakat doğruluk işi bitmeden geniş sorumluluk parçalama yapma.
- Mimari inceleme dosyası: `ARCHITECTURE_ANALYSIS_REPORT.md` (çalışma ağacında untracked; içeriğini ayrıca doğrula).

## Ortam ve doğrulama kaydı

- Kök: `C:/Dev/InkbytefoProjects/deneme/AstralEngine`
- Shell: PowerShell; MinGW araçları: `C:/mingw64/bin`; build klasörleri: `build-debug`, `build-release`.
- GPU: RTX 3060, Vulkan 1.4; önceki GPU/smoke çalışmalarında kullanılabildi.
- Genel derleme: `cmake --build build-debug --target <target> -j4`
- Test: `ctest --test-dir build-debug -R <test> --output-on-failure` (hedef/test adlarını CMake'den doğrula).
- Mevcut renderer düzeltmesi için yeni test sonucu: **henüz yok**. Önceki editör test başarılarını bu işe kanıt olarak kullanma.
- Git çalışma ağacı çok sayıda editör/asset/reflection değişikliği içeriyor. Toplu reset/checkout/clean yapma; kullanıcı uygulamasını kapatma.

## İlerleme günlüğü

| Tarih | İş | Sonuç / takip |
|---|---|---|
| 2026-09-06 | Mimari iddiaları kaynak kodla karşılaştırma | Aktif eski veri yolu, kimlik/geçmiş kaybı, grid ve kalite bağlantısı sorunları doğrulandı. |
| 2026-09-06 | Snapshot yolu inceleme | LegacyPackedScale ebeveyn dönüşümü sorunu bulundu; renderer göçünden önce düzeltilecek. |
| 2026-09-06 | Kalıcı görev planı | Bu dosya yazıldı; kısmi snapshot düzenlemesindeki eksik transforms bildirimi kaydedildi. |
| 2026-09-06 | Kritik görüntü titremesi | GPU regresyon testi önce 0.416519 piksel sahte hareketle başarısız, düzeltmeden sonra başarılı. Debug/Release derlemeleri geçti; kullanıcı sahnesindeki görsel sonuç henüz teyit edilmedi. |
