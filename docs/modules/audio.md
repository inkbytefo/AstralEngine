# Audio (`Astral::Audio`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core (public); miniaudio 0.11.26 (modüle derlenir), Threads, `dl` (private) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

miniaudio üzerinde küçük bir ses motoru: bellekteki WAV/FLAC/MP3 verisini çözme, ses örnekleri ("voice") çalma
(2B veya 3B konumlu), dört mikser kanalı (bus), tek bir dinleyici ve ana ses seviyesi. Modül ECS bilmez; `App` onu
`AudioSource`/`AudioListener` component'lerine bağlar (`modules/app/src/services/audio_bridge.cpp`). Efekt zinciri (reverb,
filtre), ses tıkanması (occlusion), ham PCM yükleme, ses üretimi/kaydı, cihaz seçimi ve Luau API'si **yoktur**.

## Public API

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `AudioEngine::create(AudioDesc, error*)` | Motoru açar; cihaz açılamazsa ve `null_device` istenmediyse `nullptr` | `modules/audio/include/astral/audio/audio.hpp:63` |
| `AudioDesc` | `null_device`, `sample_rate` (48000), `channels` (2), `max_voices` (256), `logger` | `modules/audio/include/astral/audio/audio.hpp:34` |
| `load(bytes, name, stream)` | Kodlanmış klibi **kopyalar** ve çözer (WAV, FLAC, MP3); geçersizse boş handle | `modules/audio/include/astral/audio/audio.hpp:70` |
| `destroy(SoundHandle)` | O sese ait voice'ları durdurur ve siler | `modules/audio/include/astral/audio/audio.hpp:72` |
| `play(sound, PlayDesc)` | Voice başlatır: bus, volume, pitch, loop, spatial, position, min/max distance, fade_in | `modules/audio/include/astral/audio/audio.hpp:76`, `:42` |
| `stop(voice, fade_out)`, `playing`, `set_volume`, `set_pitch`, `set_position` | Voice kontrolü | `modules/audio/include/astral/audio/audio.hpp:78-82` |
| `set_listener(pos, forward, up)` | Tek dinleyici (indeks 0) | `modules/audio/include/astral/audio/audio.hpp:84` |
| `AudioBus` (`Effects`, `Music`, `Ambience`, `Interface`), `set_bus_volume`, `bus_volume`, `set_master_volume` | Sabit 4 kanal, ana ses | `modules/audio/include/astral/audio/audio.hpp:32`, `:85-87` |
| `update()` | Ana thread'de, kare başına: biten voice'ları serbest bırakır | `modules/audio/include/astral/audio/audio.hpp:90` |
| `read(span<float>)` | Yalnızca null cihazda: miks'i isteğe bağlı çeker (sunucu, deterministik test) | `modules/audio/include/astral/audio/audio.hpp:97` |
| `mixed_frames()` | Null cihazda `read()` ile üretilen toplam kare; App başsız saat için kullanır | `modules/audio/include/astral/audio/audio.hpp:99` |
| `info(sound)`, `voice_count`, `has_device`, `sample_rate`, `channels` | Sorgular | `modules/audio/include/astral/audio/audio.hpp:73`, `:91-94` |
| `AudioSource`, `AudioListener` (App) | Sahneye yazılan component'ler; sesi entity'ye bağlar | `modules/app/include/astral/app/components.hpp:248`, `:262` |

## Nasıl çalışır

**Motor.** `ma_engine` + her `AudioBus` için bir `ma_sound_group`; `create` motoru başlatır, `null_device` için
`noDevice` ayarlar (`modules/audio/src/audio.cpp:49-66`). Cihaz açılamazsa `create` hata metniyle `nullptr` döner
(`modules/audio/src/audio.cpp:55-58`); `App` bu durumda sessiz (null) motora düşer (`modules/app/src/services/audio_bridge.cpp:25-29`) ve onu oyun saatiyle ilerletir (ADR-0054).
miniaudio arka uçları çalışma anında yüklenir; sistem ses kütüphanesine link edilmez (`modules/audio/CMakeLists.txt:3-4`).
Kodlama ve ses üretimi derleme dışı bırakılmıştır (`modules/audio/CMakeLists.txt:26`).

**Yükleme.** `load` kodlanmış baytları kopyalayıp miniaudio kaynak yöneticisine bellekteki veri olarak kaydeder, çözme için
bir "prototip" `ma_sound` açar ve bunu hiç çalmadan tutar; çözülmüş veri böylece önbellekte kalır (`modules/audio/src/audio.cpp:82-108`).
`stream=true` olsa bile kodlanmış bayt dizisinin tamamı bellekte durur (`modules/audio/src/audio.cpp:86`); akış yalnızca çözmeyi çalarken
yapar. Çözülemezse `Warn` log'u ve geçersiz handle (`modules/audio/src/audio.cpp:92-99`).

