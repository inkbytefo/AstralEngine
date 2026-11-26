# Fizik vitrini (`physics_showcase`)

Motorun fiziği tek bir dünyada. Ortada bir göl var, çevresindeki düz platoda dört istasyon duruyor. Bu örnek `physics_demo`,
`physics_lab` ve `physics_playground` örneklerinin yerini alır. Üçü examples yenilemesinin son dalgasına (E5) kadar yerinde kalır.

```sh
cmake --build --preset debug --target physics_showcase physics_showcase_package
build/debug/examples/physics_showcase/physics_showcase
build/debug/examples/physics_showcase/physics_showcase --capture out.ppm --frames 150 --width 1280 --height 720 --view joints
```

| Seçenek | Etki |
|---|---|
| `--view pile\|joints\|surfaces\|water` | Oyuncunun başladığı istasyon |
| `--raster` | Işın izlemeyi kapatır |
| `--overlays` | Fizik şekilleri, eklemler ve alanlar (rüzgâr, su kutuları) çizilir |

**Kontroller:** WASD yürü, Space zıpla, oklar dön, sol tık top at, sağ tık baktığını it, F uçan kamera (WASD/Q/E, sağ fare ile bak),
F1 debug, Esc çıkış. Gamepad: sol çubuk, A zıpla, sağ omuz at, sol omuz it, Y uçan kamera.

## İstasyonlar

| İstasyon | Nerede | Ne gösterir | Nerede yazılı |
|---|---|---|---|
| **Yığın** | güney | Kutu piramidi, top yağmuru, `Lifetime` ile kendiliğinden yok olan toplar, karakter kontrolcüsü, raycast ile itme | `src/stations/pile.cpp`, `src/features/{player,shooter,rain,lifetime}` |
| **Eklemler** | kuzey | Hedefi Luau'dan sürülen motorlu menteşe, SwingTwist konisi, SixDOF kızak, 250 N'da kırılan Fixed eklem, toplu raycast sensörleri, sahne dosyasından gelen karakter, sensör ve motorlu kızak | `src/stations/joints.cpp`, `assets/joints_station.scene`, `assets/features/lab/lab.luau` |
| **Yüzeyler** | doğu | Fizik materyal tablosu (buz/kauçuk/tutuş, birleştirme modları), cook edilmiş mesh zemin ve convex hull, aynı hızla atılan iki pak, Luau'nun `torque` ile döndürdüğü çark, kendi gövdesini yok sayıp (`ignore`) sensörü gören (`sensors = true`) tarayıcı ışını | `src/stations/surfaces.cpp`, `assets/features/showcase/showcase.luau` |
| **Su ve Rüzgâr** | orta | Yüzen kasalar ve batan taşlar (`Buoyant`), rüzgârda sürüklenen toplar (`AirDrag`), dönen ve esen rüzgâr (Luau), menteşeli köprü, zıplama pedi (sensör + `TriggerEntered`), planör (`Airfoil`) | `src/stations/water.cpp`, `assets/features/playground/playground.luau` |

Sağ üstteki satır o anki sayaçları gösterir: gövde sayısı, temas olayları, ped fırlatmaları, kırılan eklemler, sensör ışını isabeti, çark hızı.

## Düzen

- `src/stations/` dünyayı kurar; kural yoktur. `src/features/` component'leri ve C++ sistemlerini, `assets/features/` Luau kurallarını ve
  girdi eşlemesini tutar. `fly_camera` ortak özelliktir (`tools/sync_features.py`), kopyası elle düzenlenmez.
- Fizik materyal kimlikleri 11–13'tür: arazinin (1) ve göl oyuncaklarının (2) kimlikleriyle çakışmaz.

## Testler

`tests/game_test.cpp` (başsız, sabit 60 Hz adım): üç eski örneğin kontrolleri (yüzme/batma, köprü, rüzgâr, ped, motor, kırılma, materyaller,
toplu sorgu, paket ile gevşek varlık eşitliği, tekrar oynatma determinizmi, 120 gövde/60 eklem stres testi), yeni Luau çağrıları (çark ve tarayıcı)
ve `AppDesc::input_script` ile oyuncunun yürüdüğü, zıpladığı ve F ile uçuşa geçtiği bir senaryo. Ayrıca `physics_showcase_features_check`
ve `physics_showcase_script_types`.

## Bilinen sınırlar

- Golden görüntü testi yok; referans CI'da (lavapipe) üretilecek (examples yenilemesi E5).
- Görsel ayar sade: düz renkli materyaller, gökyüzü shader'ı ve post efekt yok. Bu örneğin konusu fizik.
- Determinizm aynı makinede, aynı derlemede test edilir; platformlar arası determinizm yoktur (bkz. [physics.md](../../docs/modules/physics.md)).
