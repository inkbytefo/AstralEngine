# G1 — Oyun geri bildiriminden genel motor iyileştirmeleri (plan)

- **Durum:** Plan (onay bekliyor, uygulanmadı)
- **Tarih:** 2026-11-25
- **Kaynak:** `examples/paperwing/MOTOR_ISTEKLERI.md` (PAPERWING — kâğıt uçak uçuş oyunu; ajan tarafından üretildi, depoya commit'lenmedi)
- **Taban:** master `10eadc8`

## Amaç

Tek bir oyunun (uçuş) isteklerini **türe özgü yamalar** olarak değil, her oyun türünün kullanacağı **genel motor yetenekleri** olarak eklemek. Her madde kaynakta doğrulandı; aşağıdaki "Kanıt" satırları mevcut kodu gösterir.

İlke:
- Önce mevcut alt katman yeteneğini üst katmanlara (servis, Luau) açmak; yeni sistem yazmak en son.
- Türe özgü davranış (aerodinamik ayrıntısı, termal profili) motor çekirdeğine değil, `features/` paketlerine veya veri alanlarına gider.
- Her iş: TDD (önce başarısız test), ayrı worktree dalı, gerekiyorsa ADR, "Faz bitti" dokümanları ([process.md](../../process.md)).

## Zaten kapandı

| Madde | Durum |
|---|---|
| `sky_pass.cpp:68` `uint32_t → float` (C4244 / hash) | Düzeltildi: `10eadc8` (master, push'lu) |

## Dalga G1.1 — Hızlı ve genel kazanımlar (küçük, düşük risk)

### 1. Fizik komut/sorgu API'sini tamamla (C++ servis + Luau)
- **Kanıt:** `PhysicsWorld::add_force_at` ve `add_torque` var (`modules/physics/include/astral/physics/physics.hpp:298-300`). `PhysicsService` yalnız `add_impulse/add_force/set_linear_velocity/set_angular_velocity` sunuyor, açısal hız okuma yok (`modules/app/include/astral/app/physics_service.hpp`). Luau `astral.physics` yalnız `impulse/force/set_velocity/velocity` (`modules/script/src/bindings_services.cpp`).
- **Yapılacak:**
  - `PhysicsService`: `add_torque(Entity, Vec3)`, `add_force_at(Entity, Vec3 force, Vec3 world_point)`, `add_angular_impulse(Entity, Vec3)`, `angular_velocity(Entity) const`. Hepsi ADR-0043 kuyruğundan (`order_key`, producer sırası) geçer; `PhysicsCommandKind` genişler.
  - Luau: `astral.physics.torque`, `force_at`, `angular_impulse`, `angular_velocity`, `set_angular_velocity`. ADR-0044 gereği yükleme aşamasında yasak listesine eklenir.
  - `raycast` Luau seçenek tablosu: `{ ignore = {e1, e2}, layers = {...}, sensors = true }` → `QueryFilter` (çoklu ignore için `QueryFilter`'a küçük bir ignore listesi gerekiyorsa ADR).
  - `API.md` / `astral.d.luau` paritesi (mevcut parite testi).
- **Test:** servis komutları seri = paralel (ADR-0043 testi kalıbı), Luau parite testi, `force_at` ile tork oluşumu (açısal hız işareti).

### 2. `ASTRAL_REFLECT` sınırı ve hata mesajı
- **Kanıt:** `ASTRAL_DETAIL_COUNT` 32 argümanla sınırlı (`modules/core/include/astral/core/reflect.hpp:126-130`); 33. alanda anlaşılmaz `ASTRAL_DETAIL_F<n>` hatası.
- **Yapılacak:** sayım makrosuna "fazla argüman" sentinel'i ekleyip `static_assert(false, "ASTRAL_REFLECT supports at most N fields; split the component")` üret. Sınırı 64'e çıkarmak ikincil (ADR-0030: component ≤ 25 veri üyesi tasarım hedefi; büyük component bölünmeli).
- **Test:** derleme-zamanı negatif test (CMake `try_compile` ya da `WILL_FAIL` hedefi) + 32 alanlı pozitif test.

### 3. Particle shader yerleşiklerinde tutarlılık
- **Kanıt:** `LIFETIME` yalnız `motion` aşamasında tanımlı (`modules/render/src/surface_shader_rt.cpp:98`); `particle()` aşamasında `AGE` normalize, `LIFETIME` yok.
- **Yapılacak:** her iki aşamada `AGE` (s), `LIFETIME` (s), `LIFE_T` (0..1) aynı anlamla. Geriye uyum: `particle()` içinde eski normalize `AGE` kullanan shader'lar için tek sürümlük uyarı veya `.ashader` sürüm alanı (ADR gerektirirse yazılır). `docs/guides/ashader-language.md` güncellenir.
- **Test:** iki aşamada da derlenen particle shader; golden değişmemeli (lavapipe).

### 4. Başsız testlerde girdi enjeksiyonu
- **Kanıt:** `Context::input` `const Input&` (`modules/app/include/astral/app/app.hpp:71`); `App` üzerinde test girdisi yolu yok.
- **Yapılacak:** `App::test_input()` (yalnız headless/offscreen'de anlamlı) ya da `AppDesc` ile verilen bir `InputScript` (kare → tuş/aksiyon listesi). Global/singleton yok; public header'a üçüncü parti tip girmez.
- **Test:** `astral.input.pressed("restart")` dalını tetikleyen başsız oyun testi.

### 5. `Atmosphere` sanatsal kontrol
- **Kanıt:** gökyüzü ortamı yalnız gece/gündüz lerp'i (`modules/app/src/services/atmosphere.cpp:37-40`); etkin `Atmosphere` her kare `ctx.sun`/`environment.sky`'ı yazar (`components.hpp:44`).
- **Yapılacak:** güneş yüksekliğine bağlı alacakaranlık / golden-hour renk eğrisi (fiziksel değil, sanatsal; veri alanları `twilight_color`, `horizon_tint`, `sky_tint` çarpanı) ve `drive_sun` / `drive_sky` bayrakları (elle ayarı ezmemek için).
- **Test:** saat 6/12/18/0 için deterministik ortam değerleri; bayrak kapalıyken `ctx.sun` dokunulmaz.

### 6. Windows derleme bellek baskısı
- **Kanıt:** rapor: 24 GB makinede Jolt ağır çeviri birimlerinde MSVC `C1060`. Bu makinede (bizim full build'ler) tekrarlanmadı.
- **Yapılacak:** Ninja `JOB_POOLS` ile `heavy_compile` havuzu (Jolt bağımlı kaynaklar), havuz boyu cache değişkeniyle (`ASTRAL_HEAVY_COMPILE_JOBS`, varsayılan çekirdek/2). Davranış değişmez, yalnız zamanlama.
- **Test:** CI ve yerel full build süresi/başarısı.

## Dalga G1.2 — Orta boy genel özellikler

### 7. Şekilli ve döndürülebilir alanlar (`WindZone`, `WaterBody`)
- **Kanıt:** ikisi de "axis-aligned, scale ignored" (`components.hpp:109, 126`).
- **Yapılacak:** `Transform.rotation` desteği (OBB), `shape = Box | Sphere | Cylinder`, isteğe bağlı radyal düşüş profili ve `swirl` (eksen etrafı teğetsel hız). CPU ve GPU (`wind_at`) aynı fonksiyon (ADR-0029 sözleşmesi korunur). Sahne formatı eski alanları okumaya devam eder.
- **Test:** CPU/GPU eşdeğerlik testi (mevcut kalıp), döndürülmüş kutuda iç/dış noktalar, silindir düşüşü.

### 8. Kaldırma yüzeyi (`Airfoil`) genelleştirme
- **Kanıt:** kuvvet kütle merkezine (`modules/app/src/services/physics_fields.cpp:71-92`); basınç merkezi, indüklenmiş/stall sürüklemesi yok.
- **Yapılacak:** madde 1'deki `add_force_at` üzerine `Airfoil.center_of_pressure` (yerel ofset), `induced_drag`, `stall_drag`, `zero_lift_alpha`. Varsayılanlar bugünkü davranışı aynen üretir. Planör/kuş/ok/yelken için ortak; ayrıntılı uçuş modeli `features/` paketi olur.
- **Test:** basınç merkezi kütle merkezinin arkasındayken yunuslama kararlılığı (açısal hız sönümü), varsayılanlarla eski sonuç bit bit aynı.

### 9. Sürekli VFX ve iz (trail)
- **Kanıt:** `.vfx` `loop` yok, `VfxEmitter` public component değil (`modules/app/src/services/vfx_runtime.cpp`).
- **Yapılacak:** `.vfx` `loop: true` (+ `duration` sınırı yalnız loop'suzlara), public `VfxEmitter` component'i (sahneye yazılabilir, Luau'dan açılıp kapanır), motor seviyesinde ribbon/trail renderer (genel: kılıç izi, araç lastiği, kanat ucu). Trail için ADR (render yolu: raster + RT davranışı).
- **Test:** loop'lu emitter'ın kare bağımsız parçacık sayısı, trail mesh üretiminin birim testi, golden.

### 10. `UiCanvas` ilkelleri ve font servisi
- **Kanıt:** yalnız `rect/frame/image/text` (`modules/render/include/astral/render/text.hpp:100-105`).
- **Yapılacak:** `line`, `polyline`, `circle`/`arc` (doldurulmuş ve çizgi), `rounded_rect`, dikey/yatay `gradient_rect`; birden fazla font için `Context` üzerinden ad → font kaydı. Luau'ya `astral.ui` (HUD çizimi: ekran uzayı, yalnız callback içinde) — ayrı ADR (script'ten çizim yaşam döngüsü).
- **Test:** üretilen tepe noktası sayısı/geometri birim testleri, golden UI sahnesi.

## Dalga G1.3 — Büyük işler (ayrı ADR ile)

### 11. Shader derleme önbelleği ve paralel derleme
- **Kanıt:** `.ashader` yüzeyleri çalışma anında `glslang` ile tek iş parçacığında, disk önbelleği olmadan derleniyor (`modules/render/src/surface_shader.cpp`, `surface_shader_rt.cpp`); `docs/modules/shader.md` "Bilinen sınırlar" aynı şeyi söylüyor. Raporda Debug soğuk açılış ~50–70 sn.
- **Yapılacak:** içerik hash'i (kaynak + include'lar + tanımlar + derleyici sürümü) ile anahtarlanan yerel SPIR-V önbelleği (oyun başına `.cache/shaders/`), cook'ta önceden derlenmiş SPIR-V'nin pakete girmesi, `JobSystem` ile aşamaların paralel derlenmesi. RT birleşik compute shader'ı için önbellek anahtarı tüm yüzeylerin hash'i.
- **Test:** sıcak açılışta sıfır `glslang` çağrısı, bozuk önbellek dosyasında güvenli yeniden derleme, paralel = seri SPIR-V.

### 12. Ses: gerçek zamanlı filtre ve prosedürel kaynak
- **Kanıt:** `AudioEngine` decode + `volume/pitch/3D` (`modules/audio/include/astral/audio/audio.hpp`).
- **Yapılacak:** ses ve grup (bus) başına biquad (LPF/HPF/BPF) parametreleri, ham float PCM'den ses oluşturma (`create_sound_from_pcm`), isteğe bağlı sentez callback'i (ses thread'inde, kilitsiz). `AudioSource`/Luau'ya filtre alanları. miniaudio node graph üzerinden.
- **Test:** filtre katsayı birim testleri, null device ile deterministik çıktı karşılaştırması.

## Kapsam dışı / bilinçli ertelenen
- Tam uçuş dinamiği (stabilite türevleri, kontrol yüzeyleri): `features/flight` paketi olarak, motor değil.
- Termal/girdap içeriği: madde 7'nin veri alanlarıyla oyunda yazılır.

## Sıra ve bağımlılık
1. G1.1 maddeleri birbirinden bağımsız; tek dalga, madde başına commit.
2. Madde 8, madde 1'e bağlı (`add_force_at`).
3. Madde 10'un Luau kısmı ve madde 9'un trail'i ADR sonrası.
4. G1.3 maddeleri ayrı dallar; 11 en yüksek DX etkisi.

## Açık sorular (uygulamadan önce)
- Madde 3: `particle()` içindeki `AGE` anlamını değiştirmek mevcut shader'ları bozar — sürüm alanı mı, yeni ad mı (`LIFE_T`) ile geriye uyumlu geçiş mi?
- Madde 4: test girdisi `App` API'si mi, `AppDesc` betiği mi?
- Madde 10: Luau'dan doğrudan çizim mi, yoksa HUD component'leri (veri) mi?
