# ADR-0054 — Başsız çalışmada sessiz mikser oyun saatiyle ilerler

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `audio` (`AudioEngine::mixed_frames`), `app` (`AudioBridge::sync`)
- **İlişkili:** [ADR-0020](0020-audio.md), [ADR-0046](0046-headless-input-script.md)

## Bağlam

Cihaz yoksa (başsız, offscreen ya da cihaz açılamadı) App bir null-device motoru kuruyordu. Bu motor yalnız `AudioEngine::read()` çağrılınca
karışım üretir, ama App onu hiç çağırmıyordu. Tek seferlik sesler bitmiyor, `AudioSource::playing` hep `true` kalıyor, sesler `max_voices`'a kadar
birikiyordu. Testler ve Pong bu yüzden karışımı kendileri okuyordu.

## Karar

- `AudioEngine::mixed_frames()` `read()` ile üretilen toplam kareyi verir.
- `AudioBridge::sync(delta)` oyun saatinin borçlu olduğu kare sayısını (`delta × sample_rate`) tutar. **Oyunun o ana kadar okumadığı** kadarını
  okuyup atar. Karışımı kendisi okuyan test ya da araç iki kat ilerletilmez.
- Uzun bir takılmadan sonra en fazla 1 saniyelik ses yetiştirilir.

## Sonuçlar

- Başsız oyunlarda ses olayları (bitiş, `playing`) gerçek zamanlı akar, sesler birikmez. Test: `modules/app/tests/audio_app_test.cpp`.
- Karışımı kendisi okuyan kod (`examples/pong/tests/game_test.cpp`) aynı sonucu alır.
- `sync()` varsayılan `delta = 0` ile mikseri ilerletmez (iç birim testleri).
