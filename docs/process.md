# Geliştirme Süreci

## Bir faz nasıl ilerler

1. **Plan:** `docs/superpowers/plans/<tarih>-<faz>.md` — görevler, arayüzler, testler.
2. **Branch:** `feat/<faz>`; her görev test-önce (TDD): test yaz → başarısız olduğunu gör → uygula → geçir → commit.
3. **İnceleme:** faz sonunda bağımsız bir inceleme; kritik/önemli bulgular testle düzeltilir,
   küçükler modül dokümanının "Bilinen sınırlar" bölümüne yazılır.
4. **Dokümanlar** (aşağıdaki liste) güncellenir, sonra `v3`'e fast-forward + push
   (önce `git merge-base --is-ancestor origin/v3 HEAD`).

## "Faz bitti" tanımı — her fazın sonunda zorunlu

- [ ] Tüm testler geçiyor: debug, release (`ASTRAL_WARNINGS_AS_ERRORS=ON`, uyarı yok) ve tsan (core, world,
  physics, script test hedefleri). Oyun örneklerinde `astral-features check` (CTest `*_features_check`).
- [ ] Görüntü testleri geçiyor (`ctest -L golden`); görüntüyü bilerek değiştiren faz referansları
  `ASTRAL_UPDATE_GOLDEN=1 ctest -L golden` ile günceller ve yeni PNG'leri gözle kontrol edip commit'ler.
- [ ] `docs/roadmap.md`: fazın durumu ✅, bir sonraki faz 🔜, kısa sonuç notu.
- [ ] `docs/modules/<modül>.md`: yeni/değişen modüllerin API özeti ve bilinen sınırları.
- [ ] `docs/decisions/`: fazda alınan her mimari karar için bir ADR.
- [ ] `CHANGELOG.md`: fazın eklediği/değiştirdiği/düzelttiği şeyler.
- [ ] `README.md` durum tablosu.
- [ ] Oyun yazmayı etkileyen değişiklikte `docs/guides/writing-a-game.md` ve `AGENTS.md`.

## Commit mesajları

`feat(modül): …`, `fix(modül): …`, `build: …`, `docs: …`, `test(modül): …`.
