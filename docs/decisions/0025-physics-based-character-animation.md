# ADR-0025 — Fizik tabanlı karakter animasyonu: iskelet, IK, kaslar, aktif ragdoll, davranışlar (Euphoria tarzı)

- **Durum:** Kabul (uygulama M20 animasyon + IK, M21 karakter fiziği)
- **Tarih:** 2026-11-22
- **Bağlam:** ADR-0010 / M4 (Jolt fiziği), ADR-0017 (Luau), ADR-0023 (özellik paketleri), ADR-0019
  (debug çizim/UI), yol haritası M20, M21; kılıç arenası (yer tutucu) bunun üstüne kurulacak.

## Araştırma

### Euphoria ve Endorphin (NaturalMotion)

- **Endorphin:** NaturalMotion'ın çevrim dışı (offline) aracı. Animatör karaktere "düş", "dengeni koru",
  "yüzünü koru" gibi davranışlar verir, araç fizikle simüle edip sonucu animasyon dosyası olarak dışa
  aktarır. Oyun içinde çalışmaz; satışı uzun zaman önce durdu.
- **Euphoria:** aynı fikrin oyun içinde çalışan hâli ("Dynamic Motion Synthesis"). Karakter, eklemleri
  motorlu bir fizik gövdesidir; her fizik adımında **davranış** denetleyicileri (denge, düşerken elle
  yakalanma, darbeye karşı kasılma, havada çırpınma, yaralı yeri tutma…) kaslara hedef verir. Sonuç her
  seferinde farklıdır: aynı kurşun, merdivende, arabaya çarpınca ya da koşarken başka tepki doğurur.
- GTA IV, GTA V, Red Dead Redemption 1-2, Max Payne 3 Euphoria kullandı (Rockstar'ın RAGE motoruna
  bağlı). NaturalMotion 2014'te Zynga'ya geçti; 2017'de Euphoria/Morpheme lisanslamasını bıraktı. Bugün
  Rockstar'ın elindeki sürüm iç kullanımda; dışarıdan alınamaz.
- Euphoria'nın iç algoritmaları yayınlanmadı. Bilinenler: ~15–20 gövdeli iskelet, eklem başına
  güç/sertlik/sönüm, davranışların öncelik ve ağırlıkla birleştirilmesi, denge için kütle merkezi +
  adım atma, animasyonla harmanlama.

### GTA 6 (RAGE)

- Rockstar karakter fiziği için resmî teknik açıklama yapmadı; "Euphoria 2.0" iddiaları doğrulanmış
  değil. Bulunan Take-Two patentleri: kısa hareket parçalarından prosedürel yürüme birleştirme
  ("virtual character locomotion") ve çalışma anında yeniden hedefleme (runtime retargeting) — yani
  motion matching'e benzer veri güdümlü animasyon + duruma göre uyarlama. Ragdoll katmanının
  Euphoria çizgisini sürdürdüğü varsayılıyor.
- Çıkarım: modern AAA yaklaşımı **üç katman**: veri güdümlü kinematik animasyon (motion matching /
  parça birleştirme) → prosedürel uyarlama (IK, retargeting, ayak yerleştirme) → fizik (aktif ragdoll,
  davranışlar). Biz de aynı katmanlamayı kuruyoruz.

### Modern sistemler

| Sistem | Yaklaşım | Bizim için ders |
|---|---|---|
| UE5 Physics Control | Eklem başına "control" (açısal/doğrusal güç, sönüm oranı, ek sönüm) + "body modifier" (kinematik/simüle, yerçekimi çarpanı); profiller asset'te | Kas parametrelerini **güç + sönüm oranı** olarak ver (1 = kritik sönüm), ham sertlik değil |
| UE5 Physical Animation | Animasyon pozuna yaylarla çekme, gövde bölgesi başına ağırlık | Gövde bölgesi başına fizik/animasyon karışımı |
| Jolt `Ragdoll` | `RagdollSettings` (iskelet → gövde + `SwingTwistConstraint`), `DriveToPoseUsingMotors`, `DriveToPoseUsingKinematics`, motor `PositionAndVelocity` (sertlik·konum hatası + sönüm·hız hatası) | Altyapı hazır; kas modelini bunun üstüne yazarız |
| Ubisoft DReCon / SuperTrack | Motion matching + öğrenilmiş (RL / dünya modeli) denetleyici fizik karakterini sürer | Güçlü ama eğitim altyapısı ister → ileride isteğe bağlı "politika" davranışı |

