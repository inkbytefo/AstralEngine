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
