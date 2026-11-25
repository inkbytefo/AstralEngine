# Araştırma: RAGE (RDR2 · GTA 6) — teknolojiler ve fikirler

**Tarih:** 2026-11-21 · Amaç: AstralEngine'e hangi sistemlerin, hangi sırayla taşınacağını belirlemek.
Kaynaklar sonda. Kesin bilinenler (Rockstar sunumları, frame analizleri, Digital Foundry'nin çıkmış oyun
incelemesi) ile tahminler ayrı tutulmuştur.

## 1. RDR2 (2018) — temel

### Renderer (RenderDoc frame analizi)
- Klasik **deferred**: 7 hedefli G-buffer — albedo, normal (+kumaş/saç bilgisi), F0/smoothness/metallic
  (+gölge maskesi), cavity/saç, ekran uzayı hız (motion vector), reversed-Z derinlik + stencil, baked AO.
- İş **birden çok kareye yayılır** (time-slicing): tek bir kare yakalaması pipeline'ın sadece bir kısmını gösterir.
- **Çamur haritası:** ayak/tekerlek izleri 2048² displacement haritasında birikir, arazi parallax occlusion ile çizilir.
- **Gölgeler:** güneş/ay için 4 kaskadlı CSM; spot/nokta ışıklar 3 kalite kademeli atlaslarda; statik ışıklar
  baked cubemap gölge kullanır, dinamik gölge yalnızca oyuncu yakındayken çizilir.
- **AO:** büyük ölçek baked (cubemap/iç mekân), küçük ölçek SSAO.
- **Işıklar:** nokta = küre hacmi, spot = oktahedron; depth-bounds + stencil ile ayıklanır. Yarı saydamlar forward.
- **Yansıma:** aynalar planar, su SSR + ortam haritası. Ortam cubemap'i tile-based deferred compute ile ışıklanır.
- **Compute ağırlıklı:** gök, bulut, volumetrik, bloom compute'ta; bloom graphics/compute kuyrukları arasında
  dönüşümlü (async compute).
- **Bloom:** CoD: Advanced Warfare yöntemi — eşiksiz giriş, 7 mip, 13-tap aşağı örnekleme, 3×3 tent yukarı örnekleme.

### Atmosfer (SIGGRAPH 2019, F. Bauer — "Creating the Atmospheric World of RDR2")
- Tüm dünyayı kaplayan, hava durumu/saatle gerçek zamanlı güncellenen **2 kanallı bulut haritası** (iki katman
  yoğunluğu); yükseklik gradyanları kümülüs/stratus şeklini verir.
- Ayrı **sis haritası** (taban yükseklik, kapsama mesafesi, yoğunluk); yağmur sisi bulut yoğunluğundan doğar.
- Yakın bölge frustum'a hizalı hacimde, uzak bölge yalnızca ray march ile.
- Uzak gölgeler arazi yükseklik haritasında güneşe doğru ray ile; mesafe arttıkça yumuşar.
- Tek saçılma modeli her şeyi (sis, bulut, gökkuşağı) kapsar; çoklu saçılma "oktav" yaklaşımıyla taklit edilir.
  Gökkuşağı bile sanatçı yerleştirmez, sistem üretir.

### Animasyon
- **Euphoria** (NaturalMotion) RAGE kaynak koduna gömülü: fizik + AI + elle animasyon → kendini korumaya çalışan
  aktif ragdoll. 2017'den beri lisanslanmıyor → kendi sistemimizi yazmamız gerekir.
- **At sistemi (GDC 2021):** tek parça klip yok; küçük animasyon havuzu çalışma anında birleştirilir, havuz
  çevre/hız/yük/ruh haline göre daraltılır; sürekli hızlanma/yavaşlama döngüleri; uçurumlarda "stop matching".

### Yapay zekâ / dünya
- Günlük rutinler (programlar), tanıma/hafıza, ~200 hayvan türü ve besin zinciri.

## 2. GTA 6 (19 Kasım 2026) — üstüne ne eklendi (Digital Foundry incelemesi)

- **Ray tracing:** her iki konsolda RT global illumination + RT yansıma; RT gölgeler yalnız PS5 Pro.
  1440p iç çözünürlük, 30 FPS; Pro'da PSSR ile 4K.
- **Saç:** NPC'ler dahil herkes fiziğe dayalı strand saç.
- **Atmosfer:** fiziksel gökyüzü, RDR2'den türeyen volumetrik bulutlar; hava durumu mahalle mahalle ilerler.
- **LOD:** geçişler neredeyse görünmez; araçlar/yayalar/çalışan sokak lambaları ufka kadar.
- **Simülasyon:** polis karakollardan çıkar ve şüpheliyi görmeden takibe başlamaz; ezilen gövde sürüşü etkiler;
  arazi deforme olur.
- **Darboğaz CPU:** dünya simülasyonu CPU'ya bağlı; 40 FPS bile trafik/kalabalık/fizik kesintisi gerektirirdi.
- **Patent (2020):** NPC ve araçlar dünyadaki node'lar üzerinde hareket eder; her NPC node'ları geçerken
  kendi özelliklerini tanımlayabilir.

## 3. AstralEngine'e çıkarımlar → yol haritası

| # | Fikir | Nereye |
|---|---|---|
| 1 | **Simülasyon önce:** dünya yoğunluğu FPS'ten önemli; ECS + JobSystem en baştan paralel | M2 ✅, sürekli |
| 2 | **Dünya ölçekli veri haritaları:** bulut, sis, çamur, yağmur, hava durumu texture'larda, compute ile güncellenir | Atmosfer fazı |
| 3 | **Hesabı karelere yaymak:** time-slicing + TAA/reprojection + temporal birikim | M3.5 (bu faz) |
| 4 | **Baked + dinamik + RT ışık karışımı:** RT GI/yansıma/gölge, RT yoksa raster yedek yol | M3.5 (bu faz) |
| 5 | **Kendi aktif ragdoll'umuz** (Jolt üzerinde, Euphoria benzeri) | M6 kılıç arenası |
| 6 | Parça animasyon havuzu + çalışma anında birleştirme (motion matching ailesi) | Animasyon fazı |
| 7 | Node tabanlı NPC/araç navigasyonu, rutinler, hafıza | AI fazı |
| 8 | Görünmez LOD + dünya streaming | M5 asset'ler sonrası |

## Kaynaklar

- F. Bauer, *Creating the Atmospheric World of Red Dead Redemption 2*, SIGGRAPH 2019 Advances —
  https://advances.realtimerendering.com/s2019/
- imgeself, *Graphics Study: Red Dead Redemption 2* — https://imgeself.github.io/posts/2020-06-19-graphics-study-rdr2
- A. Courrèges, *GTA V Graphics Study* — https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/
- PC Gamer, RDR2 at animasyonu (GDC 2021 özeti)
- Digital Foundry, *Grand Theft Auto 6 Tech Review* (18 Kasım 2026) ve özetleri (TweakTown, VGTimes)
- Take-Two patenti (NPC navigasyonu), VGC/GameSpot haberleri
- Hillaire 2020, *A Scalable and Production Ready Sky and Atmosphere Rendering Technique*
