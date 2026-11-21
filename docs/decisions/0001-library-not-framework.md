# ADR-0001: Motor bir kütüphane, framework değil

**Durum:** Kabul · 2026-11-21

**Bağlam:** Tek motor üzerinde birden çok oyun (kılıç, sürüş, FPS) yapılacak; AI ajanları
(Claude Code) motoru editör olmadan, koddan ve headless testlerle kullanabilmeli.

**Karar:** Oyun main loop'un sahibidir ve motor modüllerini çağırır. Motor oyuna
callback ile hükmetmez. Hazır döngü isteyenler için ileride opsiyonel `Astral::App`.

**Sonuçlar:** + Güncelleme sırası, fizik adımı, thread'ler oyunun kontrolünde; modüller
bağımsız test edilir. − Basit bir oyun için biraz daha başlangıç kodu (App ile kapanır).
Performans farkı yok.
