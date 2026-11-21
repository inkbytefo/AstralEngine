# AstralEngine — AI ajanları için notlar

- Mimari ve kurallar: `docs/architecture.md`. Değişmez kurallar oradaki listede.
- Süreç: `docs/process.md`. **Her faz sonunda "Faz bitti" listesindeki dokümanları güncelle.**
- Yeni karar → `docs/decisions/` altına ADR. Bilinen eksik → modül dokümanının "Bilinen sınırlar" bölümü.
- TDD: önce başarısız test. Derleme: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`.
- Public header'lara üçüncü parti tip, singleton veya global state ekleme.
- Kod/yorumlar İngilizce, `docs/` Türkçe.
