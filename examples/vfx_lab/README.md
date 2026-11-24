# VFX Lab — M14 atmosfer ve efekt sahnesi

Avlu ve koridor, atmosfer ve hacim efektleri için sabit kamera test sahnesidir. `Atmosphere` component'i sahnenin güneş yönünü, doğrudan güneş ışığının renk/geçirgenliğini ve ortam gökyüzü rengini aynı saatten üretir. `atmosphere.ashader`, küresel atmosfer kabuğunda Rayleigh ve Mie tek saçılmasını örnekler. Ortam rengi renderer'ın mevcut yaklaşık aydınlatmasına girer.

```sh
cmake --build --preset debug --target vfx_lab_package
build/debug/packages/vfx_lab/vfx_lab --raster --hour 7 --capture morning.ppm --profile
build/debug/packages/vfx_lab/vfx_lab --hour 17.5 --capture evening-rt.ppm --profile
build/debug/packages/vfx_lab/vfx_lab --baseline --raster --capture baseline.ppm --profile
build/debug/examples/vfx_lab/vfx_lab --hours-per-second 0.5
```

`--profile` son GPU pass örneğini loglar. `--baseline` atmosfer sürücüsünü devre dışı bırakır. Scene kaydı çalışma saatini ve atmosfer ayarlarını saklar. Güneş ve çevre değerlerinin yazılmış varsayılanları korunur. Aktif atmosfer, çalışma sırasında güneş yönünü, rengini, şiddetini ve ortam gökyüzü rengini yönetir. `sun_intensity` doğrudan ışığı ve güneş diskini etkiler. Örnek gökyüzünün saçılma katsayıları shader profilinde sabittir.

## Atmosfer perspektifi

Sahnenin `environment.aerial_extinction` alanı metre başına sönümü, `aerial_start_m` ise kamera önündeki temiz mesafeyi ayarlar. Varsayılan sönüm sıfırdır. `--no-aerial` aynı saati ve gökyüzünü koruyarak sadece uzaklık etkisini kapatır. Örnek uzak silüetleri, ön plan ve gökyüzüyle aynı karede kıyaslar.

Etki HDR aşamasında en yakın mesh veya SDF yüzey derinliğine uygulanır. Saydam yüzeyler kendi konumlarından geçirgenliği hesaplar. Additive yüzeyler uzaklıkla sönümlenir. `--path-tracer` ana görüş segmentine aynı HDR yaklaşımı uygular.

## Froxel sis

`--fog 0.035` sahnede metre başına homojen sis sönümü açar. Sahne dosyalarında `environment.fog_extinction` ile `fog_range_m` kullanılabilir. Sıfır sönüm, iki sis pass’ini devre dışı bırakır.

```sh
build/debug/packages/vfx_lab/vfx_lab --raster --hour 17 --fog 0.035 --capture fog-raster.ppm --profile
build/debug/packages/vfx_lab/vfx_lab --hour 17 --fog 0.035 --capture fog-rt.ppm --profile
```

Kamera hacmi 16 derinlik dilimidir. Compute pass, birikimli Beer–Lambert geçirgenliği ve saçılmayı üretir. Raster ve hibrit RT, güneş için gölge haritasını kullanır. HDR kompozisyon en yakın mesh/SDF derinliğini, forward yüzeyler kendi derinliğini örnekler. Sis atlası her kare yeniden oluşturulur. Hibrit RT’de sis TAA’dan sonra birleştirilir. Hacimsel ortam ışığı sahnenin gökyüzü renginden gelir.

## Gameplay dumanı

`GameplaySmoke`, C++ oyun kodunun kök bir entity üzerinde `Transform` ile oluşturduğu çalışma zamanı component’idir. App, component’i her fizik adımında tamamlanmış son fizik sorgu görünümüyle ilerletir. `obstacle_layers` ile dumanı durduran collision katmanları seçilir. Yayılım altı komşuya gerçekleşir ve açık kenarlar bir sonraki yayılma turunda yeniden sorgulanır. Bir engel kapandığında mevcut yoğunluk korunur ve engel yeni yayılımı durdurur. Yoğunluk, ayarlanan sürenin ardından sıfıra iner.

```cpp
const auto cloud = ctx.world.create(astral::Transform{.position = centre}, astral::GameplaySmoke{});
const auto* smoke = ctx.world.get<const astral::GameplaySmoke>(cloud);
const float visible = smoke->transmittance(centre, eye, target);
```

