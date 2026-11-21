# ADR-0018 — Arazi: heightfield + SDF hacimleri, yıkım

- **Durum:** Kabul (uygulama M15 ve M19)
- **Tarih:** 2026-11-21
- **Bağlam:** yol haritası v3, ADR-0011 (RT aydınlatma), ADR-0010 (fizik), mevcut analitik SDF render'ı

## Sorun

Açık dünya oyunları için bir arazi sistemi gerekiyor. Godot'nun Terrain3D eklentisi iyi bir örnek:
clipmap LOD'lu heightfield, bölgelere ayrılmış veri, 32 dokuya kadar boyama, motor içinde
heykeltıraşlık (sculpt), delikler, fizik ve bitki yerleştirme. Heightfield'ın yapamadığı şeyler de
istiyoruz: mağara, sarkan kaya, kemer ve yıkım (krater açmak, duvar delmek, kazmak). Yıkım arazide
olduğu kadar nesnelerde de olmalı.

## Karar: iki katmanlı arazi + aynı SDF altyapısıyla yıkım

### Katman 1 — Heightfield arazi (M15, Terrain3D benzeri)

Kilometre ölçeğinin %95'i: ucuz, akıcı, fizikle aynı veri.

- **Veri:** sabit boyutlu bölgeler (region). Her bölgede:
  - yükseklik haritası (R32F);
  - kontrol haritası (texel başına 32 bit: temel doku, üst doku, karışım, UV döndürme/ölçek, delik bayrağı);
  - renk/tonlama haritası.
  Bölgeler ayrı dosyalarda tutulur (`.aterrain`); cook'lanır, ileride stream edilir.
- **Render (raster):** GPU geometry clipmap. Kamerayı izleyen eş merkezli halkalar; vertex shader
  yüksekliği texture'dan okur. Mesafeye göre LOD, CPU'da mesh üretimi yok.
- **Render (RT):** clipmap görüşe bağlı olduğu için ışın izleme için ayrıca bölge başına kaba bir mesh
  (BLAS) tutulur. RT gölge, DDGI ve yansımalar bunu kullanır; düzenleme yapılınca güncellenir.
- **Materyal:** M10'un materyal sistemi. 32 katmanlık texture dizisi (albedo, normal, roughness), yüksekliğe
  dayalı karışım, eğime göre otomatik doku (kayalık yamaçlar), tekrar kırma (detiling), makro varyasyon,
  ıslaklık (M17 hava durumu).
- **Fizik:** bölge başına Jolt heightfield şekli (M12); delikler fizikte de delik.
- **Düzenleme:** motor içinde fırça araçları (yükselt, alçalt, düzleştir, yumuşat, rampa, boya, delik,
  renk), M8'in debug arayüzü üstünde. Fırçalar GPU'da çalışır, geri alma desteklenir.
- **Yerleştirme:** bitki/kaya örnekleyici (instancer), boya katmanlarına bağlı kurallarla. Rüzgârla
  sallanma M12 alanlarından gelir. Büyük ölçek M22'de.

### Katman 2 — SDF hacimleri (M19)

Heightfield'ın yapamadığı yerler için yerel, seyrek hacimler.

- **Veri:** seyrek tuğla (brick) SDF: örneğin 8³ vokselik tuğlalar, yalnızca yüzeye yakın bölgelerde.
  Boş ve dolu uzay saklanmaz. Hacimler dünyada yerleştirilir: mağara girişi, uçurum çıkıntısı, kaya kemer.
- **Arazi ile birleşim:** SDF hacmi heightfield'a delik açar ve o bölgede yüzeyi kendisi tanımlar. Geçiş
  kenarları hacmin içinde yumuşatılır.
- **Render:** SDF yüzeyi parça (chunk) başına GPU'da mesh'e çevrilir (surface nets / dual contouring).
  Sonrası normal mesh yolu: raster, RT BLAS, materyal, gölge. Ayrı bir SDF render yolu açılmaz; bugünkü
  analitik SDF ışın yürütme küçük şekiller için kalır.
- **Materyal:** voksel başına materyal kimliği, triplanar izdüşüm (UV yok), arazinin doku katmanlarıyla
  aynı set.
- **Fizik:** parça mesh'leri M12'un mesh çarpışma şekilleri olur; düzenlenince yeniden oluşturulur.

### Yıkım (M19 + M21)

Aynı SDF altyapısı, iki ölçekte:

- **Arazi ve büyük yapılar:** çalışma anında CSG işlemleri (küre/kapsül/kutu çıkar ya da ekle): patlama
  kraterleri, kazma, tünel açma. Yalnızca etkilenen tuğlalar ve parçalar yeniden mesh'lenir, BLAS ve
  çarpışma şekli güncellenir. Heightfield'a yapılan değişiklik orada bir SDF hacmine dönüşür.
- **Yıkılabilir nesneler:** cook adımı mesh'i SDF'ye çevirir (bake). Çalışma anında çıkarma işlemleri
  uygulanır; kopan parçalar bağlantı analiziyle (connected components) bulunur ve kendi convex
  şekilleriyle dinamik body olur (M21). Mermi deliği, ikiye ayrılan sütun, parça parça yıkılan duvar.
- **Önceden kırılmış mesh'ler** (Voronoi) SDF gerektirmeyen ucuz nesneler için ayrıca desteklenir (M21).
  Cam ve tahta gibi şeyler bu yolla daha iyi görünür.

## Neden bu ikili yapı

- **Yalnız SDF/voksel arazi:** mağara ve yıkım bedava gelir, ama kilometre ölçeğinde bellek, mesh üretimi ve
  fizik maliyeti çok yüksek; LOD ve streaming de karmaşıklaşır.
- **Yalnız heightfield:** ucuz ve olgun, ama mağara, sarkan kaya ve yıkım imkânsız.
- **İkisi birlikte:** geniş alan heightfield'da ucuz kalır; maliyet yalnızca ilginç noktalarda ödenir.
  Yıkım için yazılan SDF altyapısı arazi ve nesneler için tek bir koddur.

## Sonuçlar

- M12'a mesh çarpışma şekillerinin hızlı yeniden oluşturulması (düzenleme sırasında) gereksinimi eklenir.
- M10'un materyal sistemi texture dizileri, triplanar izdüşüm ve katman karışımını desteklemeli.
- M8'in debug arayüzü ilk düzenleme araçlarını taşıyacak kadar genişlemeli (fırça, katman listesi, geri al).
- Su (M16) kıyı ve nehirleri arazinin yükseklik verisinden okur; bu yüzden arazi sudan önce geliyor.
- Ayrı bir editör uygulaması yerine düzenleme motorun içinde ve oyun çalışırken yapılır.
