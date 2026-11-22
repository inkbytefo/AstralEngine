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
