# ADR-0017 — Oyun mantığı ve scripting

- **Durum:** Kabul (uygulama M11)
- **Tarih:** 2026-11-21
- **Bağlam:** ADR-0006 (App katmanı), ADR-0013 (reflection), ADR-0015 (sahneler), M7 (Pong)

## Sorun

Bugün oyun mantığı yalnızca C++ ile yazılıyor. Pong bunun iyi çalıştığını gösterdi, ama zayıf yanları da
var:

- Her kural değişikliği derleme ve yeniden başlatma istiyor. Tasarım döngüsü dakikalar sürüyor, saniyeler değil.
- Oyun kodundaki bir hata (geçersiz pointer, sonsuz döngü) motoru da çökertiyor.
- Yapay zekâ ile oyun yazarken C++'ın derleme ve bellek hataları en pahalı geri dönüş döngüsü.
- Mod desteği ve sahneye bağlı küçük davranışlar (kapı, tetik, görev adımı) için C++ fazla ağır.

## Karar

### 1. İki katman, tek ECS

| Katman | Dil | Ne yazılır |
|---|---|---|
| Motor ve ağır sistemler | C++ | Render, fizik, alanlar, animasyon, yapay zekâ navigasyonu; binlerce entity'ye dokunan sıcak döngüler |
| Oyun kuralları | **Luau** | Skor, maç akışı, silah davranışı, görevler, kapılar, tetikler, arayüz mantığı |

İki katman aynı ECS'i paylaşır. **Durum her zaman component'lerde tutulur, script'lerde değil.** Script
yalnızca davranıştır. Böylece:
- hot-reload güvenli (script yeniden yüklenince hiçbir veri kaybolmaz);
- kayıt ve sahne dosyaları script'in yazdığı durumu kendiliğinden içerir;
- bir kural C++ ile Luau arasında taşınabilir (Pong'un kuralları M11'de Luau'ya taşınacak, aynı testler
  geçecek).

### 2. Neden Luau

Luau, Roblox'un Lua türevi (MIT lisanslı).
- **Kademeli tip sistemi:** `local speed: number = 9`. Hatalar çalıştırmadan yakalanır; editör ve yapay
  zekâ için tam tamamlama sağlar.
- **Sandbox:** dosya, ağ ve işletim sistemi erişimi yok. Script motoru çökertemez; bir hata yalnızca o
  script'i durdurur.
- **Hız:** yorumlayıcısı Lua 5.x'ten belirgin hızlı. Opsiyonel native derleme, x64 ve ARM64'te.
- **Küçük ve gömülebilir:** C++ ile yazılmış, kendi bellek ayırıcısını kabul ediyor, çöp toplayıcısı
  artımlı (kare takılması yok).
- **Yaygın:** milyonlarca geliştiricinin kullandığı bir dil. Yapay zekâ modellerinin Luau'yu iyi
  yazması, bu projenin çalışma biçimi için belirleyici.

### 3. Bağlama: elle değil, reflection'dan

`ASTRAL_REFLECT` edilmiş her component Luau'ya kendiliğinden açılır; bağlama kodu elle yazılmaz.
- Alan erişimi, reflection'dan üretilen alan tablosuyla (ofset + tip) çalışır:
  `paddle.speed = 12`.
- `Vec3`, `Quat`, `Entity` için yerel değer tipleri. Matematik Luau'nun vektör hızlandırmasını kullanır.
- Motor API'si (dünya, sorgular, olaylar, fizik sorguları, ses, input) tek bir `astral` modülünden gelir.
- Reflection'dan **tip tanımı dosyası** (`astral.d.luau`) üretilir. luau-lsp ve yapay zekâ ajanları her
  component'i ve alanını bilir.

### 4. İki script biçimi

**Sistem script'i:** bir sorgu üstünde toplu çalışır; çok sayıda entity için doğru biçim.
```lua
--!strict
local astral = require("@astral")
local Paddle, Transform = astral.component("pong::Paddle"), astral.component("astral::Transform")

return astral.system {
    phase = "Update",
    query = { Paddle, Transform },
    update = function(ctx, entity, paddle, transform)
        local move = ctx.input:axis("move")              -- input eylemi
        transform.position += Vector3.new(move.x, move.y, 0) * paddle.speed * ctx.time.delta
    end,
}
```

