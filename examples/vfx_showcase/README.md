# VFX vitrini (`vfx_showcase`)

Motorun hacim, atmosfer, parçacık, duman ve ses efektlerini dört istasyonda gösterir; VFX zaman çizelgeleri ve sesler Luau kuralı (`vfx_director`) ile sürülür. `vfx_lab` örneğinin yerini alır (eski örnek E5'e kadar yerinde kalır).

Aynı anda **tek istasyon** yüklüdür. Güneş, atmosfer, froxel sis ve DDGI hacmi global olduğu için her istasyon bütün sahnenin sahibidir.

| İstasyon | Gösterdiği |
|---|---|
| `courtyard` | Luau'dan tetiklenen tam `.vfx` zaman çizelgesi: `smoke`, `volume` (alev), `particles` (kıvılcım), `light`, `decal` ve `sound` track'leri; avlu duvar çarpıştırıcıları, rüzgâr ve döngülü ortam sesi |
| `particles` | İki Luau feneri: rüzgârda (`WindZone`) savrulan `blend_add` kıvılcımlar (derinlik sekmesi, yumuşak geçiş) ve `blend_mix` köz parçacıkları + prosedürel `flame.material` hacim alevi + ses |
| `smoke` | Küresel froxel sis kapalıyken çalışan yerel `GameplaySmoke`, fizik duvarlarına takılan yayılım ve `C` tuşuyla Luau üzerinden `astral::DisperseSmoke` kapsül dağıtma + uzamsal `AudioSource` sesi |
| `atmosphere` | `atmosphere.ashader` ile Rayleigh/Mie gökyüzü, gökyüzü IBL'i, uzak silüetlerde hava perspektifi ve küresel froxel sis (hacim öz gölgeleri + DDGI) |

## Tuşlar

| Tuş | İş |
|---|---|
| `1` `2` `3` `4` | İstasyon (`courtyard`, `particles`, `smoke`, `atmosphere`) |
| `Space` | Aktif fenerlerden Luau ile anında `astral::PlayVfx` + uzamsal tek seferlik `AudioSource` tetikle |
| `X` | Etiketli aktif VFX'leri Luau üzerinden `astral::StopVfx` ile durdur |
| `C` | Koridordaki `GameplaySmoke` bulutunu Luau üzerinden `astral::DisperseSmoke` ile dağıt ve rüzgâr sesi çal |
| `T` | Gün döngüsü saatini başlat / durdur (`Atmosphere.hours_per_second`) |
| `F` / `G` / `I` | Sis öz gölgeleri (`fog_self_shadows`), sis DDGI (`fog_ddgi`) ve gökyüzü IBL'i (`sky_ibl`) aç/kapat |
| `W` `A` `S` `D`, `Q` `E`, `Shift` | Uç, in/çık, hızlan (`fly_camera`) |
| Sağ fare | Bak |

## Çalıştırma

```sh
cmake --build --preset debug --target vfx_showcase vfx_showcase_package
build/debug/examples/vfx_showcase/vfx_showcase
build/debug/examples/vfx_showcase/vfx_showcase --capture build/captures/courtyard.ppm --view courtyard --width 960 --height 540 --frames 90
build/debug/examples/vfx_showcase/vfx_showcase --capture build/captures/courtyard-raster.ppm --view courtyard --raster --width 960 --height 540 --frames 90
```

Seçenekler: `--view courtyard|particles|smoke|atmosphere`, `--frames N`, `--width W`, `--height H`, `--raster`, `--path-tracer`, `--hour H`, `--hours-per-second R`, `--fog F`, `--no-aerial`, `--baseline`, `--no-volume-shadows`, `--no-fog-ddgi`, `--no-sky-ibl`, `--profile`.

## Düzen

- `src/stations/`: dört istasyonun kurulumu (`courtyard.cpp`, `particles.cpp`, `smoke.cpp`, `atmosphere.cpp`).
- `src/features/vfx_director/` ve `assets/features/vfx_director/`: `Beacon`, `SmokeDisperser`, `SoundOneShot` component'leri, `.actions` girdi eşlemesi ve `--!strict` `vfx_director.luau`.
- `src/features/fly_camera/`: `features/fly_camera` ortak özelliğinin kopyası (`tools/sync_features.py` ile eşlenir).
- `assets/vfx/`, `assets/materials/`, `assets/shaders/`, `assets/sounds/`: `.vfx` zaman çizelgeleri, `.material`/`.ashader` dosyaları ve 4 prosedürel WAV ses klibi.

## Testler

`vfx_showcase_tests` başsız (60 Hz sabit adım, sessiz ses mikseri oyun saatiyle ilerler) çalışır: dört istasyonun kurulumu, çarpıştırıcılar, Luau `PlayVfx`/`StopVfx`/`DisperseSmoke` akışı, duman geçirgenliğinin kapsül boyunca açılıp geri kapanması, başsız ses voice sayımı ve tek seferlik `AudioSource` temizliği, CLI seçenekleri ve tuş anahtarları, `1..4` geçişinde artık kalmaması, gevşek varlık ↔ `.apak` paketi eşitliği. Ayrıca `vfx_showcase_features_check` ve `vfx_showcase_script_types`.

## Bilinen sınırlar

- **Luau'da doğrudan `astral.audio.*` API'si yoktur** ([audio.md](../../docs/modules/audio.md), [script.md](../../docs/modules/script.md)). Luau sesleri ya `.vfx` zaman çizelgesindeki `"sound"` track'i (`astral::PlayVfx`) ya da `astral::AudioSource` component'i taşıyan entity'ler yaratarak sürer; bus/ana ses düzeyi, doğrudan fire-and-forget voice çalma veya ses örneği sorgusu Luau'dan yapılamaz.
- **Luau `GameplaySmoke` doluluğunu/geçirgenliğini doğrudan okuyamaz:** `GameplaySmoke` çalışma zamanı C++ component'idir; Luau dumanı yalnız `astral::PlayVfx` ve `astral::DisperseSmoke` olaylarıyla yönetir.
- **Hacimler arası gölge ve path tracer'da hacim ışığı yoktur** (motor sınırı; `vfx_lab` ile aynı).
- **Işın izleme desteklemeyen GPU'da** oyun kendiliğinden raster yola geçer; DDGI sis aydınlatması (`fog_ddgi`) yalnız hibrit RT yolunda etkindir.
- **Golden görüntü referansı henüz yok** (examples yenilemesi E5, CI).
