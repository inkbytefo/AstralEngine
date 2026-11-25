# ADR-0043 — Fizik komutlarında deterministik üretici sırası

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `app` (`PhysicsCommandQueue`, `SystemScheduler`), ADR-0034 §3'ü günceller, MSR PH05
- **İlişkili:** [ADR-0034](0034-gameplay-api-and-physics-step-contract.md), [Physics denetimi](../archive/audits/physics-jolt-deep-audit-2026-11-25.md)

## Bağlam

Kuyruk komutları `(order_key, sequence)` ile sıralıyordu. Varsayılan `order_key` entity'dir; `sequence` ise mutex'i alma sırasıyla verilir. Aynı fazda paralel çalışan iki sistem aynı entity'ye `set_linear_velocity` gönderdiğinde anahtarlar eşit, kazanan thread zamanlamasına bağlıydı. ADR-0034'ün "seri = paralel" iddiası setter'larda sağlanmıyordu.

## Seçenekler

- **Setter'larda `order_key` zorunlu:** public API ve oyun kodu kırılır.
- **Çakışan setter'ları reddetmek:** davranış değişikliği, oyun kodunu bozar.
- **Üretici kimliği (seçilen):** sıralama anahtarına zamanlayıcının bildiği deterministik üretici sırası eklenir.

## Karar

- Komut sıralama anahtarı `(order_key, producer, sequence)` olur.
- `producer` = (faz, sistemin kayıt sırası) bileşimi. `SystemScheduler` her `System::update` çağrısını, `app/src` içinde tanımlı thread-local bir üretici kapsamıyla sarar; public header'a tip veya global eklenmez.
- `sequence` (kilit sırasıyla verilen genel sayaç) yalnız aynı üreticinin komutlarını sıralar; tek thread'de çalışan bir üreticinin çağrı sırası böylece korunur. Ayrı üretici başı sayaç gerekmez.
- Sistem dışından (ana thread: frame loop, oyun callback'leri, editor) gelen komutlar üretici `0` alır: aynı `order_key` içinde sistemlerden önce, kendi aralarında gönderim sırasıyla uygulanır (tek thread olduğu için deterministik).
- Bir sistem kendi içinde paralel job açıp aynı entity'ye çakışan komut üretiyorsa, sırayı açık `order_key` ile vermelidir. Aynı üretici kapsamında farklı thread'lerden gelen komutlar için sıra garanti edilmez; bu sözleşme `physics_service.hpp` yorumunda yazılıdır (çalışma zamanı uyarısı yok).

## Sonuçlar

- Paralel ve seri zamanlama aynı komut sırasını üretir; ADR-0034 §3 bu ADR ile tamamlanır.
- Kayıt sırası artık semantiktir: sistem kayıt sırasını değiştirmek aynı entity'ye çakışan setter'ların kazananını değiştirebilir.
- Test: `modules/app/tests/physics_step_contract_test.cpp` içinde zorlanmış ters zamanlamalı çakışan setter senaryosu (PH05).
