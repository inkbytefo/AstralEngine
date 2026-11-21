# ADR-0024 — VFX altyapısı: hacim tabanlı efektler (CS2 dumanı gibi), GPU partikülleri, `.vfx`

- **Durum:** Kabul (uygulama M14; ADR-0022'nin `volume`/`particle` shader türleriyle)
- **Tarih:** 2026-11-21
- **Bağlam:** Counter-Strike 2 hacimsel duman bombası, ADR-0011 (RT: ray query, DDGI), ADR-0021 (dünya),
  yol haritası M12 (alanlar: rüzgâr)

## Araştırma: CS2'nin dumanı nasıl çalışıyor

Valve'ın yayınlanmış bir makalesi yok; davranış ve yeniden üretimlerden (Acerola, UE5/Unity örnekleri,
SMU Guildhall tezi) çıkan ortak model:

1. **Voksel ızgarası:** patlama noktasında küçük (≈20–30 cm) görünmez küplerden bir ızgara açılır.
2. **Flood fill:** duman merkezden komşu vokselere yayılır; duvar engelliyse geçmez, açık yoldan
   dolanır. Hacim sınırlıdır: kapalı köşeye sıkışan duman koridora taşar, kapıdan, merdivenden süzülür.
   Yayılma zamanla animasyonludur (yumuşatma eğrisi).
3. **Tek veri, her şey:** aynı ızgara görüntüyü, görüş engellemeyi, mermi ve el bombası etkileşimini
   sürer. Mermi yolundaki vokselleri geçici olarak boşaltır (tünel), HE bombası bir küreyi iter/boşaltır,
   duman sonra geri dolar.
4. **Render:** her piksel için ızgarada raymarch; Beer–Lambert geçirgenlik, ışık saçılması (faz
   fonksiyonu), voksel altı ayrıntı için 3D gürültü (Worley/curl) ve kaynaşma. Duman sahnenin ışığını
   alır.
5. **Ağ:** şekil deterministik; istemciler aynı tohumla aynı dolumu hesaplar, ızgara gönderilmez.

## Karar: genel bir "hacimsel efekt" sistemi; CS2 dumanı onun ilk ön ayarı

### 1. İki katmanlı veri

| Katman | Nerede | Ne |
|---|---|---|
| **Oyun ızgarası** (kaba, ≈25 cm) | CPU, deterministik | Doluluk + yoğunluk; flood fill, mermi/patlama etkileşimi, görüş sorgusu (`smoke_density_along(ray)`) — yapay zekâ görüşü ve ağ için doğruluk kaynağı |
| **Görüntü hacmi** (ince) | GPU, seyrek tuğla atlası (8³ tuğlalar, paylaşılan 3D texture havuzu) | Oyun ızgarasından türetilir: gürültü, kıvrılma, rüzgâr taşıması, yumuşak kenarlar, ısı/renk |

Oyun kararları asla GPU'dan geri okunmaz; GPU yalnızca güzelleştirir. Böylece deterministiklik ve
performans aynı anda.

### 2. Duvarları bilmek: ızgarayı dünyaya oturtmak

Flood fill'in "duvar var mı" sorusu, komşu voksel merkezleri arasında **kısa ışınla** sorulur:
- Fizik sorgusu (Jolt raycast / shape cast) — her cihazda, deterministik; oyun ızgarası bunu kullanır.
- Ayrı bir dünya vokselleştirmesi gerekmez; açılan kapı, yıkılan duvar (M19) kendiliğinden hesaba girer.
Sorgular JobSystem'de paralel; tipik bir bomba birkaç bin voksel, milisaniyenin altında.

### 3. Simülasyon (her kare, sabit adım)

- Yayılma: dalga dalga BFS, bütçeli hacim; yaşam süresi sonunda kenarlardan erime.
- Etkileşim olayları: `ProjectileHit` → kapsül boşalt; `Explosion` → küreyi it/boşalt; hareket eden
  karakter → küçük iz. Boşluklar zamanla komşulardan dolar.
- Rüzgâr alanı (M12) görüntü hacmini sürükler; oyun ızgarası yalnızca güçlü rüzgârda kayar.

### 4. Render

- Hacimsel geçiş (compute, yarım/çeyrek çözünürlük): derinliğe kadar raymarch, Beer–Lambert, Henyey–Greenstein,
  güneş ışığı (gölge haritası veya ray query ile), ortam ışığı DDGI'dan, kendi gölgesi için ışık
  geçirgenliği hacmi. Zamansal yeniden kullanım (TAA geçmişiyle) + kenar koruyan büyütme.
- M14'teki froxel sisiyle aynı "katılımcı ortam" yolu: duman, sis, toz, zehirli gaz, ateş tek
  birleştiriciden geçer; şeffaflar ve partiküller doğru sıralanır.
- Görünüm `shader_type volume` ile yazılır (ADR-0022): yoğunluk → renk, saçılma, ışıma. Ateş = ışıyan
  hacim; zehirli gaz = yeşil, yere çöken; duman bombası = ön ayar.

### 5. VFX'in geri kalanı ve `.vfx` dosyası

- **GPU partikülleri:** emitter'lar, `shader_type particle` (güncelleme + çizim), alan örnekleme
  (rüzgâr, su akışı), derinlik çarpışması, DDGI ile aydınlatma.
- **Decal'lar ve izler:** mermi deliği, yanık, ayak izi.
- **`.vfx` bileşimi:** bir efekt = partiküller + hacim + ışık + ses + decal + kamera sarsıntısı, zaman
  çizelgesiyle. Bir olay (`Explosion`) bir `.vfx` çalar; oyun kodu efektin içini bilmez.

## Sonuçlar

- Duman bombası oyun tarafında ~birkaç satır: `spawn_vfx("vfx/smoke_grenade.vfx", pos)`; görüş için
  `smoke_density_along`.
- Aynı altyapı M17 (hava: sis, yağmur perdesi) ve M19 (yıkım tozu) tarafından yeniden kullanılır.
- Bilinçli sınırlar: tam Navier–Stokes akışkan simülasyonu değil (ileride yerel efektler için araştırma);
  bir anda aktif hacim sayısı ve toplam tuğla bütçesi sınırlı (ayar).
