# ADR-0026 — Script ana bilgisayarı, dinamik component'ler ve özellik paketlerinin uygulanışı (M11)

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** ADR-0017 (Luau), ADR-0023 (özellik paketleri), ADR-0013 (reflection). Bu belge M11'de
  uygulama sırasında alınan kararları ve ADR'lerden sapmaları kaydeder.

## Kararlar

1. **Luau, `Astral::Script` içinde derlenir.** Luau (MIT, 0.748) `FetchContent` ile alınır; VM + derleyici +
   ayrıştırıcı kaynakları modülün içine derlenir (Luau'nun kendi CMake hedefleri kullanılmaz). Böylece kurulum
   export'u Luau hedeflerini taşımaz, public header'da Luau tipi yoktur.
2. **Katman:** `Astral::Script` yalnız Core ve World'e bağlıdır. App'e, Input'a, Platform'a bağlı değildir:
   dünyaya `ScriptEnv` ile, girdiye küçük `ScriptInput` arayüzüyle ulaşır (arayüz ayrımı, bağımlılık tersine
   çevirme). Fazları App sürer.
3. **`require` yok.** Script'ler tek global `astral` tablosunu görür (ADR-0017'deki `require("@astral")`
   yerine). Sandbox basit kalır: `luaL_sandbox` ile kütüphaneler salt okunur, `os`/`io` yok, her modül kendi
   global tablosunda çalışır. Çok dosyalı kütüphane paylaşımı gerekirse M11 sonrası eklenir.
4. **Reflection çalışma anı tablosuna iner.** `TypeRegistry::Entry` artık alan tablosu (`FieldInfo`: ad, tür,
   ofset, enumerator'lar) taşır. Script'ler component alanlarını bu tabloyla doğrudan bellekten okur/yazar
   (`world.get_raw`), yazma değişiklik takibini işaretler. Entry'nin işlevleri `std::function` oldu (dinamik
   component'ler kapanış taşır).
5. **Dinamik component'ler düz veridir.** `astral.define_component` şemadan 4 baytlık kelimelerden oluşan bir
   blob (en çok 256 bayt) tanımlar: Bool, Int, UInt, Float, Vec2/3/4, Quat, Entity. String/iç içe yapı yok (taşıma
   `memcpy`; ECS'in ham erişimi bunu yeter). Bir component'in alanları oyun çalışırken değişemez (aynı şema
   tekrar tanımlanabilir — hot reload —, farklısı hata). Düzen alan adına göre sıralanır (deterministik).
6. **Olaylar adla taşınır.** `EventRegistry`: yansıtılmış C++ olayları (`add<T>`) ve yalnız script'in bildiği
   olaylar (`add_dynamic`) aynı arayüzle gönderilir/okunur; yük JSON'dur (Vec3 → vektör, Entity → entity'ye
   alan tablosuyla çevrilir).
7. **Script'ler sistemlerden sonra çalışır.** Aynı fazda önce C++ sistemleri, sonra script'ler (modül yükleme
   sırasıyla; modül içinde önce dinleyiciler, sonra sistemler). Davranışlar (`astral.behaviour`) `Script`
   component'iyle entity'ye bağlanır. Durum hiçbir zaman script'te tutulmaz.
8. **Hata sınırı:** her geri çağırma korumalı; hata `dosya:satır` ile loglanır, o modül durur (diğerleri sürer);
   dosya düzelince yeniden yüklenir. Zaman bütçesi (varsayılan 100 ms/faz) sonsuz döngüyü keser.
   Hot-reload: dosya içeriği 0.5 sn'de bir karşılaştırılır; bozuk sürüm eskisini bırakır.
9. **Özellik paketi manifesti asset olarak taşınır** (`assets/features/<ad>/feature.json`, C++ kaynakları
   `features/<ad>/`). Böylece manifest, script ve eylem dosyaları cook/`.apak` ile gelir. `install()` manifestte
   olmayan component/olay kaydederse hata verir (manifest = özelliğin API'si). Sınır denetimi `astral-features
   check` aracıdır (CTest): başka özelliğin başlığı yalnız `depends_on` + `exports.headers` ile, iç motor
   başlığı (`/detail/`, `/src/`) yok, değişebilir `static` global yok (`// astral-allow-global: neden` ile istisna).
10. **API belgesi** `generate_script_api` ile reflection'dan üretilir (`astral.d.luau`, `API.md`);
    `ctx.write_script_api(klasör)`. Component/olayın sahibi özellik API.md'de görünür.

## ADR-0017/0023'ten sapmalar ve ertelenenler

- `require("@astral")` yerine global `astral` (madde 3).
- Girdi eylemleri yalnız klavye + fare (Platform'da gamepad yok).
- Zamanlayıcı var; **tween, durum makinesi yardımcısı, tetik hacimleri, kayıt sistemi** M11'de yok
  (kayıt için `save_world` var; tetik hacmi fizik sensörleri M12'de).
- Luau bytecode cook'u ve luau-lsp/analiz entegrasyonu yok; cook script'leri düz dosya olarak paketler.
- Reflection'lı iç içe yapılar (ör. `PrimitiveMesh::material`) script'ten görünmez (`Other`): ihtiyaç
  olursa noktalı yol erişimi eklenir.
- Pong'da skor ışıkları (`PrimitiveMesh.material` yazar) bu yüzden C++ özelliği (`pong_scorelights`) olarak kaldı.