## Karar

### 1. Katmanlar (alttan üste)

```
5  Davranışlar   denge · darbe tepkisi · düşüşü yakala · başını koru · sendele · kalk · tutun
4  Kaslar        eklem başına hedef + güç/sönüm/maks tork; kas profilleri; yorgunluk/acı
3  Fizik gövdesi .ragdoll: kemik → kapsül, eklem sınırları (Jolt Ragdoll + SwingTwist)
2  IK            iki kemik, zincir (FABRIK), bakış, ayak yerleştirme, el uzanma
1  Animasyon     iskelet, poz, klip, blend, root motion (M20); motion matching (sonra)
```

Her katman alttakinin **pozunu** girdi alır, kendi pozunu üretir; katmanlar birbirini doğrudan
çağırmaz (ADR-0023). Ortak dil `Pose` (kemik başına yerel dönüşüm dizisi) tipidir.

### 2. Modüller

- `Astral::Animation` (M20, yeni): `Skeleton`, `Pose`, `AnimationClip`, blend, root motion, IK
  çözücüleri. Fizikten bağımsız; saf matematik, deterministik, test edilebilir.
- `Astral::Physics` genişler (M21): `RagdollDesc`, `Ragdoll` tutamağı, `MuscleProfile`. Jolt tipleri
  `src/` içinde kalır.
- `Astral::Character` (M21, yeni, katman 3): `CharacterBody` component'i, durum makinesi, davranışlar.
  Animation + Physics + World'e bağlı; oyun bu modülü kullanır.

### 3. Fizik gövdesi (`.ragdoll`)

- JSON asset: her gövde → kemik adı, şekil (kapsül/kutu), kütle oranı; her eklem → tür (swing-twist /
  menteşe), koni ve burulma sınırları. Varsayılan insan şablonu (~15 gövde: pelvis, 3 omurga, boyun,
  baş, kol/önkol/el ×2, uyluk/baldır/ayak ×2).
- Çok ayrıntılı görsel iskelet → az ayrıntılı ragdoll iskeleti **eşlemesi** (Jolt `SkeletonMapper`).
- Kendi kendine çarpışma, yakın gövdeler arasında kapalı (grup filtresi).

### 4. Kaslar

- Her eklemde bir PD motor: hedef açı (animasyon/davranıştan), **güç** (0…1 × profilin maks torku),
  **sönüm oranı** (1 = kritik). Ham sertlik/sönüm motor içinde hesaplanır: kullanıcı ve yapay zekâ
  okunabilir değerlerle çalışır.
- `MuscleProfile` (veri): gövde bölgeleri (bacaklar, gövde, kollar, boyun) için güç/sönüm. Örnek ön
  ayarlar: `relaxed`, `alert`, `braced`, `injured`, `limp`.
- Durum değerleri: yorgunluk (güç zamanla düşer), acı/yara (vurulan bölge geçici zayıflar), bilinç
  (0 → tamamen gevşek).
