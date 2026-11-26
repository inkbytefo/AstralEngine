# lighting_showcase

Motorun aydınlatmasını üç istasyonda gösterir. `rt_showcase` ve `sdf_scene` örneklerinin yerini alır (eskiler E5'e kadar durur).

Aynı anda **tek istasyon** yüklüdür. Hareketli SDF şekilleri hibrit ışın izlemenin zamansal geçmişini sıfırlar ([render](../../docs/modules/render.md)), güneş, ortam ve GI hacmi de globaldir; bu yüzden her istasyon bütün görüntünün sahibidir.

| İstasyon | Gösterdiği |
|---|---|
| `room` | Tek tavan paneli (alan ışığı) ile RGB oda: renk taşması, yumuşak gölge, krom küredeki yansıma, SDF torus ve eriyen SDF toplar |
| `yard` | SDF avlusu: yumuşak birleşim, oyma, dönen torus, güneş, Luau `orbits` kuralıyla dönen renkli ışıklar |
| `night` | 36 nokta ışık, küreler ve sütunlar: ReSTIR DI |

## Tuşlar

| Tuş | İş |
|---|---|
| `1` `2` `3` | İstasyon |
| `P` | Path tracer. Açıkken avludaki animasyon durur; yoksa birikim her karede sıfırlanır |
| `R` | ReSTIR DI açık/kapalı |
| `V` | Debug görünümü: kapalı, albedo, normal, yaygın ışık, speküler, hareket |
| `W` `A` `S` `D`, `Q` `E`, `Shift` | Uç, in/çık, hızlan |
| Sağ fare | Bak |

## Çalıştırma

```sh
cmake --build --preset debug --target lighting_showcase lighting_showcase_package
build/debug/examples/lighting_showcase/lighting_showcase
build/debug/examples/lighting_showcase/lighting_showcase --capture room.ppm --view room
```

Seçenekler: `--view room|yard|night`, `--frames N`, `--width W`, `--height H`, `--path-tracer`, `--raster`, `--debug-view 0..5`.

## Bilinen sınırlar

- **Path tracer'da SDF yüzeylerinde koyu halka ve nokta görünür** (`room` ve `yard`). 128 ve 400 karelik capture'larda da duruyor, yani gürültü değil. Motorun davranışıdır; nedeni incelenmedi.
- Path tracer açıkken `--debug-view 2` capture'ı normal görüntüyü verdi (yalnız bu mod denendi).
- `--raster`: alan ışığı yalnız ışın izlemede çizilir, bu yüzden `room`'a yedek bir nokta ışık eklenir. Yansıma olmadığı için metal yüzeyler neredeyse siyah kalır; GI ve yumuşak gölge yoktur.
- Işın izleme desteklemeyen GPU'da oyun kendiliğinden raster yola geçer ve `P` çalışmaz.
- Golden referansı henüz yok (E5, CI).

## Test

`lighting_showcase_tests` başsız çalışır: her istasyonun içeriği ve ayarları, istasyon geçişinde artık kalmaması, tuşlar, path tracer'da animasyonun durması, paketten açılış.
