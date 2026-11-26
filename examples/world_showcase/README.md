# world_showcase

Motorun açık dünya ve arazi yeteneklerini tek vitrinde gösterir. `open_world` ve `terrain_lab` örneklerinin yerini alır (eskiler silinmedi).

Aynı anda **tek istasyon** yüklüdür; harita akışı, arazi akışı ve aydınlatma her istasyonda temiz bir başlangıçla çalışır.

| İstasyon (`--view`) | Gösterdiği | Karşıladığı eski örnek |
|---|---|---|
| `world` (varsayılan) | 4 × 4 km akan `.world` haritası (`main` 128 m / 384 m ve `landmarks` 512 m / 2000 m ızgaraları, `level_instance` sektörleri), 2 km kayan orijin (`OriginShifted`), `"night"` veri katmanlı sokak lambaları, `--night`, `--nudge`, `C` hücre çizgileri | `open_world` |
| `range` | 6 × 6 karolu (768 × 768 m) akan `.aterrain` dağ silsilesi (`render_radius_m = 280`, `collision_radius_m = 320`), `.ashader` yükseklik/eğim katmanları (kum, çimen, kaya, kar), hareketle dinamik karo ve fizik akışı | yeni + `terrain_lab` akışı |
| `hill` | 2 × 2 karolu (128 × 128 m) `.aterrain` tepe (`ground.ashader` metre cinsinden XZ UV), yakın 65 × 65 / uzak 33 × 33 dikişli LOD ve tam 65 × 65 Jolt çarpışma; `--far` ve `F` ile uzak açı | `terrain_lab` |

## Tuşlar

| Tuş | İş |
|---|---|
| `1` `2` `3` | İstasyon (`world` / `range` / `hill`) |
| `N` | Gündüz / gece (`day_night` özelliği: `"night"` harita katmanını, güneşi ve gökyüzünü değiştirir) |
| `C` | Açık harita hücre sınırları ve yükleme menzili (`ctx.debug_show.world_cells`) |
| `F` | `hill` istasyonunda yakın karışık LOD / uzak LOD kamerası arasında geç |
| `W` `A` `S` `D`, `Q` `E`, `Shift` | Uç, in/çık, hızlan (`fly_camera` ortak özelliği) |
| Sağ fare | Bak |
| `F1` / `Esc` | Hata ayıklama arayüzü / çıkış |

## Çalıştırma

```sh
cmake --build --preset debug --target world_showcase world_showcase_package
build/debug/examples/world_showcase/world_showcase
build/debug/examples/world_showcase/world_showcase --capture world.ppm --view world --width 960 --height 540 --frames 90
build/debug/examples/world_showcase/world_showcase --capture range.ppm --view range --width 960 --height 540 --frames 90
build/debug/examples/world_showcase/world_showcase --capture hill.ppm --view hill --width 960 --height 540 --frames 90
```

Seçenekler: `--view world|range|hill`, `--night`, `--nudge <metre>`, `--far`, `--raster`, `--cells`, `--frames N`, `--width W`, `--height H`. `--view` verilmeden `--far` yazılırsa `hill` açılır.

## Bilinen sınırlar

- **`.aterrain` yerleşik yüzey katmanı (splatmap) tutmaz:** Motorun `.aterrain` biçimi (`modules/assets/include/astral/assets/terrain.hpp`) yalnızca 16 bit yükseklik karoları içerir. `range` istasyonundaki kum, çimen, kaya ve kar katmanları `assets/terrain/range.ashader` içinde dünya yüksekliği (`WORLD_POSITION.y`) ve eğimden (`NORMAL.y`) üretilir.
- **`close_world()` kayan orijini sıfırlamaz:** `WorldBridge::close_world()` harita varlıklarını temizler ancak `FloatingOrigin` konumunu sıfırlamaz (`modules/app/src/services/world_bridge.cpp`). Bu nedenle `world` istasyonu kamera konumunu `ctx.maps().world_origin()` değerine göre hesaplar; `range`/`hill` istasyonlarında yerel `(0, 0, 0)` tabanlı arazi konumları 2 km eşiğinin altında kaldığı için yeni kayma tetiklenmez.
- **`world` haritası `level_instance` ile üretilir:** `open_world`'ün derleme zamanında ~4500 `.ent` dosyası üreten `tools/make_test_world.py` adımı yerine `make_world.py` 49 `.ent` dosyası (`level_instance` sektörleri + uzak kuleler -> ~4001 varlık) üretip depoda tutar; böylece `astral_add_game` gevşek dosyalarla ve `world_showcase.apak` paketiyle birebir aynı haritayı kullanır.
- **Işın izleme desteği olmayan GPU:** Oyun kendiliğinden `--raster` yoluna geçer.
- **Golden referansı henüz yok:** Eski `open_world` ve `terrain_lab` golden testlerine dokunulmadı.

## Test

`world_showcase_tests` başsız çalışır: `world` hücre akışı ve 2 km kayan orijin, `--night` ve `N` tuşuyla gece katmanı/lambalar, `--nudge` kararlılığı, `range` yarıçap tabanlı karo ve fizik akışı, `hill` yakın ve `--far` görünümü, `1/2/3` istasyon geçişlerinde artık kalmaması, `C`/`F` tuşları, komut satırı ayrıştırma, gevşek dosyalar ile `world_showcase.apak` paketinin hem akan haritada hem arazide aynı davranması.
