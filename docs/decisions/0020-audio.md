# ADR-0020 — Ses: miniaudio üstünde `Astral::Audio`, component'lerle sahneye bağlama

- **Durum:** Kabul (uygulandı: M8.5)
- **Tarih:** 2026-11-21
- **Bağlam:** yol haritası M8, ADR-0006 (App), ADR-0014 (asset'ler), ADR-0016 (paketleme)

## Karar

1. **Arka uç miniaudio** (public domain / MIT-0, tek dosya): cihaz çıkışı, WAV/FLAC/MP3 çözme, mikser,
   3D panlama ve mesafe zayıflaması. Sistem ses kütüphanesine link edilmez; arka uçlar çalışma anında
   yüklenir. miniaudio tipleri public header'a sızmaz.
2. **Modül ECS bilmez.** `AudioEngine`: `load` (bayttan), `play` → `VoiceHandle`, `stop` (fade), ses
   seviyesi/pitch/konum, dinleyici, kanallar (`AudioBus`: Effects, Music, Ambience, Interface).
3. **Sessiz cihaz birinci sınıf.** Headless, offscreen ve cihaz bulunamayan ortamlar aynı mikseri çalıştırır
   (`null_device`); testler `read()` ile sesi çeker ve sesin gerçekten üretildiğini ölçer.
4. **App bağlaması veriyle.** Reflect edilmiş `AudioSource` (ses yolu, kanal, seviye, döngü, 3D, mesafe,
   `playing`) ve `AudioListener` component'leri; sahne dosyasına yazılır. Dinleyici `AudioListener`'dan,
   yoksa aktif kameradan. Tek seferlik ses bitince `playing` kendiliğinden `false` olur; component silinince
   ses durur.
5. **Sesler asset yoluyla.** `ctx.sound("sounds/hit.wav")` bir kez yükler, önbellekte tutar; gevşek klasörden
   ya da `.apak`'tan okur (`AssetManager::read`). Olay sesleri koddan `ctx.audio->play(...)` ile çalınır.

## Sonuçlar

- Pong bir oyun olayından (`Bounced`, `Scored`) ses çalıyor; sahne dosyasında bir `AudioSource` ortam sesi.
- Bilinçli olarak sonraya kalanlar: streaming müzik, DSP efekt zinciri (reverb), ses tıkanması (occlusion,
  fizik raycast'iyle), fizik materyali → çarpma sesi eşlemesi (M12).
