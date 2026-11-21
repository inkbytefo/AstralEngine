# ADR-0021 — Dünya ve harita sistemi: World, hücreler, streaming, katmanlar (World Partition benzeri)

- **Durum:** Kabul (uygulama M9; HLOD ve GPU-driven kısmı M22)
- **Tarih:** 2026-11-21
- **Bağlam:** ADR-0015 (sahneler ve prefab'lar), ADR-0016 (paketleme), ADR-0018 (arazi), Unreal Engine 5
  World Partition / One File Per Actor / Data Layers / Level Instance

## Sorun: "bir harita nedir?" sorusunun net cevabı yok

Bugün bir harita = tek bir `.scene` dosyası. İçinde kamera, ışıklar, oyun kuralları, her nesne. Küçük
oyunda (Pong) bu yeterli, ama:

- **Tek dosya, tek kişi:** iki kişi (ya da iki yapay zekâ ajanı) aynı haritayı aynı anda düzenleyemez;
  her değişiklik dev bir dosyada birleşme çatışması.
- **Hepsi ya da hiçbiri:** sahne tümüyle yüklenir. Açık dünyada kilometrelerce içerik belleğe sığmaz,
  yükleme ekranı uzar.
- **Sınır yok:** "hangi ayar haritaya, hangisi oyuna ait, gece/gündüz versiyonu nerede, bir köyü başka
  haritada nasıl tekrar kullanırım" soruların cevabı yok.
- **Büyük koordinatlar:** `float` konum merkezden ~5–10 km sonra titrer.

## Karar: Unreal 5'in fikirleri, bizim veri modelimizle

### 1. Kavramlar (bir oyun tasarımcısının göreceği hâli)

| Kavram | Dosya | Ne |
|---|---|---|
| **Dünya (World)** = harita | `maps/island.world` | Haritanın kimliği: dünya ayarları (oyun kuralları, gökyüzü, render ayarları, yerçekimi), streaming ızgarası, katman listesi |
| **Entity dosyası** | `maps/island.entities/<id>.ent` | Haritaya **yerleştirilmiş tek bir şey** (bir ağaç, bir ev prefab örneği, bir ışık) ve çocukları. Bir dosya = bir nesne (UE'nin One File Per Actor'ı) |
| **Hücre (cell)** | cook çıktısı | Izgaranın bir karesi. Cook, her entity'yi sınır kutusuna göre hücrelere dağıtır; oyun hücreleri yükler |
| **Veri katmanı (data layer)** | `.world` içinde | Entity'lerin etiketi: `gece`, `köprü_yıkıldı`, `görev_3`. Çalışırken açılır/kapanır |
| **Bölüm örneği (level instance)** | `.scene` | Hazır bir alan (köy, zindan, karakol) bir bütün olarak dünyaya yerleştirilir; prefab'ın harita ölçeği |
| **Prefab** | `.prefab` | Değişmedi (ADR-0015) |

Küçük oyunlar için hiçbir şey değişmez: tek `.scene` dosyası hâlâ geçerli bir haritadır (streaming'siz).

### 2. Tek dosya = tek nesne

Editörde/kaynakta her üst seviye entity kendi küçük dosyasında, bugünkü `astral.scene` kayıt biçimiyle.
- İki kişi aynı haritanın farklı nesnelerine dokunursa çatışma yok.
- Yapay zekâ ajanı "bu evi taşı" derken yalnızca o evin 20 satırlık dosyasını okur ve yazar.
- `.world` dosyası küçük kalır (ayarlar + ızgara + katmanlar); entity listesi klasörden gelir.
- Cook bu dosyaları hücre paketlerine birleştirir; oyunda dosya sayısı sorun olmaz.

### 3. Hücreler ve streaming

- **Izgaralar:** `.world` bir ya da birkaç çalışma ızgarası tanımlar (ör. `main`: 128 m hücre, 384 m
  yükleme mesafesi; `landmarks`: 512 m hücre, 2 km). Entity'ye ızgara atanabilir; varsayılan `main`.
  `always_loaded` işaretli entity'ler (oyun kuralları, gökyüzü, oyuncu) hücreye girmez.
- **Streaming kaynakları:** `StreamingSource` component'i (varsayılan: aktif kamera/oyuncu). Birden fazla
  olabilir (ör. araç ileriye bakarken önünü yükler, sinematik kamera).
- **Asenkron yükleme, bütçeli birleştirme:** hücre JobSystem'de okunur ve ayrı bir hazırlık dünyasına
  çözülür; ana dünyaya kare başına zaman bütçesiyle (ör. 1 ms) eklenir. Mesh/texture yüklemesi asset
  sisteminin asenkron yolundan. Kare takılması yok.
- **Boşaltma:** mesafe + histerezis (sınırda git-gel yüklemesin). Boşaltılan hücrenin entity'leri
  silinir; oyunun değiştirdiği durum (kırılan kapı, alınan eşya) **hücre farkı** olarak saklanır ve
  yeniden yüklemede uygulanır (aynı override mekanizması: ADR-0015).
- **Hücreler arası referans:** `"hücre/id"` kalıcı kimlikleri; hedef yüklü değilse referans boş görünür,
  yüklenince bağlanır.

### 4. Büyük koordinatlar: kayan orijin

Dosyalarda konumlar `double`. Çalışırken `Transform` `float` kalır ama **dünya orijinine göredir**;
kamera orijinden belli bir mesafe (ör. 2 km) uzaklaşınca orijin kaydırılır: kök transform'lar, fizik
body'leri ve render geçmişi (TAA) tek adımda taşınır, bir `OriginShifted` olayı gönderilir. ECS matematiği
hızlı kalır; on kilometrelerce harita titremez.

### 5. Performans ilkeleri

- Hücre başına tek ikili blok (cook), sıfır JSON ayrıştırma; aynı mesh'ler instancing ile tek çizim.
- Yükleme ve çizim **ölçülür**: profiler'da hücre yükleme süresi, entity sayısı, bellek; debug çizimde
  hücre ızgarası ve yükleme yarıçapları.
- HLOD (uzaktaki hücrelerin birleştirilmiş/sadeleştirilmiş vekil mesh'i), GPU culling ve meshlet LOD
  ikinci aşamada (M22) — veri modeli bunlara hazır: her hücre kendi sınır kutusunu ve vekilini taşıyabilir.

### 6. Oyun akışı

- `ctx.open_world("maps/island.world")`: haritayı açar (eskisini kapatır); `ctx.load_scene` bölüm örneği
  ve küçük haritalar için kalır.
- Veri katmanları koddan/script'ten: `ctx.world_layers().activate("gece")`.
- Arazi bölgeleri (ADR-0018) ve SDF hacimleri aynı hücre ızgarasıyla stream edilir.

## Sonuçlar

- "Harita nedir" sorusunun tek cevabı: `.world` + entity klasörü. Oyun ayarı, harita ayarı ve içerik
  ayrıldı (Single Responsibility).
- Yeni streaming politikası ya da yeni ızgara türü mevcut kodu değiştirmeden eklenir (Open/Closed).
- Bilinçli olarak yok: çok oyunculu sunucuda oyuncu başına streaming (ağ modülü gelince), çalışırken
  haritayı düzenleyen tam editör (debug UI'da yerleştirme/taşıma ile başlar).
