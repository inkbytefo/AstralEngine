# ADR-0052 — Luau zaman bütçesi modül ve faz başına

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `script` (`ScriptLimits::budget_ms`, `ScriptHost::run`, interrupt)
- **İlişkili:** [ADR-0017](0017-gameplay-scripting.md), [ADR-0044](0044-staged-script-module-load.md)

## Bağlam

Bütçe fazda bir kez kuruluyordu ve bütün modüller onu paylaşıyordu. Aşımda interrupt deadline'ı `max`'a çekiyordu ("bir kez raporla"); bu
yüzden aşan modülden sonraki modüller o faz boyunca **bütçesiz** koşuyordu. Aynı fazdaki ikinci bir sonsuz döngü oyunu kilitliyordu. Test:
sonlu ama uzun bir döngü 18 sn kesilmeden koştu (`modules/script/tests/script_test.cpp`).

## Karar

- Deadline her modülden önce yeniden kurulur: bir modülün o fazdaki dinleyicileri ve sistemleri `budget_ms` içinde bitmeli.
- Bütün behaviour'lar fazda bir bütçe paylaşır; bütün zamanlayıcılar da (`astral.after/every`) ayrı bir bütçe.
- Interrupt bir çağrıyı bütçe aşımıyla durdurursa `Impl::budget_overrun` işaretlenir. `protected_call` dönüşte yeni bir bütçe kurar,
  sonraki çağrılar yine sınırlıdır.
- Varsayılan 100 ms değişmedi.

## Sonuçlar

- Bir fazın toplam süresi artık "modül sayısı × bütçe" ile sınırlı, tek bütçeyle değil. Çok modüllü oyunda kötü durumda faz daha uzun
  sürebilir. Karşılığında hiçbir modül sınırsız koşmaz.
- Önceki modüllerin yavaşlığı sonraki modülü haksız yere kesmez. Paralel test yükünde 100 ms bütçeye takılan `pong_tests` /
  `spinning_cubes_tests` flaky'liğine etkisi ölçülmedi.
- Behaviour'lar entity'ler arasında modülleri karıştırarak koştuğu için modül başına değil grup başına bütçe alır.
