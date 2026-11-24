# VFX Lab — M14 atmosfer başlangıcı

Avlu ve koridor, atmosfer ile ileride eklenecek hacim efektleri için sabit kamera test sahnesidir. `Atmosphere` component'i sahnenin güneş yönünü, doğrudan güneş ışığının renk/geçirgenliğini ve ortam gökyüzü rengini aynı saatten üretir. `atmosphere.ashader`, küresel atmosfer kabuğunda Rayleigh ve Mie tek saçılmasını örnekler. Ortam rengi renderer'ın mevcut yaklaşık aydınlatmasına girer. Gökyüzü LUT'u, tam sky IBL ve aerial perspective bu örneğin sonraki işleridir.

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

Etki HDR aşamasında en yakın mesh veya SDF yüzey derinliğine uygulanır. Saydam yüzeyler kendi konumlarından geçirgenliği hesaplar. Additive yüzeyler uzaklıkla sönümlenir. `--path-tracer` ana görüş segmentine aynı HDR yaklaşımı uygular. Işın sekmeleri atmosferin içinde simüle edilmez. Froxel sis ve yerel duman hacimleri sonraki iş paketidir.

## Froxel sis

`--fog 0.035` sahnede metre başına homojen sis sönümü açar. Sahne dosyalarında `environment.fog_extinction` ile `fog_range_m` kullanılabilir. Sıfır sönüm, iki sis pass’ini devre dışı bırakır.

```sh
build/debug/packages/vfx_lab/vfx_lab --raster --hour 17 --fog 0.035 --capture fog-raster.ppm --profile
build/debug/packages/vfx_lab/vfx_lab --hour 17 --fog 0.035 --capture fog-rt.ppm --profile
```

Kamera hacmi 16 derinlik dilimidir. Compute pass, birikimli Beer–Lambert geçirgenliği ve saçılmayı üretir. Raster ve hibrit RT, güneş için gölge haritasını kullanır. HDR kompozisyon en yakın mesh/SDF derinliğini, forward yüzeyler kendi derinliğini örnekler. Sis atlası her kare yeniden oluşturulur. Hibrit RT’de sis TAA’dan sonra birleştirilir. Bu sürümde ortam ışığı sahnenin gökyüzü renginden gelir. Yerel yoğunluk hacimleri, DDGI’dan hacimsel örnekleme ve path tracer’da hacim simülasyonu sonraki işlerdir.

## Gameplay dumanı

`GameplaySmoke`, C++ oyun kodunun kök bir entity üzerinde `Transform` ile oluşturduğu çalışma zamanı component’idir. App, component’i her fizik adımında tamamlanmış son fizik sorgu görünümüyle ilerletir. `obstacle_layers` ile dumanı durduran collision katmanları seçilir. Yayılım altı komşuya gerçekleşir ve açık kenarlar bir sonraki yayılma turunda yeniden sorgulanır. Bir engel kapandığında mevcut yoğunluk korunur ve engel yeni yayılımı durdurur. Yoğunluk, ayarlanan sürenin ardından sıfıra iner.

```cpp
const auto cloud = ctx.world.create(astral::Transform{.position = centre}, astral::GameplaySmoke{});
const auto* smoke = ctx.world.get<const astral::GameplaySmoke>(cloud);
const float visible = smoke->transmittance(centre, eye, target);
```

Konumlar aynı yerel dünya koordinatlarında verilmelidir. `optical_depth` segmentin dolu hücrelerde katettiği mesafeyi entegre eder. Oyun, görüş eşiğini kendi kurallarında belirler. Uygulama kaba bir hücre modelidir ve hücreden küçük engeller için alt-hücre doluluğu hesaplamaz. Görsel GPU dumanı ve Luau bağlayıcısı sonraki çalışmadır.

```sh
cmake --build --preset debug --target astral_app_tests
build/debug/modules/app/astral_app_tests '--test-case=gameplay smoke*'
```

## Yerel duman ve VFX zaman çizelgesi

`--smoke` CPU duman ızgarasını froxel atlasına taşır. GPU, dolu hücrelerin içinden geçen görüş segmentini entegre eder. Global sis kapalıyken de çalışır. Bir karede en fazla 16 duman ızgarası yüklenir.

`--vfx`, `courtyard.vfx` paketli örneğini başlatır. Sürümlü JSON dosyası `smoke`, `particles`, `light`, `decal` ve `sound` track’leri destekler. Her track saniye cinsinden `start` ve `duration` kullanır. Partiküller GPU’da analitik olarak ilerler, spawn anındaki rüzgârı kullanır ve mesh/SDF derinliğiyle örtülür. Partikül bütçesi burst başına 1.024 ve kare başına 8.192’dir. C++’ta `play_vfx` ve `stop_vfx` kullanılır. Luau olayları sabit adımda işlenir:

```lua
astral.send("astral::PlayVfx", {asset = "courtyard.vfx", position = vector.create(0, 1, 0), tag = "courtyard"})
astral.send("astral::StopVfx", {tag = "courtyard"})
```

```sh
build/debug/packages/vfx_lab/vfx_lab --vfx --raster --hour 17 --frames 25 --width 640 --height 360 --capture vfx.ppm --profile
```

Bu sürümde yerel hacim mevcut froxel atlasına entegre edilir. Özel `volume`/`particle` shader derleme hattı, seyrek hacim atlası ve partiküllerin derinlikle fiziksel çarpışması ayrıca ele alınır.
