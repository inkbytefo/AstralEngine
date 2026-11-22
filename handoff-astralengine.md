# active-memory handoff: AstralEngine (C++20 Vulkan oyun motoru) — v3

**Handoff #1** · 2026-11-22 · Lineage: #1 (2026-11-22): M5c→M8.5 yapıldı (sahneler, paketleme, Pong, profiler, debug çizim, ImGui, HUD, ses), yol haritası v4 + ADR-0017…0024 yazıldı, `v3` dalı push'landı

## 0. Instructions for Claude (read first)

You are continuing work from a previous chat. That chat is gone; this file is the complete context and the source of truth.

1. Read this whole file before replying.
2. Follow sections 3 (Style), 4 (Hard rules) and 5 (Corrections) in every reply, for the rest of this chat. They override your defaults.
3. Use the values in section 8 exactly. Never round, re-estimate, or "correct" them.
4. Do not suggest anything listed in section 7 (Changed / rejected) again unless the user brings it up.
5. Code word: None active. If the user types /amcodeword, start every reply with the phrase they choose (default "Yes Boss!").
6. Your first reply: at most 5 lines covering the goal, the current state, and the next step (section 11). Mention any files from section 13 that were not attached. Ask the questions in section 12 if there are any. End with "Ready to continue with <next step>?" Then wait for the user's go.

## 1. Mission
- **Goal:** AstralEngine'i (kendi C++20 + Vulkan 1.3 oyun motoru) yol haritası v4'e göre geliştirmek: Unreal Engine 5.8 kalitesinde, açık dünyada da yüksek performanslı dünya/harita sistemi; yapay zekâ modelleriyle rahat yazılan, spagettiye dönmeyen modüler oyun kodu + scripting; gdshader benzeri özel shader/materyal dili; CS2 dumanı gibi hacim tabanlı VFX; sonra fizik, arazi, su, hava, yıkım.
- **Done looks like:** her faz `docs/process.md` "Faz bitti" listesiyle kapanır (testler geçer, belgeler güncel, GitHub'a push'lu); her fazın çalışan bir demosu ve görüntü testi olur.
- **Why it matters / context:** "enerjimizi motorun gücüne verelim" — oyunlar (Pong gibi) motoru sınamak için küçük tutuluyor.

## 2. About the user (as relevant to this work)
- Tevfik Poyraz, GitHub: `inkbytefo`. Kendi kendine öğrenmiş, kodlamada çok yeni; projelerini yapay zekâyla yazıyor. Dövme sanatçısı, İstanbul.
- Kendi sözü: "ben koddan falan anlamıyorum kanka" → açıklamalar kod bilmeyen birine göre olmalı.
- Teknik kararları Claude'un vermesini istiyor; tasarım gerekçesini anlamak istiyor.

## 3. Style & communication
- **Language:** Kullanıcıya Türkçe. Repoda: kod/yorumlar İngilizce, `docs/` Türkçe (CLAUDE.md kuralı).
- **Tone:** doğrudan, dolgusuz, analitik; samimi ("kanka" diyebilir).
- **Reply length:** kısa; bir iş bitince sonucu birkaç cümle + gerekiyorsa kısa liste.
- **Formatting:** markdown başlık/liste/tablo serbest; emoji yok (durum işaretleri ✅🚧🔜⬜ yalnızca belgelerde).
- **Working style:** seçenek sunma, karar ver ve uygula; gerekçeyi kısaca anlat. Kullanıcı iş bitmeden yeni istek verebilir/durdurabilir — durdurunca hemen dur ve commit'le.
- **Avoid:** kod dökümü göstermek (kullanıcı kod okumuyor), uzun teknik jargon, "şunu mu yoksa bunu mu istersin" soruları, gereksiz özet tekrarları.

