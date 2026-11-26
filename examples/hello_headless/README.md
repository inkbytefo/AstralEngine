# Başsız örnek (`hello_headless`)

Pencere ve GPU olmadan motorun özellik paketi (`FeatureSet`), Luau kuralı, adlandırılmış girdi eylemleri (`.actions`) ve sabit zaman adımlı (`fixed_timestep = 1/60`) deterministik döngüsünü gösterir. Sunucular, araçlar ve başsız testler için en küçük şablondur.

## Çalıştırma

```sh
cmake --build --preset debug --target hello_headless hello_headless_package
build/debug/examples/hello_headless/hello_headless
build/debug/examples/hello_headless/hello_headless --frames 120
build/debug/examples/hello_headless/hello_headless --scenario
```

| Seçenek | Etki |
|---|---|
| `--frames N` | Çalıştırılacak kare sayısı (varsayılan: 180; `--scenario` ile 120) |
| `--scenario` | Yerleşik `AppDesc::input_script` senaryosunu koşturur (hızlanma, duraklatma, sürdürme, sıfırlama) |
| `--script-api-only <klasör>` | `astral.d.luau` ve `API.md` dosyalarını yazıp çıkar |

## Betikli girdi senaryosu (`tally.actions`)

| Eylem | Tuş | Etki |
|---|---|---|
| `boost` | `Space` | Basılı tutulduğu karelerde sayaç 2x ilerler (`boosted_frames` artar) |
| `pause` | `P` | Duraklatma durumunu (`paused`) açar/kapatır |
| `reset` | `R` | `frames` ve `seconds` alanlarını sıfırlar (`resets` artar) |

`hello_headless::demo_input_script()` 120 karelik deterministik bir tuş dizisi üretir (`Space` 30–45, `P` 60 ve 90, `R` 100). `AppDesc::input_script` penceresiz çalışmada her kare yeni girdi kenarı (`pressed`/`released`) üretir.

## Testler

`hello_headless_tests`: sabit 60 Hz 120 kare sayımı, `AppDesc::input_script` ile `boost`/`pause`/`reset` eylemlerinin doğrulanması ve `demo_input_script()` senaryosunun iki ayrı `App` örneğinde birebir aynı sonucu vermesi. Ayrıca `hello_headless_features_check` ve `hello_headless_script_types`.

## Bilinen sınırlar

- `AppDesc::input_script` yalnız klavye tuşlarını (`astral::Key`) destekler; fare ve gamepad eksenleri betiklenemez ([ADR-0046](../../docs/decisions/0046-headless-input-script.md)).
- GPU ve pencere açmadığı için `--capture` desteklemez.