Konumlar aynı yerel dünya koordinatlarında verilmelidir. `optical_depth` segmentin dolu hücrelerde katettiği mesafeyi entegre eder. Oyun, görüş eşiğini kendi kurallarında belirler. En küçük doluluk birimi bir hücredir.

C++ ve Luau, bir kapsüldeki dolu hücreleri belirli sayıda fizik adımı için açabilir. Kapsülün iki ucu aynı noktadaysa etki küreseldir. Görüş sorgusu ve GPU dumanı aynı boşluğu görür.

```lua
astral.send("astral::DisperseSmoke", {
    from = vector.create(-1, 1, 0), to = vector.create(1, 1, 0),
    radius_m = 0.3, hold_steps = 12
})
```

```sh
cmake --build --preset debug --target astral_app_tests
build/debug/modules/app/astral_app_tests '--test-case=gameplay smoke*'
```

## Yerel duman ve VFX zaman çizelgesi

`--smoke` CPU duman ızgarasını froxel atlasına taşır. GPU, dolu hücrelerin içinden geçen görüş segmentini entegre eder. Global sis kapalıyken de çalışır. Bir karede en fazla 16 duman ızgarası yüklenir.

`--vfx`, `courtyard.vfx` paketli örneğini başlatır. Sürümlü JSON dosyası `smoke`, `volume`, `particles`, `light`, `decal` ve `sound` track’leri destekler. Her track saniye cinsinden `start` ve `duration` kullanır. Partiküller GPU’da analitik olarak ilerler, spawn anındaki rüzgârı kullanır ve mesh/SDF derinliğiyle örtülür. Partikül bütçesi burst başına 1.024 ve kare başına 8.192’dir. C++’ta `play_vfx` ve `stop_vfx` kullanılır. Luau olayları sabit adımda işlenir:

```lua
astral.send("astral::PlayVfx", {asset = "courtyard.vfx", position = vector.create(0, 1, 0), tag = "courtyard"})
astral.send("astral::StopVfx", {tag = "courtyard"})
```

```sh
build/debug/packages/vfx_lab/vfx_lab --vfx --raster --hour 17 --frames 25 --width 640 --height 360 --capture vfx.ppm --profile
```

Bir `particles` veya `smoke` track’ine `"material": "effect.material"` ekleyerek VFX görünümünü ayrı bir varlık olarak düzenleyebilirsiniz. Yol, asset köküne göredir. `.material` dosyası bir `.ashader` seçer ve uniform değerlerini ayarlar. Cooker bu shader’ları da doğrular.

| Shader | Girdiler | Yazılabilir değerler |
| --- | --- | --- |
| `particle()` | `UV`, `WORLD_POSITION`, `AGE` (burst zamanında saniye) | `COLOR`, `ALPHA` |
| `motion()` (isteğe bağlı) | `AGE` (parçacık yaşı), `LIFETIME`, `ORIGIN`, `WIND`, `RANDOM` | `WORLD_POSITION`, `SIZE` (metre) |
| `volume()` | `UVW`, `WORLD_POSITION`, `TIME` (saniye), `AGE` (görsel hacmin yaşı) | `TINT`, `DENSITY`, `EMISSION` |

Partiküller varsayılan olarak additive karışır. Alfa karışımı için `render_mode blend_mix;` kullanılır. Hacim shader’ı her hücre merkezinde GPU’da çalışır. `DENSITY` 0 ile 1 arasında görsel yoğunluk çarpanıdır, `TINT` saçılma rengini çarpar. Oyun sorguları CPU doluluk ızgarasını kullanır. Her iki shader türü de materyal uniform’larını ve `sampler2D` destekler.

`particles` track’inde `soft_distance_m` ayarlanırsa billboard, en yakın mesh veya SDF yüzeyine yaklaşırken opaklığı azalır. Varsayılan değer sıfırdır. Örtülme ve yumuşak geçiş görüş derinliğini kullanır.

`volume` track’i için hacim materyali zorunludur. `size`, 17³ hücreli küpün kenar uzunluğunu metre cinsinden, `strength` ise sönüm ve ışıma ölçeğini ayarlar. `flame.material` ile avluda örnek bir prosedürel alev oluşturulur. Bu track yalnızca renderer’a veri gönderir. Çevre geometrisini de aydınlatmak için zaman çizelgesine bir `light` track’i eklenebilir.

Seyrek GPU hacim atlası ve partikül fizik çarpışması M14’ün açık işleridir.
