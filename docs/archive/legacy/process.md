# Geliştirme Süreci

## Bir faz nasıl ilerler

1. **Plan:** `docs/superpowers/plans/<tarih>-<faz>.md` — görevler, arayüzler, testler.
2. **Worktree:** tek uzun ömürlü dal `master`. Her görev `master`'dan açılan ayrı bir git worktree'de, kendi
   dalında yapılır (bağımsız görevler paralel worktree'lerde); her görev test-önce (TDD): test yaz → başarısız
   olduğunu gör → uygula → geçir → commit.
3. **İnceleme:** faz sonunda bağımsız bir inceleme; kritik/önemli bulgular testle düzeltilir,
   küçükler modül dokümanının "Bilinen sınırlar" bölümüne yazılır.
4. **Dokümanlar** (aşağıdaki liste) güncellenir, sonra görev dalı `master`'a merge edilip push'lanır ve
   görev dalı silinir. Eski dallar yalnızca `archive/*` tag'leri olarak durur (`archive/main-deprecated`,
   `archive/v2-old`).

## "Faz bitti" tanımı — her fazın sonunda zorunlu

- [ ] Tüm testler geçiyor: debug, release (`ASTRAL_WARNINGS_AS_ERRORS=ON`, uyarı yok) ve tsan (core, world,
  physics, script test hedefleri). Oyun örneklerinde `astral-features check` (CTest `*_features_check`).
- [ ] Görüntü testleri geçiyor: lavapipe/CI üzerinde `ctest -L '^golden$'`, donanım GPU'sunda `ctest -L '^golden-hw$'`
  ([ADR-0031](decisions/0031-hardware-independent-golden-tests.md)); görüntüyü bilerek değiştiren faz referansları
  yalnız lavapipe üzerinde `ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$'` ile günceller ve yeni PNG'leri gözle kontrol edip commit'ler.
- [ ] `docs/roadmap.md`: fazın durumu ✅, bir sonraki faz 🔜, kısa sonuç notu.
- [ ] `docs/modules/<modül>.md`: yeni/değişen modüllerin API özeti ve bilinen sınırları.
- [ ] `docs/decisions/`: fazda alınan her mimari karar için bir ADR.
- [ ] `CHANGELOG.md`: fazın eklediği/değiştirdiği/düzelttiği şeyler.
- [ ] `README.md` durum tablosu.
- [ ] Oyun yazmayı etkileyen değişiklikte `docs/guides/writing-a-game.md` ve `AGENTS.md`.

## Technology Lab ve Game API teslimi

[Technology Labs roadmap](technology-labs-roadmap.md) kapsamındaki genel motor yetenekleri için faz kapanışına ek olarak: ilk oynanabilir lab dilimi, en az bir ikinci bağımsız public SDK tüketici testi, uygulanabilir C++/Luau API ve generated type check, serialization/reflection/cook sözleşmesi, ölçüm ve kaynak/kanıt SHA'sı gerekir. Lab ve oyun kodu motor private header'larına bağımlı olmaz. Gameplay kuralları isteğe bağlı FeatureSet paketlerinde kalır. MSR altyapı engeli sırasında sonuçlar pending verification olarak tutulur; çalıştırılmamış test PASS sayılmaz. VR/XR bu programın kapsamı dışındadır.

## Yerel ajan dosyaları ve paylaşılan rehberler

- **Paylaşılan rehberler (git'te takip edilir):** `CLAUDE.md`, `AGENTS.md` ve `docs/` altındaki mimari,
  süreç ve ADR belgeleri bilinçli kararla paylaşılır ve incelemeyle güncellenir.
- **Yerel ajan/çalışma zamanı ve üretilmiş grafik çıktıları (`.gitignore`):** `/.agents/`, `/.codex/`,
  `/.claude/` (depo içi `/.claude/worktrees/` dahil) ve `/graphify-out/` kök kurallarıyla yoksayılır;
  yerel ayar, çalışma zamanı ve üretilmiş grafik dosyaları commit'lenmez ve olağan `git status` çıktısını kirletmez.

## Commit mesajları

`feat(modül): …`, `fix(modül): …`, `build: …`, `docs: …`, `test(modül): …`.
