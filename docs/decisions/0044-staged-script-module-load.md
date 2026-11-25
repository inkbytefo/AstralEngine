# ADR-0044 — Luau modül yüklemesinde aşamalı (staged) yan etkiler

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `script` (`ScriptHost::load`, `astral.*` binding'leri), `world` (`TypeRegistry`/`EventRegistry::rollback_dynamic`), MSR AG09
- **İlişkili:** [ADR-0028](0028-script-tooling-and-shared-features.md), [App & Gameplay denetimi](../archive/audits/app-gameplay-deep-audit-2026-11-25.md)

## Bağlam

Aday modülün top-level kodu canlı ortamda çalışır. `astral.define_component`, `astral.define_event`, `astral.spawn` ve entity değiştiren çağrılar canlı `World` ve registry'leri hemen değiştiriyordu. Yükleme sonradan hata verirse eski modül korunuyor ama dünya/registry yan etkileri kalıyordu; "eski modül çalışmaya devam eder" garantisi yalnız callback'ler için geçerliydi.

## Seçenekler

- **World mutasyon günlüğü ve rollback:** World API'sine ve tüm binding'lere dokunur; kapsam büyük.
- **Tanımları bekleyen listede tutup commit'te uygulamak:** top-level `astral.system { query = {"game::X"} }` aynı modülde tanımlanan tipi kayıt anında çözdüğü için çalışmaz.
- **Bildirim aşaması + registry geri alma + commit sonrası hook (seçilen).**

## Karar

- Top-level yükleme bir **bildirim aşamasıdır** (`Impl::loading`). Bu aşamada:
  - World'ü veya dış durumu değiştiren çağrılar Lua hatası verir ("… is not allowed while the module loads; call it from on_load() or a callback"): `astral.spawn`, `entity:add/remove/destroy`, component alanına yazma, `astral.send`, `astral.save/load`, `astral.physics` komutları (impulse, force, set_velocity, move_character, teleport, wake, sleep, joint motor/break).
  - `define_component` / `define_event` registry'ye hemen yazılır (aynı modül onları kullanabilsin), ama yükleme öncesi `entries().size()` işaretlenir. Yükleme başarısız olursa `TypeRegistry::rollback_dynamic` ve `EventRegistry::rollback_dynamic` işaretten sonraki girdileri geri alır. Geri alınan tip girdileri `retired_` içinde yaşatılır; `ComponentId` (tip etiketinin adresi) sonraki bir tanımda yeniden kullanılmaz.
  - Okuma çağrıları (`find`, `query`, `each`), callback/timer/listener kaydı serbesttir; kayıtlar modül yereldir ve hata halinde zaten bırakılır.
- Yükleme başarılı olunca, eski sürüm yerine geçtikten sonra modülün global `on_load()` fonksiyonu (varsa) çağrılır. `on_load` hatası diğer callback hataları gibi raporlanır ve modülü `failed` yapar; mesaj `load` sonucunda döner. Commit geri alınmaz.
- `astral.spawn` önce tüm bileşen adlarını çözer, sonra entity yaratır; alan hatasında (Lua hatası C++ exception olarak açılır, `LUA_USE_LONGJMP 0`) entity yok edilir.

## Sonuçlar

- Başarısız reload dünyayı, tip ve event registry'sini değiştirmez.
- Top-level'da entity yaratan veya event gönderen script'ler `on_load`'a taşınmalıdır (depodaki örnekler `spawn`'u fonksiyon içinde çağırıyor).
- `rollback_dynamic` yalnız script host için güvenlidir: geri alınan tiplerin hiçbir entity'de bulunmadığını bildirim aşaması garanti eder.
- Test: `modules/script/tests/msr_regression_test.cpp` içinde AG09 senaryoları.
