# Astral::Audio

Ses çalma: çözme (WAV, FLAC, MP3), mikser, kanallar, 3D konumlu ses. miniaudio üstünde; miniaudio tipleri
public header'da yok. ECS bilmez; sahneye bağlama App'te. Karar: [ADR-0020](../decisions/0020-audio.md).

```cpp
#include <astral/audio/audio.hpp>

auto audio = astral::AudioEngine::create({});          // varsayılan cihaz
// auto audio = AudioEngine::create({.null_device = true}); // sessiz mikser (test, sunucu)
astral::SoundHandle hit = audio->load(bytes, "hit.wav");   // dosya baytlarından; geçersizse invalid
astral::VoiceHandle v = audio->play(hit, {.bus = astral::AudioBus::Effects, .volume = 0.8f,
                                          .spatial = true, .position = {2, 0, -5}});
audio->set_listener(camera_pos, forward, up);
audio->set_bus_volume(astral::AudioBus::Music, 0.5f);
audio->stop(v, 0.2f);                                   // 0.2 s'de kısarak
audio->update();                                        // biten sesleri toplar (App her kare çağırır)
```

| Tür | Ne |
|---|---|
| `AudioDesc` | `null_device`, örnekleme hızı, kanal sayısı, en çok ses (voice), logger |
| `PlayDesc` | kanal, seviye, perde (pitch), döngü, 3D, konum, min/max mesafe, fade-in |
| `AudioBus` | `Effects`, `Music`, `Ambience`, `Interface` (reflect edilmiş enum) |
| `SoundInfo` | süre, kanal, örnekleme hızı |

## App'te

- `ctx.audio`: her zaman var. Pencereli uygulama gerçek cihazı açar; headless, offscreen ya da cihaz yoksa
  sessiz mikser (`AppDesc::audio = false` her zaman sessiz).
- `ctx.sound("sounds/hit.wav")`: asset yolundan bir kez yükler, önbellekte tutar (gevşek klasör veya `.apak`).
- `AudioSource` component'i (sahne dosyasına yazılır): `sound`, `bus`, `volume`, `pitch`, `loop`, `spatial`,
  `min_distance`, `max_distance`, `playing`. Entity'nin `WorldTransform`'unu izler; tek seferlik ses bitince
  `playing = false`; component silinince ses durur.
- `AudioListener`: bu entity'den duyulur; yoksa aktif kamera.

## Test

`astral_audio_tests`: bellekte üretilen sinüs WAV ile çözme, bozuk veri reddi, biten/duran/döngülü sesler,
kanal seviyesi, 3D pan ve mesafe, ses sınırı. Sessiz cihazda `read()` ile miks gerçekten ölçülür.
App: `audio_app_test.cpp`; Pong: maç boyunca sesler yükleniyor ve çalıyor.

## Bilinen sınırlar

- Streaming (uzun müzik dosyaları) API'de var (`load(..., stream=true)`) ama bellekteki bayttan okur.
- Efekt zinciri (reverb, filtre), ses tıkanması (occlusion), fizik materyali → çarpma sesi yok (M12 sonrası).
- Doppler kapalı.
