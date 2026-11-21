# ADR-0014: Asset sistemi — AssetId, iki katmanlı yükleme, yerinde hot-reload

**Durum:** Kabul · 2026-11-21 · Bağlam: [ADR-0012](0012-render-api-contract.md)

**Karar:**
1. **AssetId = normalize yolun kalıcı hash'i.** Ayrı bir GUID veritabanı yok; yol zaten kimlik. Cook (M6)
   aynı kimliği paket içindekiler tablosunda kullanır. Bedeli: dosyayı yeniden adlandırmak kimliği değiştirir.
2. **Saf CPU yükleyici + yönetici.** `load_gltf` yan etkisiz ve thread-safe (test edilebilir, worker'da
   koşar); `AssetManager` yalnız ana thread'de renderer'a dokunur (renderer thread-safe değil).
3. **glTF kendi kodumuz, görüntü çözme `stb_image`.** glTF'in JSON'u zaten `astral::Json` ile okunuyor;
   ayrı bir glTF kütüphanesi bağımlılık ekler, kazanç küçük. PNG/JPEG çözmek ise ayrı bir uzmanlık:
   `stb_image` (public domain), sabit commit, yalnız bu modülde özel bağımlılık.
4. **Hot-reload handle'ları korur** (`update_mesh`/`update_texture`/`update_material`): entity'ler aynı
   handle'ı tutmaya devam eder, yeniden oluşturma yalnız yapı değişince (`Model::version`).
5. **Sahnede `ModelRef{path}`**: serileşebilir; üretilen parça entity'leri `TypeRegistry::exclude` ile
   kaydedilmez, yüklemede yeniden üretilir — kayıt dosyası modelin kopyasını taşımaz.
6. **Normal map'ler tangent'sız** (türevlerden TBN): her glTF'te tangent yok, üretmek (MikkTSpace) ayrı
   bir iş; sonuç gözle ayırt edilmez. RT vuruşlarında normal map yok (türev yok, bulanık ışık).

**Sonuçlar:** M5c sahne dosyaları asset'lere yol/AssetId ile referans verir; M6 cook aynı yükleyiciyi
çıktı üretmek için kullanır.