**Voice.** `play` her çağrıda yeni bir `ma_sound` başlatır; `max_voices` dolunca `Warn` loglayıp geçersiz handle döner
(`modules/audio/src/audio.cpp:129-158`). Konumlu değilse `NO_SPATIALIZATION` bayrağı verilir; konumlu ise konum, min/max mesafe ayarlanır ve
Doppler kapatılır (`modules/audio/src/audio.cpp:141`, `:147-152`). Mesafe zayıflama modeli kodda ayarlanmaz; miniaudio'nun varsayılanı (ters
mesafe, `AL_INVERSE_DISTANCE_CLAMPED` eşdeğeri) kullanılır (kaynak: miniaudio 0.11 belgesi, derleme dizinindeki
`miniaudio.h`; bu kodda doğrulanmadı). Bu modelde ses `max_distance`'da sıfıra inmez, o mesafeden sonra sabit bir
düzeyde kalır. `stop(voice)` (fade'siz) voice'u hemen serbest bırakır; fade'li durdurma sesi söner ve voice sonraki `update`
çağrısında serbest kalır (`modules/audio/src/audio.cpp:160-166`, `:202-209`).

**Dinleyici.** Tek dinleyici: pozisyon, yön ve yukarı vektörü doğrudan miniaudio'ya verilir (`modules/audio/src/audio.cpp:183-187`).
`App` `AudioListener` taşıyan entity'yi, yoksa ilk aktif kamerayı dinleyici yapar
(`modules/app/src/services/audio_bridge.cpp:50-67`).

**App bağlaması.** `AudioBridge::sync` her karede (frame loop'taki `audio` profil kapsamı içinde, `modules/app/src/services/frame_loop.cpp:157-159`)
dinleyiciyi günceller, `AudioSource` değişikliklerini voice'lara uygular, biten tek seferlik sesin `playing`'ini sıfırlar,
silinen entity'nin voice'unu 50 ms fade ile durdurur ve `update()` çağırır (`modules/app/src/services/audio_bridge.cpp:69-153`). Sesler yol
adıyla bir kez yüklenir ve **hiç boşaltılmayan** bir `unordered_map`'te tutulur (`modules/app/src/services/audio_bridge.cpp:34-48`).

**Thread modeli.** Çalma miniaudio'nun kendi ses thread'inde yapılır; `AudioEngine` API'si, `HandlePool` thread-safe olmadığı için,
tek thread'den (ana thread) çağrılmalıdır (`modules/audio/include/astral/audio/audio.hpp:89`; `modules/core/include/astral/core/handle.hpp:24`). Bu kural
başlıkta yalnızca `update()` için yazılıdır, diğer çağrılar için açıkça belirtilmemiştir.

## Güçlü yanlar

- Sessiz cihaz birinci sınıf: aynı mikser `read()` ile deterministik olarak çekilebilir ve testler çıkışın RMS'ini ölçer
  (`modules/audio/tests/audio_test.cpp:41-51`).
- `max_voices` sınırı çalışır ve test edilmiştir (`modules/audio/tests/audio_test.cpp:127-136`).
- Bozuk/boş veri reddedilir, çökme yok (`modules/audio/tests/audio_test.cpp:62-72`).
- miniaudio tipleri public header'a sızmaz; modül yalnızca Core'a bağlıdır (`modules/audio/CMakeLists.txt:20-21`).
- Sahne dosyasına yazılabilir component'ler (reflect edilmiş): `ASTRAL_REFLECT(astral::AudioSource, ...)`
  (`modules/app/include/astral/app/components.hpp:299`) ve `AudioBus` enum'u isimle serileştirilir (`modules/audio/include/astral/audio/audio.hpp:110`).
- App seviyesinde entity ömrüne bağlı voice yönetimi testli: entity silinince döngülü ses durur
  (`modules/app/tests/audio_app_test.cpp:65`).

## Zayıf yanlar ve bilinen sınırlar

- **Eksik özellikler (kodda bulunmayan):** filtre (low/high-pass), reverb/efekt zinciri, ses tıkanması (occlusion), bus başına
  efekt veya ducking, panlama kontrolü, geri çağrı/olay (ses bitti), çalma konumu sorgusu/seek, birden çok dinleyici,
  dinleyici/kaynak hızı (Doppler bilinçli kapalı, `modules/audio/src/audio.cpp:152`). miniaudio'da bulunan filtre ve efekt
  düğümleri bu API'den erişilemez.
- **PCM/ham veri yükleme yok:** `load` yalnızca kodlanmış kapsayıcı (WAV, FLAC, MP3) kabul eder
  (`modules/audio/include/astral/audio/audio.hpp:70`); `MA_NO_GENERATION` ile sentez de kapalı. OGG/Vorbis/Opus çözülmez
  (hata mesajı yalnızca WAV/FLAC/MP3 der, `modules/audio/src/audio.cpp:98`).
- **Akış gerçek anlamda akış değil:** kodlanmış dosya tamamen RAM'e kopyalanır (`modules/audio/src/audio.cpp:86`); `stream=true` yalnızca çözmeyi
  çalma sırasına erteler. Çözülmüş (`stream=false`) ses PCM olarak bellekte durur; boyut ölçülmedi.
- **Cihaz seçimi ve ayarları yok:** yalnızca varsayılan çıkış cihazı; gecikme/period ayarı, cihaz değişimi/çıkarılması yönetimi
  `AudioDesc`'te yok (`modules/audio/include/astral/audio/audio.hpp:34-40`). Yalnızca 2 kanal (stereo) test edilmiştir (`modules/audio/tests/audio_test.cpp:46-47` stereo varsayar).
- **Başsız/offscreen modda karışım oyun saatiyle ilerler, ama atılır:** cihaz yoksa `AudioBridge::sync` her kare oyunun okumadığı
  kadar kareyi `read()` ile çekip atar (en fazla 1 sn yetişir), böylece sesler biter ve birikmez (`modules/app/src/services/audio_bridge.cpp:69-82`,
  ADR-0054; test `modules/app/tests/audio_app_test.cpp:93`). Duyulabilir çıktı yoktur; karışımı isteyen kod `read()` ile kendisi çeker.
- **`AudioBridge` ses önbelleği büyür:** yüklenen sesler hiç boşaltılmaz; başarısız yükleme de geçersiz handle olarak önbellekte
  kalır ve yeniden denenmez (`modules/app/src/services/audio_bridge.cpp:34-48`). Dosya sonradan eklense bile yeniden yüklenmez.
- **Sessiz hata yolları:** `play` içinde `ma_sound_init_from_file` başarısız olursa log'suz geçersiz handle döner
  (`modules/audio/src/audio.cpp:143`); `logger` yoksa `Warn` iletileri hiç görünmez (`modules/audio/src/audio.cpp:37-39`).
- **Küçük API boşlukları:** `set_master_volume` var, getter yok (`modules/audio/include/astral/audio/audio.hpp:87`); `set_volume` ve `set_pitch` değer doğrulamaz;
  `stop` fade'siz anında keser (tık sesi riski, doğrulanmadı) (`modules/audio/src/audio.cpp:165`).
- **Luau'da ses API'si yok:** `modules/script` içinde `audio`/`sound` geçen bir kaynak bulunamadı (grep). Oyun betiği ses
  çalmak için C++ özelliğine (ör. `examples/pong/src/features/pong_audio/audio.cpp`) ihtiyaç duyar.
- **Thread güvenliği belgesiz** (yukarıda "Thread modeli").
- Kaynakta `TODO`/`FIXME` işareti yok.

## Testler

`modules/audio/tests/audio_test.cpp` (4 test, hedef `astral_audio_tests`, `modules/audio/CMakeLists.txt:31-33`); hepsi null
cihazla çalışır, gerçek ses cihazı kullanılmaz:

| Test | Kapsam |
|---|---|
| `sounds decode from memory; garbage is refused` (`modules/audio/tests/audio_test.cpp:62`) | WAV çözme, süre/kanal bilgisi, bozuk/boş veri |
| `voices play, end on their own, stop, and buses scale them` (`modules/audio/tests/audio_test.cpp:74`) | çalma, kendiliğinden bitme, döngü, bus seviyesi, `stop`, `destroy` |
| `3D voices pan towards their side and fade with distance` (`modules/audio/tests/audio_test.cpp:108`) | sağa pan, uzaklıkla azalma (yalnızca 40 m'de < %30) |
| `the voice limit holds` (`modules/audio/tests/audio_test.cpp:127`) | `max_voices` |

App tarafı: `modules/app/tests/audio_app_test.cpp` (AudioSource çalma/izleme/bitme, `ctx.sound` önbelleği, entity silme).

**Kapsanmayanlar:** gerçek cihaz yolu (`create` null olmayan), `stream=true`, FLAC/MP3 çözme (testte yalnızca WAV üretilir),
`fade_in`/`fade_out`, `set_pitch`, `set_master_volume`, birden çok bus'ın birlikte davranışı, `max_distance` ötesi seviye,
hatalı `AudioBus` değeri, çok thread kullanımı.

## Performans

Ölçülmedi. Voice başına CPU, bellek ve gecikme ölçümü yok; `max_voices=256` varsayılanının gerçek bir cihazda yeterliliği
doğrulanmadı.

## Kararlar

- [ADR-0020: Ses, miniaudio üstünde `Astral::Audio`, component'lerle sahneye bağlama](../decisions/0020-audio.md)
  (ADR "bilinçli sonraya kalanlar" olarak akış müziği, reverb ve occlusion'ı sayar; bu belgedeki eksik listesiyle tutarlı)
- [ADR-0054: Başsız çalışmada sessiz mikser oyun saatiyle ilerler](../decisions/0054-headless-silent-mixer-clock.md)
