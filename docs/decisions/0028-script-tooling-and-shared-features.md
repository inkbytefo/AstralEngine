# ADR-0028 — Script araçları (tip denetimi, bytecode), ortak özellikler, gamepad, script yardımcıları

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** ADR-0017/0026 (Luau), ADR-0023 (özellik paketleri), ADR-0027 (şablon). M12'den önce kapatılan M11 artıkları.

## Kararlar

1. **Tip denetimi Luau'nun kendi çözümleyicisiyle:** stok `luau-analyze` CLI'si tanım dosyası yükleyemiyor; `astral-luau-check` küçük bir sürücüdür
   (`Frontend::loadDefinitionFile` + `check`). `astral.d.luau` `keyof`/`index` tip işlevleriyle component ve olay adlarına göre indekslenir (19 aşırı
   yükleme "çok karmaşık" hatası veriyordu). `astral_check_scripts` her oyunda CTest olarak çalışır; `--script-api-only` oyunu başsız bir karede açıp
   API'yi yazar. Luau Analysis kütüphanesi yalnız testler açıkken derlenir (`ASTRAL_LUAU_CHECK`, ~1 dk).
2. **Bytecode cook:** `.luau` → `"ALBC"` + Luau bytecode; `CookOptions::compile_script` ile `Assets` Script'e bağlanmaz (katman korunur), `astral-cook`
   derleyiciyi takar. Cook sürümü yükseltildi. Host kaynak ve bytecode'u magic ile ayırır.
3. **Ortak özellikler tek kaynak + kopya + denetim:** `features/<ad>/{src,assets}` kaynak; oyunlar kopya taşır (kendi paketini taşıyan tek klasör
   ilkesi) ve `tools/sync_features.py --check` (CTest `shared_features_in_sync`) kaymayı yakalar. Çok köklü asset katmanı (overlay) şimdilik gerekmedi.
4. **Gamepad:** `Input` ilk bağlı gamepad'in düğme/eksenini taşır (SDL3 gamepad olayları `Window::poll`'da); `.actions` `gamepad`, `gamepad_axis`, `stick`,
   `dpad`, `stick_scale` anahtarlarını okur; ölü bölge 0.15 (çubuklar radyal). Oyun kodu değişmez.
5. **Script yardımcıları Luau'da yazıldı** (`prelude.hpp`; sandbox kapanmadan yüklenir): `astral.tween/ease/lerp`, `astral.state_machine`. Kayıt C++'ta:
   `astral.save/load/has_save` listelenen component'leri `save_world` ile yazar (`ScriptStorage` arayüzü; App dosyaya yazar, test bellekte tutar).
   `load` o component'lere sahip entity'leri yok edip kaydedileni geri getirir.
6. **Render düzeltmesi (ayrı):** ışın başlangıç payı kameraya uzaklıkla ölçeklenir (`ray_bias`); CHANGELOG.

## Hâlâ yok

Tetik hacmi (M12 fizik sensörleri), titreşim ve çoklu gamepad, luau-lsp proje ayarı, ortak özellikler için asset overlay.