## 4. Hard rules (word for word)
1. "githuba yükle mutlaka" — her faz/iş GitHub'a push edilir (feature branch → sonra ana dala fast-forward; önce `git merge-base --is-ancestor origin/<ana dal> HEAD` kontrolü).
2. "solid prensipleri her zaman" — motor ve oyun kodunda SOLID (bkz. `docs/architecture.md`, ADR-0023).
3. "ai modelleri tarafından scripting güzel olsun kod tabanı ve oyun tarafı spagettiye dönmesin modern modüler olsun"
4. "bu kılıç arenası sadece basit bir placeholder olsun" (kılıç arenası yalnızca yer tutucu; animasyon + aktif ragdoll fazlarından sonra).
5. Repo kuralları (`CLAUDE.md`): TDD (önce başarısız test); `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`; public header'lara üçüncü parti tip, singleton veya global state yok; yeni karar → `docs/decisions/` ADR; her faz sonunda "Faz bitti" dokümanları.
6. "Faz bitti" (docs/process.md): tüm testler (debug + tsan), uyarısız (`ASTRAL_WARNINGS_AS_ERRORS=ON`); roadmap ✅/🔜; modül dokümanı; ADR; CHANGELOG; README durum tablosu.
7. Commit mesajları: `feat(modül): …`, `fix(modül): …`, `build: …`, `docs: …`, `test(modül): …`; sonuna:
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` (ortamın verdiği attribution satırları).

## 5. Corrections log
| # | Claude did | The user wanted |
|---|---|---|
| 1 | M7 olarak kılıç arenası oyunu planladı | "kılıç arenası sadece basit bir placeholder olsun ve onun yerine 3d pong gibi basit bir oyun yapalım enerjimizi motorun gücüne verelim" |
| 2 | Kullanıcı yeni istekler yazarken tam test turunu (ctest, uzun) çalıştırmaya devam etti | "sen durdur ve commitle her şeyi" — dur, mevcut işi commit'le |
| 3 | Teknik/kod ağırlıklı anlatım | "ben koddan falan anlamıyorum" → sade, kod göstermeyen Türkçe açıklama |
| 4 | Belgeleri feature branch'te bıraktı | "herşeyi dokümante et commitle v3 olarak" → `v3` dalı olarak push |

## 6. Decisions
| Decision | Why |
|---|---|
| Yol haritası v4: M9 dünya/harita, M10 `.ashader`, M11 modüler oyun kodu + Luau, M12 fizik alanları, M13 gelişmiş PBR, M14 atmosfer + hacimsel VFX, M15 arazi, M16 su I, M17 hava, M18 su II, M19 SDF + yıkım, M20 animasyon, M21 fizik II, M22 dünya ölçeği II | kullanıcı öncelikleri: dünya sistemi, AI-dostu modüler kod, shader dili, CS2 dumanı |
| Dünya sistemi UE5 World Partition benzeri: `.world` + `<harita>.entities/<id>.ent` (nesne başına dosya), hücre streaming, veri katmanları, bölüm örnekleri (`.scene`), kayan orijin (dosyada double, çalışırken float) — ADR-0021 | çakışmasız düzenleme, açık dünya belleği, büyük koordinatlar |
| Shader dili `.ashader`: GLSL gövdesi + Godot benzeri başlık (`shader_type surface|particle|volume|post|sky|ui`, `render_mode`, `uniform ... : range()/source_color`), `vertex()`/`surface()`; motor raster/RT/gölge/forward varyantlarını üretir; `.material` + ebeveyn zinciri — ADR-0022 | yeni dil ayrıştırıcısı yazmamak, AI/insan GLSL biliyor, raster-RT tutarlılığı |
| Oyun kodu = özellik paketleri (`game/features/<ad>/` + `feature.json` + `install()`), sistemler birbirini çağırmaz (olaylarla), global yok, `AGENTS.md`, reflection'dan `astral.d.luau` + `API.md` — ADR-0023 | spagetti önleme, AI dostu |
| Scripting dili Luau (C++ motor + Luau kurallar) — ADR-0017 | tip sistemi, sandbox, AI'nin iyi bildiği dil |
| Hacimsel VFX: CPU'da deterministik oyun ızgarası (~25 cm voksel, flood fill, duvar bilgisi Jolt raycast ile, mermi/patlama boşaltma, görüş sorgusu) + GPU görüntü hacmi (seyrek 8³ tuğla atlası, raymarch, Beer–Lambert, HG faz, DDGI) + `.vfx` — ADR-0024 | CS2 modeli; oyun kararı GPU'dan okunmaz (determinizm, ağ) |
| Arazi: Terrain3D benzeri heightfield + seyrek SDF hacimleri, yıkım aynı SDF altyapısıyla — ADR-0018 | kullanıcı isteği |
| Ses: miniaudio, sessiz cihaz (headless/offscreen/cihaz yok), `AudioSource`/`AudioListener` — ADR-0020 | |
| Dev araçları: global olmayan profiler, pass başına GPU timestamp, tek 2D katman (`UiLayer`) ImGui+HUD, kendi ImGui arka ucu, SDF font — ADR-0019 | |
| `v3` dalı = belgeler + M8.5 dahil yeni ana hat (Claude'un yorumu; teyit: §12) | "commitle v3 olarak" |

## 7. Changed / rejected
- M7 kılıç arenası → 3D Pong; kılıç arenası yer tutucu (enerji motora).
- Yol haritası v3 (M9 shader, M10 fizik, M11 Luau, M13 atmosfer, M14 arazi … M21) → v4 (§6); faz numaraları birer kaydı, ADR-0018 içindeki numaralar güncellendi.
- Shader işi "`.material` + `surface()` fonksiyonu" → tam `.ashader` dili (gdshader benzeri).
- ❌ Görsel düğüm editörünü ayrı sistem yapmak (ileride yalnızca `.ashader` üreticisi olacak; metin tek doğruluk kaynağı).
- ❌ ImGui'nin resmî Vulkan arka ucu (RHI'yi atlıyor) → kendi arka ucumuz.
- ❌ Sesi oyun kararlarına GPU'dan geri okumak / duman şeklini GPU'da hesaplamak (determinizm).
- ❌ Kullanıcının `#ifdef` shader anahtarları tanımlaması (varyant patlaması).

