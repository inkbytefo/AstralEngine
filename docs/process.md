# Geliştirme Süreci

Hem insan geliştirici hem yapay zekâ ajanları için geçerli çalışma kuralları.

## Bir iş nasıl ilerler

1. **Oku, planla:** ilgili modül dokümanı ([modules/](modules/)), [architecture.md](architecture.md), ADR'ler ve kod. Büyük işler için önce plan; kullanıcı onayı olmadan büyük değişikliğe başlanmaz.
2. **Worktree:** uzun ömürlü tek dal `master`. Her iş `master`'dan açılan ayrı bir git worktree'de (`.claude/worktrees/<ad>`) kendi dalında yapılır; bağımsız işler paralel worktree'lerde. Ana checkout'ta kullanıcının commit'lenmemiş dosyalarına dokunulmaz.
3. **TDD:** önce başarısız test yazılır ve **koşturularak kırmızı görülür**, sonra uygulama, sonra yeşil. Testle kanıtlanamayan şeyler (GPU süresi, işletim sistemi davranışı) ölçümle raporlanır ve bu açıkça yazılır.
4. **Madde başına commit:** biten her iş ayrı commit; kesinti olursa kayıp küçük kalır.
5. **Doğrulama:** Debug ve Release, `-DASTRAL_WARNINGS_AS_ERRORS=ON`, tam ctest (aşağıda).
6. **Merge:** kullanıcı onayıyla `master`'a `--no-ff` merge ve push. Görev dalı silinir; tarihçe için `archive/<dal>` tag'i push'lanır. Eski dallar yalnız `archive/*` tag'leridir.

## "Bitti" tanımı

- [ ] Debug ve Release, warnings-as-errors, tam ctest geçiyor. Bilinen ve [status.md](status.md)'de kayıtlı başarısızlıklar dışında hata yok; şüpheli flaky testler seri olarak yeniden koşturulur.
- [ ] Denetimler geçiyor: `code_size_check`, `layer_check`, `editor_isolation_check`, `astral_public_headers_clean`, `shared_features_in_sync`, `docs_check`, `test_environment_check`, oyunlarda `*_features_check` ve `*_script_types`.
- [ ] Görüntü testleri: CI/lavapipe'ta `ctest -L '^golden$'`, donanım GPU'sunda `ctest -L '^golden-hw$'` ([ADR-0031](decisions/0031-hardware-independent-golden-tests.md)). Görüntüyü bilerek değiştiren iş referansları yalnız lavapipe'ta `ASTRAL_UPDATE_GOLDEN=1` ile günceller ve gözle kontrol eder.
- [ ] tsan: CI'da (`linux-tsan` işi; core, world, physics, script, app test hedefleri). Windows'ta tsan/asan çalıştırılmaz; bu durumda "doğrulanmadı" diye yazılır.
- [ ] Dokümanlar: değişen modülün `docs/modules/<modül>.md` dosyası (API, güçlü/zayıf yanlar, testler), [status.md](status.md), [roadmap.md](roadmap.md) (durum), yeni mimari karar için ADR ([decisions/](decisions/)), `CHANGELOG.md`; oyun yazmayı etkiliyorsa [guides/writing-a-game.md](guides/writing-a-game.md) ve `AGENTS.md`.

## Doküman kuralları

- `docs/` Türkçe; kod, yorumlar ve commit mesajları İngilizce.
- Dokümanlar **bugünkü kodu** anlatır; plan ve hedefler yalnız [roadmap.md](roadmap.md)'de. Zayıf yanlar ve bilinen sınırlar yazılması zorunlu bölümlerdir.
- API ve davranış iddiaları `path:line` kaynak referansı taşır. `docs_check` (`tools/check_docs.py`) kırık linkleri ve dosya sonunu aşan referansları yakalar; kod değişince satır numaraları güncellenir.
- Ölçülmemiş sayı yazılmaz; ölçümler ortamıyla (donanım, çözünürlük, build türü, sahne) birlikte yazılır.
- `docs/archive/` tarihçedir, güncellenmez ve denetlenmez.

## Windows'ta derleme (bu makine)

- Visual Studio 2026 (MSVC 19.51), CMake 4.2, Ninja. Önce `vcvars64.bat`, `CC=cl CXX=cl`, ortamda `CMAKE_POLICY_VERSION_MINIMUM=3.5`.
- Çevrimdışı derleme için `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` ve `FETCHCONTENT_SOURCE_DIR_<AD>` değişkenleriyle ana checkout'taki `build/debug/_deps/*-src` klasörleri kullanılır.
- **Aynı `cmd` sürecinde `vcvars64.bat` iki kez çağrılmaz:** PATH taşar ("The input line is too long", ninja `CreateProcess` hatası 87).
- Ninja `heavy_compile` havuzu Jolt derlemesini sınırlar (`ASTRAL_HEAVY_COMPILE_JOBS`, varsayılan çekirdek/2).
- Ara sıra `LNK1104` (antivirüs kilidi) → yeniden build. Silinemeyen worktree klasörleri kilitlidir; git kaydı `git worktree remove` ile temizlenir, klasör sonra elle silinir.
- Testler SPIR-V önbelleğini `<build>/test-shader-cache` altına yazar (`ASTRAL_SHADER_CACHE`, CMake ≥ 3.28).

## Ajanlarla çalışma

- Kod incelemesinde önce bilgi grafiği: `graphify query "<soru>"` (grafik ana checkout'taki `graphify-out/` içinde); kod değişince `graphify update .`.
- Alt ajanlar kendi worktree'lerinde çalışır, yalnız kendilerine verilen dosyalara yazar, merge/push yapmaz. Sonuçları doğrudan kabul edilmez: lider ajan diff'i ve testleri kendisi doğrular.
- Harici ajan CLI'ları (agy, codex…) `external-agents` sarmalayıcısıyla, `-Worktree` ile çalıştırılır; mekanik işler için uygundur, API ayrıntısı içeren içerik mutlaka kaynakla karşılaştırılır.
- Commit kimliği (makinede git kullanıcısı yok): `git -c user.name=Claude -c user.email=noreply@anthropic.com commit ...`; mesaj sonunda `Co-Authored-By` satırı.

## Commit mesajları

`feat(modül): …`, `fix(modül): …`, `perf(alan): …`, `build: …`, `test: …`, `docs: …`. Gövde: neden ve nasıl; ilgili ADR.
