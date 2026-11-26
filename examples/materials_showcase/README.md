# materials_showcase

Motorun PBR materyal modellerini, `.material` iş akışını ve `.ashader` dilini üç istasyonda gösterir. `material_showcase`, `material_workflow` ve `shader_showcase` örneklerinin yerini alır (eskiler sonraki adıma kadar durur).

Aynı anda **tek istasyon** yüklüdür. Güneş, ortam, gökyüzü (`ctx.sky`), son işlem (`ctx.post`) ve GI hacmi globaldir; bu yüzden istasyon değişince önceki istasyonun tüm varlıkları, model parçaları ve global görsel ayarları temizlenir.

| İstasyon | Gösterdiği |
|---|---|
| `studio` | 7 M13 PBR gölgelendirme modeli (kaba altın, clearcoat, sheen, anizotropik bakır, Beer-Lambert cam, SSS, ıslak bazalt), `studio/shaders/car_paint.ashader` özel clearcoat sergisi, 2 zemin decal'ı (ıslak su birikintisi ve boya şeridi) ve path tracer geçişi |
| `workflow` | `.material` ebeveyn kalıtımı (`stone` → `stone_triplanar` → `stone_wet`), triplanar projeksiyon, aynı `door.glb` üzerinde `oak` slot ezmesi, maskeli yaprak ve şeffaf cam GLB'leri, `U` ile `uv_inspect` / texel yoğunluğu görünümü, gevşek varlıklardan canlı yeniden yükleme (hot reload) |
| `gallery` | Gece `.ashader` galerisi: rüzgârla salınan yaprak (vertex + `alpha_test`), cam (`blend_mix` + ebeveyn kalıtımı), lav, yanardöner alaşım, plazma kabuğu (vertex + `blend_add`), hologram, özel gökyüzü (`dusk_sky.ashader`) ve son işlem (`vignette.ashader`) |

## Tuşlar

| Tuş | İş |
|---|---|
| `1` `2` `3` | İstasyon (`studio`, `workflow`, `gallery`) |
| `P` | Path tracer açık/kapalı (ışın izleme destekli GPU'da) |
| `U` | `workflow` istasyonunda UV dama (`uv_inspect`) / texel yoğunluğu (`uv_density`) / kapalı döngüsü |
| `W` `A` `S` `D`, `Q` `E`, `Shift` | Uç, in/çık, hızlan |
| Sağ fare | Bak |
| `F1` | Debug arayüzü ve Materials paneli |
| `Esc` | Çıkış |

## Çalıştırma

```sh
cmake --build --preset debug --target materials_showcase materials_showcase_package
build/debug/examples/materials_showcase/materials_showcase
build/debug/examples/materials_showcase/materials_showcase --capture studio.ppm --view studio --width 960 --height 540 --frames 90
```

Seçenekler: `--view studio|workflow|gallery`, `--frames N`, `--width W`, `--height H`, `--path-tracer`, `--raster`, `--uv-inspect`, `--uv-density`.

## Bilinen sınırlar

- **`blend_mix` ve `blend_add` yüzeyleri ışın izleme ve path tracer'da görünmez:** şeffaf özel shader'lar ileri (forward) geçişte çizilir ve TLAS'a girmez; bu yüzden yansımalarda ve GI'da görünmezler (`gallery`'deki `glass`, `hologram`, `plasma_shell`).
- **Decal'lar path tracer'da yalnız ilk vuruşta (first hit) uygulanır:** ikincil yansıma ve GI ışınları zemin decal'larını görmez (`studio`'daki ıslak şerit ve boya bandı).
- **Post shader'larının derinlik (depth) erişimi yoktur:** `shader_type post` yalnız tonemap sonrası renk tamponunu (`SCREEN_TEXTURE`) ve ekran koordinatını (`SCREEN_UV`) okur; derinliğe bağlı sis, DoF veya kenar algılama yazılamaz (`gallery`'deki `vignette.ashader`).
- **Özel `.ashader` yüzeylerinin RT yolu sınırlıdır:** `vertex()`, `NORMAL_MAP` ve `dFdx`/`dFdy` ekran türevleri RT hesaplama geçişinde çalışmaz; `uv_density` texel yoğunluğu renklendirmesi birincil G-buffer/raster geçişinde geçerlidir.
- **`--raster`:** `RectLight` alan ışığı yalnız ışın izlemede çizilir (`studio`'da raster için yedek bir nokta ışık eklenir); yansımalar, GI ve yumuşak alan gölgeleri yoktur. Işın izleme desteklemeyen GPU'da oyun kendiliğinden raster yola geçer ve `P` çalışmaz.
- **Hot reload yalnız gevşek dosyalarda çalışır:** `.apak` paketinden çalışan sürümde dosya izleme kapalıdır. F1 → Materials panelindeki değişiklikler yalnız çalışma anında geçerlidir, diske yazılmaz.
- Ham PNG ile cook edilmiş BC dokular arasında hafif renk/sıkıştırma farkı olabilir.

## Test

`materials_showcase_tests` başsız çalışır: üç istasyonun varlıkları ve ayarları, `P` ve `U` tuşları, istasyon geçişinde varlık/model parçası/`ctx.sky`/`ctx.post` artığı kalmaması, tüm `.ashader` dosyalarının (RT varyantları dahil) derlenmesi, tüm `.material` dosyalarının ebeveyn kalıtımıyla çözülmesi ve gevşek varlık ile `.apak` paketinin aynı davranması.
