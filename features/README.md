# Ortak özellikler

Birden çok oyunda kullanılan özellik paketlerinin **tek kaynağı**. Her klasör bir özelliktir:

```
features/<ad>/src/      →  <oyun>/src/features/<ad>/       (C++ tarafı)
features/<ad>/assets/   →  <oyun>/assets/features/<ad>/   (feature.json, Luau, .actions)
```

Bir oyun özelliği kullanmak için `src/features/<ad>/` klasörünü oluşturur ve eşitler:

```sh
python3 tools/sync_features.py          # features/ altındakileri kullanan her oyuna kopyalar
python3 tools/sync_features.py --check  # kopyalar farklıysa hata verir (CTest: shared_features_in_sync)
```

Özelliği değiştirirken **burada** düzenle, sonra eşitle; oyundaki kopyayı elle düzenleme (denetim yakalar). Neden kopya:
oyun kendi paketini taşır (tek klasör, tek `.apak`), bağımlılık yönetimi gerektirmez; denetim kopyaların kaymasını önler.

Özellikler: `fly_camera` (WASD, Q/E, Shift, sağ fare; `autopilot` ile görüntü testleri için sabit uçuş).

## Planlanan Game Framework paketleri

[Technology Labs roadmap](../docs/technology-labs-roadmap.md) kapsamında mevcut shared feature mekanizması, tekrar kullanılabilir isteğe bağlı gameplay paketleri için genişletilecek: kamera/locomotion, interaction, health/abilities, inventory/equipment, team/match, vehicle control ve UI widget'ları. Native simülasyon ve genel servisler motor modüllerinde, oyuna özgü kurallar oyun feature'larında kalır. Bu liste planlıdır; mevcut paket veya tamamlanmış API iddiası değildir. Source-of-truth ve sync check korunur.
