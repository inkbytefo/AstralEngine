# M15 — Terrain temeli uygulama planı

**Hedef:** `terrain_lab` örneğinde karakter dört tile’ın sınırlarını geçebilsin, kamera LOD değiştirirken çatlak oluşmasın ve aynı senaryo kaynak dosyalar ile `.apak` paketinde geçsin.

**Tasarım:** [ADR-0039](../../decisions/0039-terrain-foundation.md). Her görev test, uygulama ve doğrulama sırasıyla ilerler. Çalışma güncel master tabanlı ayrı bir dalda yapılır. Doğrulanan commit’ler bu dala gönderilir.

| Görev | Uygulama | Kabul testi |
|---|---|---|
| **1. Veri ve cook** | Assets tarafında sürümlü `.aterrain` manifesti ve okuyucu. 16 bit gri tonlu heightmap girişini uç değer kaybı olmadan tile payload’larına dönüştür. Önce asimetrik rampa fixture’ı kullan. | Min ve max değerler, +X/+Z yönleri, negatif tile adresi ve ortak kenar doğrulanır. Bozuk biçim ve tutarsız kenar hata verir. Kaynak ve cook edilmiş veri aynı örnekleri üretir. |
| **2. Tek tile uçtan uca** | App’te kayıtlı `TerrainRef`, asset çözümleme ve runtime tile sahipliği. Renderer’a mesh, fiziğe statik heightfield gönder. `terrain_lab` örneğini ayağa kaldır. | Sahne kaydet/yükle, köşe ve hücre içi raycast, görsel-fizik yüksekliği ve entity silinince mesh/body temizliği. Asimetrik hücre ile iki üçgenin yönü ölçülür. Jolt’un 65 örnekli profil davranışı ve sıkıştırma hata payı belgelenir. |
| **3. Dört tile ve LOD** | 2 × 2 tile fixture’ı. En az iki mesh LOD’u, komşu geçiş indeksleri, ortak kenar normalleri ve hysteresis. Collider sabit çözünürlükte kalır. | Karakter sınırları ve dört tile’ın ortak köşesini yürüyerek geçer. Kamera iki LOD arasında geçerken silüet, normal ve gölge incelenir. LOD zorlamasıyla raycast yüksekliğinin değişmediği doğrulanır. |
| **4. Streaming, origin ve paket** | StreamingSource’tan tile önceliği, yükleme bütçesi, collider ön yükleme ve geçiş koruması. Terrain silinirse veya istek iptal edilirse geç gelen sonuçları bırak. | Hızlı yaklaşma, gecikmiş collider, yükleme hatası, unload/reload ve eksi koordinatta floating origin testi. Kaynak ve `.apak` aynı sabit-adım girdileriyle aynı kabul sonucunu verir. Kapanışta canlı tile kaynağı kalmaz. |
| **5. Ölçüm ve devir** | Mesh üretimi ve upload süresi, frame-time dağılımı, RAM/VRAM, aktif collider ve tile sayısı, RT açıksa BLAS maliyeti. Küçük fixture ile başlayıp M15 hedefindeki 2 × 2 km sahnede koş. | Debug testleri, ilgili golden kareleri ve ölçüm koşulları raporlanır. Yazılımsal Vulkan ve gerçek GPU sonuçları ayrı etiketlenir. Eksik donanım ölçümleri açıkça belirtilir. |

İlk merge incelemesinde tek tile geometri/fizik hizası, dört tile geçişi ve paketli çalıştırma için loglar ve sahne görüntüleri sunulur. Materyal katmanları, boyama ve delik maskesi için yeni görev paketi, bu temel doğrulandıktan sonra açılır.

## Uygulama ve doğrulama

- Görev 1–3. 16 bit örnekler paketlemede korunur. 65 × 65 tile mesh’i ve collider hizası raycast ile doğrulanır. Dört tile ve ortak köşe karakterle geçilir. 16 kenar maskesi için dikiş topolojisi, ortak UV ve normaller ile kamera LOD değişiminde collider eşliği test edilir.
- Görev 4. Pozitif `TerrainRef` yarıçapları mesh ve collision için ayrı residency etkinleştirir. Tile verisi ve ilk mesh worker’da hazırlanır. Yayın ana thread’de fizik adımından önce yapılır. Bekleyen işin sahibi veya isteği kaybolursa sonucu uygulanmaz. Eksik dosya uyarısı ve bölgeye tekrar girişte yeniden deneme test edilir. Kaynak ve `.apak` aynı karakter sonucunu verir. Collider, negatif floating origin kaymasından sonra aynı handle ve raycast yüksekliğini korur.
- Görev 5. Software golden referansları karma ve uzak LOD için `tests/golden/references/terrain_*.png` altında. `golden-hw` uygun cihaz olduğunda aynı sahneleri çalıştırır.

| 2.048 × 2.048 m stres senaryosu | Sonuç |
|---|---:|
| Manifestteki tile | 1.024 |
| En yüksek eşzamanlı residency | 6 collider ve 6 mesh |
| Headless medyan kare | 28,6 ms |
| llvmpipe, 160 × 90 raster medyan kare | 67,3 ms |
| Son karede LOD mesh üretimi / toplam mesh upload | 9,1 ms / 2,5 ms |
| Test süreci tepe RSS | 128 MiB |

Ölçüm Debug derlemede altı odak adımı ile yapıldı. Her karede odak 320 m sıçratıldı, yükleme zorla tamamlandı ve 1.024 tile merkezinde raycast çalıştı. Bu değerler residency stres senaryosuna aittir. Donanım VRAM ve RT BLAS maliyetleri bu raster koşusunda ölçülmedi. LOD dikiş batch’i ana thread’de oluşturulup yükleniyor. Karakter güvenlik kontrolü `CharacterController` yolunda uygulanıyor. Serbest rijit cisimler için ayrı bir geçiş politikası gerekiyor.
