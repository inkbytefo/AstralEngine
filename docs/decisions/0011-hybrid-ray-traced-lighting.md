# ADR-0011: Donanımsal ray query ile hibrit fotogerçekçi aydınlatma

**Durum:** Kabul · 2026-11-21 · Araştırma: [research/rage.md](../research/rage.md)

**Bağlam:** Hedef RAGE seviyesi (GTA 6) gerçek zamanlı fotogerçekçilik. GTA 6 rasterize G-buffer üzerine
RT global illumination, RT yansıma ve (üst donanımda) RT gölge koyuyor. M3 renderer'ı tek gölge haritası,
sabit gök ambient'i ve gökyüzü rengi yansımasıyla sınırlı: renk taşması (color bleeding), çoklu sekme,
gerçek yansıma ve alan ışığı yumuşak gölgesi yok.

**Karar:**
1. **RHI:** `VK_KHR_acceleration_structure` + `VK_KHR_ray_query` desteklenirse açılır. BLAS mesh başına bir kez,
   TLAS her kare komut listesinde kurulur. Shader'lar TLAS'a **adresle** ulaşır
   (`accelerationStructureEXT(uint64_t)`), descriptor yok — ADR-0008'in BDA çizgisiyle aynı. Ray tracing
   pipeline'ı (raygen/hit shader'ları) değil **compute içinde ray query**: tek pipeline türü, render graph'a uyar.
2. **Birincil görünürlük raster kalır** (G-buffer). Işınlar yalnızca ikincil etkiler içindir.
3. **Doğrudan ışık:** güneş (açısal yarıçaplı koni), nokta ışıklar (yarıçaplı küre) ve **dikdörtgen alan ışıkları**
   piksel başına stokastik örneklenir, görünürlük RT gölge ışınıyla. Gürültülü sinyal denoise edilir.
4. **Dolaylı diffuse:** **DDGI** (Majercik ve ark. 2019) — ışın atan prob ızgarası, oktahedral irradiance +
   mesafe momentleri, Chebyshev görünürlüğü. Prob ışınları vurdukları yüzeyi önceki karenin problarıyla
   ışıkladığı için **sonsuz sekme** zamanla birikir; gürültüsüz ve kararlı.
5. **Speküler:** piksel başına GGX örneklemeli yansıma ışını (pürüzsüzde ayna, pürüzlüde dağınık); vuruş noktası
   doğrudan ışık + DDGI + emisyonla ışıklanır, ıska gökyüzü. Alan ışıkları yansımada analitik dikdörtgen olarak görünür.
6. **Denoise (SVGF ailesi):** diffuse sinyali albedodan ayrılır (demodülasyon); diffuse ve speküler ayrı ayrı
   reprojeksiyonla zamansal biriktirilir (geçmiş uzunluğu, normal/derinlik reddi), sonra kenar duyarlı à-trous
   uzamsal filtre. Ardından **TAA** (Halton jitter, YCoCg varyans kırpma), **bloom** (CoD:AW / RDR2 yöntemi),
   **ACES** tonemap.
7. **SDF'ler** TLAS'ta değil: RT pass'leri üçgen vuruşundan sonra SDF sahnesini aynı ışın üzerinde vuruş
   mesafesine kadar raymarch eder; böylece SDF'ler de gölge atar, yansır ve GI'a katılır.
8. **Yedek yol:** RT yoksa veya `RenderSettings::ray_tracing = false` ise M3 raster yolu aynen çalışır.

**Neden ReSTIR/yol izleme değil:** ReSTIR DI/GI daha az gürültü verir ama rezervuar yönetimi bu fazın kapsamını
aşar; DDGI + stokastik doğrudan ışık + SVGF aynı görsel sınıfa daha az riskle ulaşır. ReSTIR ileride doğrudan ışık
örneklemesinin yerine aynı arayüzle takılabilir.

**Sonuçlar:** Yeni efektler yeni pass'lerdir (OCP). Kalite/performans `RenderSettings` ile ayarlanır
(prob yoğunluğu, ışın sayısı, denoise adımları). Hareketli nesnelerin hız vektörü bu fazda yalnız kameradan
türetilir (bilinen sınır).