**Entity davranışı:** tek bir entity'ye bağlı olaylar; kapı, tetik, görev nesnesi gibi tekil şeyler için.
```lua
-- door.luau; sahnede: "astral::Script": {"path": "scripts/door.luau"}
return astral.behaviour {
    on_trigger_enter = function(self, other)
        if other:has("game::Player") then self:send("game::DoorOpened") end
    end,
}
```

### 5. Script'te tanımlanan component'ler

Oyun verisi için C++ yazmak gerekmemeli:
```lua
astral.define_component("game::Health", { value = "f32", max = "f32", regen = "f32" })
```
Bunun için ECS'e **dinamik component tipleri** eklenecek: boyut, hizalama ve alan tablosu çalışma anında
gelen bir şemadan. Reflection'lı C++ component'leriyle aynı yolu kullanırlar: sahne dosyası, kayıt,
inspector ve script erişimi. Bu M11'in en önemli altyapı işi; bundan sonra editör ve ağ kodu da aynı
şemayı kullanır.

### 6. Çalışma kuralları

- **Fazlar:** script sistemleri C++ sistemleriyle aynı fazlara kaydolur (`PreUpdate`, `Update`,
  `FixedUpdate`, `PostUpdate`) ve aynı değişiklik takibini görür (`changed`, `added`).
- **Belirlenimcilik:** `FixedUpdate` script'leri sabit adımda çalışır. `math.random` motorun tohumlu
  üretecine bağlanır; aynı tohum aynı maçı verir.
- **Hata:** derleme ya da çalışma hatası dosya:satır ile loglanır; o script devre dışı kalır, oyun sürer.
  Dosya düzeltilince yeniden yüklenir.
- **Hot-reload:** dosya değişince modül yeniden yüklenir. Durum component'lerde olduğu için kaybolmaz.
- **Cook:** script'ler Luau bytecode'una derlenip `.apak`'a girer. Dağıtılan oyunda kaynak kod gerekmez.
- **Bütçe:** script başına süre ölçülür (M8 profiler). Sonsuz döngü bir talimat sınırıyla kesilir.

### 7. Oyun çatısı (M11'de script'le birlikte)

- Input eylemleri: `move`, `jump`, `fire` gibi adlandırılmış eylemler; klavye, fare ve gamepad eşlemesi
  bir dosyadan gelir.
- Zamanlayıcılar ve tween'ler; tetik hacimleri (fizik sensörlerinin üstüne); durum makinesi yardımcısı;
  kayıt sistemi (`save_world` üstüne, seçilen component'lerle).

## Sonuçlar

- Yeni modül `Astral::Script`, katman olarak World ve App arasında: App script sistemlerini çalıştırır,
  World dinamik component'leri tutar.
- C++ ile oyun yazmak (`examples/pong` gibi) desteklenmeye devam ediyor; script zorunlu değil.
- ECS'e dinamik component desteği ekleniyor (şema tabanlı). Reflection'lı C++ tipleri de aynı şemayı
  üretir; tek kaynak.
- Script'ten C++'a geçiş maliyeti, her alan erişiminin bir tablo aramasına bedel olması. Bu yüzden
  binlerce entity'lik sıcak döngüler C++'ta kalır. Profiler hangi script'in pahalı olduğunu gösterir.

## Alternatifler

- **Yalnız C++ (bugünkü hâl):** en hızlısı, ama yavaş tasarım döngüsü ve çökmeye açık. Ağır sistemler için
  kalıyor.
- **C# (.NET):** güçlü dil ve araçlar, ama büyük bir çalışma zamanı gömmek gerekiyor, çöp toplayıcı
  duraklatıyor, mobil/konsol dağıtımı karmaşık.
- **Lua 5.4 / LuaJIT:** küçük ve hızlı, ama tip sistemi yok; LuaJIT'in bakımı belirsiz ve bazı platformlarda
  JIT yasak.
- **JavaScript (QuickJS):** tanıdık, ama yorumlayıcısı Luau'dan yavaş, oyun ekosistemi zayıf.
- **WebAssembly:** her dil kullanılabilir ve güçlü sandbox sağlar, ama derleme araç zinciri oyun yazarına
  yük olur. Mod desteği için ileride ikinci seçenek olarak değerlendirilebilir.
- **Hot-reload'lu C++ DLL:** C++ hızında, ama ABI ve durum taşıma kırılgan; çökme sorunu yine çözülmüyor.
- **Görsel scripting (node grafı):** mantık yazmak için verimsiz. Materyal ve VFX grafiği olarak ileride
  ayrıca düşünülebilir.
