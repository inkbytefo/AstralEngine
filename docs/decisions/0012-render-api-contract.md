# ADR-0012: Render API sözleşmesi; ECS, sahne ve paketleme yönü

**Durum:** Kabul · 2026-11-21

## 1. Render API sözleşmesi (uygulandı, 0.7.0)

**Bağlam:** M3.5 sonunda render API'si üç yerde öteki modüllerle sürtünüyordu: materyal her instance'ta
satır içiydi (asset sistemi ve GPU'daki materyal tablosuyla uyumsuz), hareket vektörleri yalnız kameradan
geliyordu (hareketli nesnede TAA/denoiser izi) ve ayarlar yalnız kurucuda veriliyordu.

**Karar:**
- **Kaynak / kare ayrımı.** Mesh ve materyal Renderer'ın sahip olduğu **handle**'lardır; her kare çağıran
  saf veriden bir `RenderScene` **çıkarır** (extract) ve `render()` çağırır. Renderer sahneyi saklamaz,
  ECS'i bilmez. App bu çıkarmayı ECS'ten yapar; başka bir motor/araç kendi verisinden yapabilir.
- **Materyal = paylaşılan kaynak** (`MaterialDesc`, glTF metallic-roughness). GPU'da tek materyal tablosu;
  raster G-buffer ve RT vuruş gölgelendirmesi aynı tabloyu okur. Silinmiş materyal → varsayılan materyal.
- **Hareket:** `MeshInstance::previous_transform`; G-buffer hız hedefi yazar. App önceki transform'u
  iç bir component'te kendisi tutar — oyun kodu hiçbir şey yapmaz.
- **Ayarlar çalışma anında** (`set_settings`); teknik/GI değişirse geçmiş sıfırlanır, gereken GPU kaynakları
  (RT pipeline'ları, BLAS'lar) tembel kurulur.
- **Headless güvenliği:** `Context::create_mesh/create_material` renderer yokken geçersiz handle döner;
  oyun kodu sunucuda/testte dallanmadan çalışır.

**Modüllerle ilişki:**

| Modül | İlişki |
|---|---|
| World (ECS) | Yok — render ECS'i bilmez. Köprü App'teki çıkarma adımıdır |
| App | Component'lerden `RenderScene` çıkarır; transform geçmişini tutar |
| Assets (M5) | Dosyadan yükler, Renderer'ın `create_*` fonksiyonlarını çağırır; Renderer GPU sahibi kalır. `MaterialDesc` texture handle'ları kazanır |
| Scene (M5) | Serileştirilmiş component'ler handle değil **AssetId** tutar; yüklemede handle'a çözülür |
| Physics | Yok |

## 2. ECS: yeniden yazmıyoruz, tamamlıyoruz

`Astral::World` zaten gerçek bir archetype ECS (SoA sütunlar, önbellekli sorgular, paralel `each`,
ertelenmiş komutlar, hiyerarşi). Eksik olan çekirdek değil, üstündeki katman:

1. **Reflection:** `ASTRAL_REFLECT(Type, field...)` ile derleme zamanı alan listesi (ad, tip, offset) +
   kalıcı tip kimliği (`component_name_hash`). Serileştirme, editör, ağ ve AI araçları bunun üzerine kurulur.
2. **Filtreler:** `without<T>`, `added<T>`, `changed<T>` — satır başına değişiklik sayacı (tick).
3. **Olaylar:** `Events<T>` kaynağı, kare başına çift tamponlu.
4. **Serileştirme:** reflection'dan otomatik; `Entity` referansları yüklemede yeniden eşlenir.

## 3. Sahne ve dünya modeli

| Kavram | Ne | Biçim |
|---|---|---|
| `World` | Çalışan ECS dünyası | bellek |
| `Scene` asset'i | Entity + component verisi (bir oda, bir arena) | yazarken **JSON** (diff'lenebilir, editör/AI dostu); paketlenmişte ikili |
| `Prefab` | Örneklenebilir sahne + alan geçersiz kılmaları (iç içe) | Scene ile aynı |
| `Map` (açık dünya) | Hücrelere (ör. 64 m) bölünmüş sahneler; kameranın etrafındaki hücreler akıtılır (RAGE'in streaming hacmi), uzakta HLOD | sonraki faz |

Asset'lere çalışma anında yol (string) ile değil **AssetId** (kalıcı 64-bit kimlik) ile ulaşılır.

## 4. Oyun paketleme

```
kaynak (glTF, PNG, .scene.json) ──cook──► GPU'ya hazır ikili (vertex/index blob, BC7/BC5 + mip texture,
                                          materyal, ikili sahne) ──pack──► game.apak
```

- **cook** (`astral-cook` CLI): içerik hash'iyle artımlı; platforma göre çıktı. Geliştirmede gevşek cook
  dosyaları + hot-reload; dağıtımda `.apak`.
- **`.apak`:** başlık + içindekiler tablosu (AssetId → offset, boyut, sıkıştırma, bağımlılıklar) + zstd
  bloklar; memory-map ile okunur, JobSystem ile asenkron.
- **CMake:** `astral_add_game(MyGame SOURCES ... ASSETS content/)` → exe + cook + `game.apak` tek hedefte.

## 5. Sıra

M5a ECS tamamlama → M5b asset sistemi (AssetId, async yükleme, hot-reload, glTF, texture'lı materyal) →
M5c sahne/prefab → M6 paketleme → M7 kılıç arenası dikey dilimi → dünya bölümleme/streaming.
Gerekçe: her adım bir öncekinin üstüne oturur — sahne reflection'sız serileştirilemez, paket asset
kimlikleri olmadan kurulamaz, oyun da bunların hepsini ilk kez gerçek yükte dener.
