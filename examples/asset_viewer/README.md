# Varlık Görüntüleyici (`asset_viewer`)

glTF/GLB modelleri ve `.scene` sahne dosyaları için tek görüntüleyici. Eski `model_viewer` ve `scene_viewer` örneklerinin yerini alır (eskiler examples yenilemesinin son dalgasına kadar yerinde durur).

Komut satırından açılacak dosyayı veya yerleşik istasyonlardan birini seçer; yörünge/uçuş kamerası, canlı model/sahne istatistik HUD'ı, canlı yeniden yükleme (hot-reload) ve cook/paket (`.apak`) hattını bir arada gösterir.

## İstasyonlar ve dosya açma

| Mod / İstasyon | Gösterdiği |
|---|---|
| `scene` (varsayılan) | `showcase.scene`: RGB oda prefab'ı, iç içe prefab'lar (`cornell_room`, `mirror_pedestal`, `box`), SDF şekilleri ve `props/crate.glb` |
| `model` | `props/crate.glb`: ışıklı stüdyo zemini, ana alan ışığı (`RectLight`), güneş ve GI hacmi içinde glTF/GLB modeli |
| `<dosya.glb\|gltf>` | Verilen harici glTF/GLB dosyasını kendi klasörünü varlık kökü yaparak stüdyoda açar; yeniden dışa aktarınca güncellenir |
| `<dosya.scene>` | Verilen harici `.scene` dosyasını kendi klasörünü varlık kökü yaparak açar; dosya veya prefab değişince canlı yenilenir |

## Tuşlar

| Tuş | İş |
|---|---|
| `1` `2` | Yerleşik istasyonlar arası geçiş: `1` sahne, `2` model (harici dosya açıldığında kapalı) |
| `C` | Kamera kipi geçişi: **Yörünge (Orbit)** ↔ **Uçuş (Fly)** (konum ve bakış korunur) |
| `R` | Kamerayı başlangıç konumuna sıfırla |
| `G` | Debug çizimlerini aç/kapat (ışık menzilleri, fizik şekilleri, XZ ızgarası, varlık eksenleri) |
| `F1` | Motorun debug arayüzünü aç/kapat (`--ui` ile açık başlar) |
| Sol fare sürükle / Ok tuşları | Yörünge kipinde hedef etrafında dön |
| Fare tekerleği / `+` `-` / `PageUp` `PageDown` | Yörünge kipinde yakınlaş/uzaklaş |
| `W` `A` `S` `D`, `Q` `E`, `Shift`, Sağ fare | Uçuş kipinde hareket ve bakış (`fly_camera`) |
| `Esc` | Çıkış |

## Çalıştırma

```sh
cmake --build --preset debug --target asset_viewer asset_viewer_package
build/debug/examples/asset_viewer/asset_viewer
build/debug/examples/asset_viewer/asset_viewer --view model
build/debug/examples/asset_viewer/asset_viewer path/to/model.glb
build/debug/examples/asset_viewer/asset_viewer path/to/level.scene --debug --ui
build/debug/examples/asset_viewer/asset_viewer --capture out.ppm --view scene --width 960 --height 540 --frames 90
build/debug/packages/asset_viewer/asset_viewer --view model
```

Seçenekler: `--view scene|model`, `--camera orbit|fly`, `--debug`, `--ui`, `--raster`, `--pack <dosya.apak>`, `--frames N`, `--width W`, `--height H`, `--capture <out.ppm>`.

## Cook ve paket hattı

`asset_viewer_package` hedefi `astral-cook` ile `assets/` altındaki sahneyi, prefab'ları, `props/crate.glb` modelini (BC7/BC5 mip zincirli `AMDL`) ve Luau script'lerini `build/<preset>/packages/asset_viewer/asset_viewer.apak` içine paketler. Paketlenmiş çalıştırılabilir dosya gevşek `assets/` olmadan `.apak` üzerinden açılır ve HUD'da `Kaynak: paket (.apak)` olarak raporlar.

## Özellik mimarisi (`FeatureSet`)

- `fly_camera`: ortak özellik (`features/fly_camera/`, `tools/sync_features.py` ile eşlenir).
- `orbit_camera`: `orbit::OrbitCamera` bileşeni ve Luau yörünge kuralı (`assets/features/orbit_camera/orbit_camera.luau`) + fare tekerleği için C++ `orbit_camera_scroll` sistemi.

## Testler

`asset_viewer_tests` başsız (`headless`) ve sabit 60 Hz adımla çalışır:
- Komut satırı çözümlemesi (yerleşik istasyonlar, harici `.glb`/`.gltf`/`.scene` dosyaları, bayraklar).
- Sahne istasyonu (prefab örnekleri, SDF'ler, `ModelRef`, canlı istatistikler).
- Model istasyonu (`model_viewer` stüdyo doğrulaması ve model istatistikleri).
- `--debug`, `--ui`, `--raster` bayrakları.
- `1`/`2` ile artık bırakmadan istasyon geçişi.
- `AppDesc::input_script` ile yörünge döndürme, yakınlaştırma, `R` sıfırlama, `C` ile uçuş kipine geçiş, `W` uçuşu ve `G` debug aç/kapa.
- Harici `.glb` yükleme (yedek özellik yüklemesiyle çalışan kamera).
- Diskte değişen `.scene` dosyasının canlı yeniden yüklenmesi.
- Gevşek varlıklar ↔ `asset_viewer.apak` eşitliği ve `cook_directory` + `write_pack` gidiş-dönüş testi.

Ayrıca `asset_viewer_features_check` ve `asset_viewer_script_types` (`--!strict` Luau tip denetimi) koşar.

## Bilinen sınırlar

- **glTF iskelet ve animasyon desteklenmez:** Motorun glTF yükleyicisi `skin`, `animations`, morph target ve sparse accessor alanlarını uyarı verip atlar; modeller statik pozda gösterilir (bkz. [assets.md](../../docs/modules/assets.md)).
- **`InputActions` fare tekerleği ekseni sunmaz:** Yörünge yakınlaştırmasında `+`/`-` ve `PageUp`/`PageDown` Luau aksiyonu olarak çalışırken, fare tekerleği C++ `orbit_camera_scroll` sisteminde `ctx.input.scroll().y` üzerinden okunur.
- **`AssetManager` tek kök destekler:** Harici bir dosya açıldığında `asset_root` o dosyanın klasörü olur; bu nedenle görüntüleyicinin kendi `fly_camera` ve `orbit_camera` script/aksiyon dosyaları harici kipte yerleşik varlık klasöründen veya `.apak` paketinden yedek yolla yüklenir.
- **İstatistik toplama için `ModelData` CPU tarafında okunur:** Çalışma anı `astral::Model` yapısı köşe/üçgen sayısını ve sınır kutusunu tutmadığı için `StatsTracker`, model yüklendiğinde veya yenilendiğinde `read_cooked_model`/`load_gltf` ile bir kez okuyup sonucu önbelleğe alır.
- **`--raster` kipi:** `RectLight` yalnızca ışın izleme yolunda çizilir; `--raster` verildiğinde oda ve stüdyoya yedek `PointLight` eklenir, ancak GI ve ışın izlemeli yansımalar yoktur.
- **Golden görüntü referansı henüz yok:** `tests/golden/CMakeLists.txt` bu adımda değiştirilmedi; golden referansları E5'te CI (lavapipe) üzerinde üretilecektir.
