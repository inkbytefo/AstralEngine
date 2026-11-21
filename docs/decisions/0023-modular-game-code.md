# ADR-0023 — Modüler oyun kodu: özellik paketleri, SOLID kuralları, yapay zekâ dostu düzen

- **Durum:** Kabul (kurallar hemen geçerli; araçlar M11'de ADR-0017 ile birlikte)
- **Tarih:** 2026-11-21
- **Bağlam:** ADR-0001 (kütüphane), ADR-0006 (App + System), ADR-0017 (Luau), docs/architecture.md (motor için SOLID)

## Sorun

Motor katmanında modül sınırları net (her modül kendi klasöründe, bağımlılık tek yönde). Oyun tarafında
böyle bir kural yok: büyüyen bir oyunda sistemler birbirini doğrudan çağırmaya, global durum biriktirmeye,
tek dev dosyaya dönüşmeye başlar (spagetti). Kodun çoğunu yapay zekâ yazacağı için bu risk daha büyük:
model geniş bir dosyayı yanlış yerden değiştirir, sınırları bilmezse her yere dokunur.

## Karar

### 1. Oyun = özellik paketleri (feature)

```
game/
  features/
    combat/        feature.json  components.hpp  events.hpp  systems.cpp  combat.luau  tests/  assets/
    inventory/     ...
    smoke_grenade/ ...
  maps/            island.world  island.entities/...
  main.cpp         // yalnızca özellikleri kurar ve haritayı açar
```

- Her özellik tek bir `install(FeatureContext&)` ile kendini kurar: component'leri kaydeder, sistemleri
  ekler, ayarlarını okur (Pong'daki `pong::install` bunun ilk örneği).
- `feature.json`: ad, bağımlı olduğu özellikler, dışa açtığı component ve olaylar, ayarlar. Motor
  bağımlılık sırasını buradan kurar; döngüsel bağımlılık hata verir.
- Bir özellik başka bir özelliğin yalnızca **dışa açtığı** başlıklarını (component + olay) kullanabilir;
  iç sistemlerine dokunamaz. Build bunu denetler.

### 2. Kurallar (SOLID'in oyundaki karşılığı)

| İlke | Kural |
|---|---|
| **Tek sorumluluk** | Component = yalnızca veri (reflect edilir, pointer yok, mantık yok). Sistem = tek iş (`BallPhysics` topu hareket ettirir; ses çalmaz) |
| **Açık/kapalı** | Yeni davranış yeni sistem veya olay dinleyicisiyle eklenir; mevcut sistem değiştirilmez. Pong'a ses eklemek `BallPhysics`'i değiştirmedi, `Bounced` olayını dinleyen `MatchAudio` eklendi |
| **Liskov** | Aynı component'i kullanan her sistem onun anlamına uyar; "özel durum" bayrakları yerine ayrı component (`Stunned`, `Burning`) |
| **Arayüz ayrımı** | Sistem yalnızca sorguladığı component'leri görür; `Context` servisleri küçük arayüzler (ses, fizik sorgusu, asset, input eylemleri) |
| **Bağımlılık tersine çevirme** | Servisler `Context` üzerinden gelir, global/singleton yok; testte sahte/sessiz sürüm takılır (sessiz ses cihazı gibi) |

Ek kurallar:
- Sistemler birbirini **çağırmaz**; olaylar (`World::send` / `EventReader`) veya component'ler üzerinden
  konuşur. Olaylar özellikler arası sözleşmedir.
- Durum component'lerde ve dünya kaynaklarında; sistem içinde gizli durum yok (hot-reload, kayıt ve
  tekrar oynatma bunu gerektirir).
- Sayılar kodda değil, dosyada (`Court`, `Sounds` gibi reflect edilmiş ayar component'leri).
- Simülasyon sabit adımda ve deterministik: aynı tohum, aynı maç. Test ve ağ bunun üstüne kurulur.

### 3. Yapay zekâ dostu düzen

1. **Küçük, kalıplı dosyalar.** Her özellik aynı iskelet; bir model bir özelliği açınca nereye ne
   yazacağını bilir. Dosya başına tek konu.
2. **Kendini anlatan API.** Reflection'dan üretilen `astral.d.luau` ve `API.md` (her component, alan,
   olay, servis); ajan kodu tahmin etmez, okur.
3. **Kapalı geri bildirim döngüsü.** Her özelliğin headless testleri var; ajan "yaz → test → hata mesajı →
   düzelt" döngüsünde çalışır. Hata mesajları ne yapılacağını söyler.
4. **Sınır denetimi otomatik.** Özellik sınırı ihlali, global durum ve yasak include'lar build/lint
   hatasıdır; ajan sınırı unutursa test kırılır.
5. **Ajan talimatı repoda.** `AGENTS.md`: özellik iskeleti, kurallar, komutlar (test, paket, yakalama).
6. **Script önce.** Oyun kuralları Luau'da (ADR-0017): derleme yok, çökme yok, saniyelik döngü; ağır
   iş gerekince aynı component'ler üstünde C++ sistemine taşınır.

## Sonuçlar

- Bir özellik silinince oyun derlenmeye devam eder (bağımlı olanlar hariç; bunu `feature.json` söyler).
- Pong M11'de bu düzene taşınır: `features/pong_rules` (Luau) + `features/pong_audio`.
- Bedel: küçük bir oyun için birkaç dosya fazlası. Kabul: büyüdükçe kazandırır.
