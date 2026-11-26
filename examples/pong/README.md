# 3D Pong (`pong`)

Motorun özellik paketi (`FeatureSet`) mimarisiyle yazılmış en küçük tam oyunu: kurallar Luau'da (`pong_rules`), skor ışıkları (`pong_scorelights`) ve 3D ses (`pong_audio`) iki küçük C++ özelliğinde. Sahne ve prefab'lar `assets/court.scene` ile `assets/prefabs/` altındadır.

## Çalıştırma

```sh
cmake --build --preset debug --target pong pong_package
build/debug/examples/pong/pong
build/debug/examples/pong/pong --demo
build/debug/examples/pong/pong --capture out.ppm --frames 90 --width 320 --height 180
```

| Seçenek | Etki |
|---|---|
| `--demo` | Yapay zekâ her iki raketi de oynar |
| `--save-dir <klasör>` | Kayıt yuvalarının yazılacağı klasör (varsayılan: `saves`) |
| `--capture <dosya.ppm>` | Offscreen, sabit 60 Hz adımda yapay zekâ maçından tek kare |
| `--script-api-only <klasör>` | `astral.d.luau` ve `API.md` dosyalarını yazıp çıkar |

## Kontroller (`pong.actions`)

| Eylem | Klavye / Fare | Gamepad | İş |
|---|---|---|---|
| `move` | `WASD` / oklar | Sol çubuk, D-pad | Oyuncu raketini hareket ettir |
| `aim` | — | Sağ çubuk | Oyuncu raketini hareket ettir |
| `look` | Fare hareketi | — | Raketi doğrudan kaydır |
| `save_match` | `F5` | `LeftShoulder` (LB) | Maç durumunu `match` yuvasına kaydet (`astral.save`) |
| `load_match` | `F9` | `RightShoulder` (RB) | `match` yuvasını geri yükle (`astral.load`) |
| — | `F1` / `Esc` | — | Debug arayüzü / çıkış |

## Kayıt ve yükleme

Maç anlık görüntüsü (`pong::MatchSave` — skor, servis ve RNG durumları, top konumu/hızı, her iki raketin konumu ve yapay zekâ hedefi) kendi hafif entity'sinde tutulur ve `assets/features/pong_rules/save.luau` içinden motorun kayıt yuvasıyla (`astral.save("match", {"pong::MatchSave"})` / `astral.load("match")`) diske yazılır. Böylece `astral.load` sahnedeki ışık, mesh, ses ve raket çocuk hiyerarşilerine dokunmadan yalnız anlık görüntüyü değiştirir.

## Testler

`pong_tests` başsız, sabit 60 Hz adımla çalışır: sahne yükleme, yapay zekâ maçı, çarpışma/sekme/skor ışıkları, ses olayları, `match` kayıt yuvasına kaydetme/yükleme (aynı oturum ve yeni `App` oturumu, boş yuva güvenliği) ve gamepad eylemleri (`left` stick + deadzone, D-pad, `right` stick, omuz tuşları). Ayrıca `pong_features_check` ve `pong_script_types`.

## Bilinen sınırlar

- Tek bir kayıt yuvası (`match`) kullanılır; çoklu yuva seçimi arayüzü yoktur.
- `pong_tests` içindeki 4 dakikalık yapay zekâ maçı, ağır paralel yük altında Luau'nun 100 ms duvar saati bütçesi nedeniyle zaman zaman kırılgan olabilir (seri çalıştırmada geçer).
- `AppDesc::input_script` yalnız klavye tuşlarını betikler; gamepad testi `astral::Input` üzerinden doğrudan `Context::actions.update(...)` besleyerek eylem katmanını ve Luau kurallarını doğrular.