- Kökten yardım ("el tutma"): denge için pelvise isteğe bağlı, sınırlı bir kuvvet; varsayılan kapalı,
  oyun tasarımına göre açılır (GTA'daki "fazla gerçekçi düşme" ile oynanabilirlik arasındaki ayar).

### 5. Karakter durumları

| Durum | Ne olur |
|---|---|
| Kinematik | Yalnız animasyon; fizik gövdeleri animasyonu izler (çarpışma için) |
| Güçlü (powered) | Fizik animasyonu kaslarla izler; dış itme küçük sapma yaratır |
| Kısmi | Gövde bölgesi başına karışım (ör. üst gövde vuruldu, bacaklar animasyonda) |
| Aktif ragdoll | Davranışlar kasları sürer (denge, düşüş, kasılma) |
| Pasif ragdoll | Kaslar kapalı/çok zayıf (bilinçsiz, ölü) |
| Kalkma | Yere oturmuş poza en yakın kalkma klibi seçilir, kaslar o klibe geçiş yapar |

Geçişler olaylarla tetiklenir (darbe, ayak kayması, patlama, araç çarpması); sistemler birbirini
çağırmaz.

### 6. Davranışlar (Euphoria tarzı)

- Her davranış küçük bir sınıf: `update(BehaviourContext&) → { hedef poz katkısı, kas ayarı, ağırlık }`.
  Girdi: gövde durumları, kütle merkezi, destek poligonu (ayak temasları), dünya sorguları (raycast:
  yakın duvar, zemin). Çıktılar **öncelik + ağırlıkla** birleşir (ör. başını koru > düşüşü yakala >
  denge).
- İlk set: `Balance` (kütle merkezini destek poligonunda tut, gerekirse adım at), `Stagger` (sendele,
  geri adım), `HitReaction` (darbe yerinde zayıflama + impuls), `BraceForImpact`, `CatchFall` (kolları
  düşüş yönüne), `ProtectHead`, `Writhe` (yerde kıvranma), `GetUp`, `GrabLedge` (sonra).
- Veri: `.behaviour` ön ayarları (JSON) davranışların ağırlık ve parametrelerini tutar; oyun kodu
  `character.react(hit)` gibi tek çağrı yapar, Luau'dan da aynı API (ADR-0017).
- Sonra: öğrenilmiş denetleyici (`PolicyBehaviour`, ONNX benzeri küçük ağ) aynı arayüzü uygular; çekirdek
  sistem değişmez.

### 7. Performans ve ölçek (açık dünya)

- Fizik LOD'u: yakın = tam aktif ragdoll, orta = yalnızca güçlü mod + basit darbe tepkisi, uzak =
  kinematik. Kare başına aktif ragdoll bütçesi (varsayılan 16); bütçe dolunca en uzak/en eski uyutulur.
- Davranışlar sabit fizik adımında koşar (deterministik); ağır dünya sorguları kare başına sınırlı.
- Ağ: NPC ragdoll'ları istemcide kozmetik; oyunu etkileyen sonuç (ölü/yere düştü) sunucudan.

### 8. Araçlar ve test

- Debug çizim: kütle merkezi, destek poligonu, eklem sınırları, motor torku (renk); ImGui paneli ile
  kas ve davranış ağırlıklarını canlı ayarlama.
- Testler: IK çözücüleri saf matematikle; ragdoll deterministik tekrar (aynı girdi → aynı poz); denge
  testi (itilen karakter N saniye ayakta kalır); görüntü testi (darbe sahnesi).

## Sonuçlar

- Euphoria'yı lisanslayamıyoruz; aynı fikri açık parçalarla (Jolt ragdoll + motorlar) kendimiz
  kuruyoruz. Kalite davranışların ince ayarına bağlı: bu iş veri (profiller, ön ayarlar) ve görsel
  araçlarla yapılacak.
- Yol haritası: M20 = animasyon + IK; M21 = karakter fiziği (bu ADR); araçlar, kumaş, soft body,
  kopan parçalar yeni M23'e taşındı.
- Kılıç arenası (yer tutucu) M21'in çıkış sahnesi olur: vurulan, sendeleyen, düşüp kalkan karakterler.

## Kaynaklar

- Euphoria: https://en.wikipedia.org/wiki/Euphoria_(software)
- NaturalMotion lisanslamayı bıraktı: https://www.pocketgamer.biz/news/65937/naturalmotion-ends-commercial-tech-licensing-business/
- Take-Two animasyon patentleri: https://www.gamepressure.com/newsroom/gta-6-may-impress-with-realistic-animations-thanks-to-new-technol/z262b8
- Jolt sürüm notları (ragdoll motorları): https://raw.githubusercontent.com/jrouwe/JoltPhysics/master/Docs/ReleaseNotes.md
- Jolt örnekleri: https://jrouwe.github.io/JoltPhysics/md__docs_2_samples.html
- UE5 Physics Control: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl
- Ubisoft DReCon: https://montreal.ubisoft.com/en/drecon-data-driven-responsive-control-of-physics-based-characters/
- Ubisoft SuperTrack: https://montreal.ubisoft.com/en/supertrack-motion-tracking-for-physically-simulated-characters-using-supervised-learning/