## 8. Data & facts (exact)
- Repo: `https://github.com/inkbytefo/AstralEngine.git`; bulut ortamında yol `/home/claude/AstralEngine`.
- Proje sürümü (CMakeLists.txt): `project(Astral VERSION 0.12.0 ...)`; M8 bitince 0.13.0 planlı (henüz bump yok; tests/consumer `find_package` sürümüyle uyumlu tutulmalı — önceki bump'ta SameMinorVersion yüzünden güncellenmişti).
- CMake preset'leri: `debug`, `release`, `tsan`, `asan`.
- Dallar (origin): `v3` = `fd16332` = `feat/m8-devtools`; `v2` = `c140ba4`; `main` = `69ccfd7`; `v2-old` = `62c6545`; eski: `docs/roadmap-v3` `86b7472`, `docs/terrain-sdf` `c140ba4`, `feat/m7-pong` `915a233`, `feat/m6-packaging` `98529d1`, `feat/m5c-scenes` `0b5da50`, `feat/m5b-assets` `496d670`, `feat/m5a-ecs` `535befd`, `feat/m3.5-photoreal` `990f778`.

| Commit | İçerik |
|---|---|
| 0c1c2d4 | M8.1 profiler (GPU timestamp/pass, CPU kapsamları) |
| afde8bc | M8.2 debug çizim |
| 4c235c2 | M8.3 debug UI (Dear ImGui 1.92.9, inspector, profiler paneli) |
| 8590a56 | M8.4 HUD metin ve 2D çizim |
| 2b3343d | M8.5 Astral::Audio (miniaudio 0.11.26) + App + Pong sesleri |
| fd16332 | docs: v3 — ADR-0019…0024, yol haritası v4, rehber, modül belgeleri |

- Son tam test sonucu: debug ctest 14/14 geçti (App ses entegrasyonundan sonra, `audio_app_test` ve Pong ses testi eklenmeden önce). Sonra ayrı ayrı geçenler: `astral_app_tests -tc="*AudioSource*"`, `pong_tests`, `astral_audio_tests`. Offscreen→sessiz ses değişikliğinden sonraki **tam** ctest kullanıcı tarafından durduruldu → doğrulanmadı.
- Paketli Pong yakalaması (debug, lavapipe, 240 kare, 640×360) ≈ 2 dk 13 s sürdü; hata yok, sesler `.apak`'tan okundu.
- Pong sesleri: `examples/pong/tools/make_sounds.py` (22050 Hz mono 16-bit, deterministik): paddle, wall, serve, point_won, point_lost, match_won, match_lost, hum (2 s döngü); toplam ~240 KB.
- ADR listesi: 0017 gameplay-scripting, 0018 terrain-and-sdf-world, 0019 dev-tools-and-ui-overlay, 0020 audio, 0021 world-and-level-system, 0022 shader-and-material-language, 0023 modular-game-code, 0024 volumetric-vfx.
- CS2 dumanı araştırması: voksel ≈20–30 cm; tipik bomba ~20×20×20 = 8000 voksel; flood fill 6 yön; mermi yolu vokselleri geçici boşaltır; raymarch + Beer–Lambert + Worley gürültü; ağda aynı tohumla deterministik.
- Teknik tuzaklar (tekrar düşme): `log.error(string)` consteval format hatası → `log.log(LogLevel::Error, msg)`; `ImGui::Indent(0)` varsayılan girinti yapar → depth>0 ise girintile; GCC13 -O2 `-Wstringop-overflow` yanlış alarmı (`insert({..})`) → push_back döngüsü; doctest int literal `-Wconversion` → float literal; designated initializer sırası alan sırasına uymalı; yeni `AppDesc` alanları sona eklenir; App ses senkronu karenin sonunda çalışır (yeni `AudioSource` ilk karede `on_update`'te görünmez); offscreen App gerçek ses cihazı açmaz (sessiz).

## 9. People, terms & names
- **People:** Tevfik (inkbytefo): sahip, tek geliştirici (AI ile).
- **Terms & nicknames:** "githuba yükle" = push; "kılıç arenası" = sword arena placeholder; "Curveball" = Pong'un adı; "roadmape başla" = sıradaki fazı uygula; M8.x = M8 alt adımları.
- **Names in use:** modüller `modules/{core,platform,gpu,render,world,physics,audio,assets,app}`; `Astral::ImGui`, `Astral::Audio`; `ctx.profiler`, `ctx.debug`, `ctx.debug_show`, `ctx.show_debug_ui`, `ctx.hud`, `ctx.font`, `ctx.screen_size()`, `ctx.audio`, `ctx.sound(path)`; `AppDesc::debug_ui`, `AppDesc::audio`; `AssetManager::read`; `Renderer::gpu_timings()`; `RenderScene::debug_lines`, `RenderScene::ui`; Pong: `pong::Court`, `pong::Paddle`, `pong::Ball`, `pong::ScorePip`, `pong::DepthMarker`, `pong::Sounds`, olaylar `pong::Scored`, `pong::Bounced{On::Serve|Wall|Paddle}`, sistem `MatchAudio`; hedefler `pong`, `pong_package`, `pong_tests`; `astral-cook`, `.apak`, `astral_add_game`; dosya biçimleri: `.scene`, `.prefab`, (planlı) `.world`, `.ent`, `.ashader`, `.material`, `.vfx`, `.aterrain`.

## 10. Work state
| Item | Status | Version / location | Notes |
|---|---|---|---|
| M0–M7 | final | v2 / v3 | tamamlandı |
| M8.1–M8.5 | final (commit'li) | `feat/m8-devtools` = `v3` @ fd16332 | tam ctest yeniden koşulmalı |
| M8.6 | not started | — | golden image test altyapısı (3 sahne, tolerans + fark görüntüsü), TSan + release `-Werror`, sürüm 0.13.0, bağımsız inceleme, roadmap M8 ✅ / M9 🔜 |
| Yol haritası v4 + ADR-0019…0024 | final | `docs/roadmap.md`, `docs/decisions/` | |
| Rehber "sahne, harita, dünya" | final | `docs/guides/scenes-and-worlds.md` | kod bilmeyen için |
| Rehber "oyun nasıl yazılır" | eski | `docs/guides/writing-a-game.md` | HUD/ses/debug UI ve ADR-0023 düzeni eklenmeli |
| M9 dünya/harita sistemi | 🔜 | ADR-0021 | plan dosyası `docs/superpowers/plans/<tarih>-m9.md` yazılacak |

## 11. Next steps
1. **Next action:** repoyu `v3` dalında aç, `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` ile tam test turunu koş, kırılan varsa düzelt.
2. M8.6: golden image testleri (offscreen, lavapipe; 3 sahne: scene_viewer showcase, pong, rt_showcase), tsan + release `-Werror`, sürüm 0.13.0 (consumer testini de güncelle), CHANGELOG `[0.13.0]`, roadmap M8 ✅ / M9 🔜, README; `feat/m8-devtools` push + `v3` fast-forward.
3. `docs/guides/writing-a-game.md`'yi HUD, ses, debug UI ve özellik paketi düzeniyle güncelle.
4. M9 (ADR-0021): plan yaz → `feat/m9-world` → TDD ile `.world` + `.ent` biçimi, cook'ta hücreleme, asenkron streaming, veri katmanları, kayan orijin; çıkış demosu 4×4 km test dünyası.

## 12. Open questions ⚠️
- Ana dal artık `v3` mü? (Claude'un varsayımı: evet, bundan sonra fazlar `v3`'e fast-forward edilir; `v2` dondurulur.) Seçenekler: v3 yeni ana hat / v2'ye de merge et / skip, varsayımla devam.
- Claude Code yerel makinede çalışacaksa: bağımlılıklar (Vulkan SDK 1.3, SDL3, Jolt — `find_dependency` listesi) kurulu mu? Değilse README'deki derleme bölümüne bakılmalı.

## 13. Re-attach checklist
- Nothing to attach. Her şey GitHub'da: `git clone https://github.com/inkbytefo/AstralEngine.git && git checkout v3` (bu dosya da repo kökünde: `handoff-astralengine.md`).

---
<sub>Audit: 13/13 sections · 7 rules · 4 corrections · 30 data points · secrets removed: none found · generated by active-memory</sub>
