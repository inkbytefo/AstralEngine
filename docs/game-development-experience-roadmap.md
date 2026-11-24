# Game Development Experience Roadmap (GDX)

**Plan v1 · 25 Kasım 2026 · Ürün yönü: kullanıcı tarafından onaylandı**  
**Referans taban:** `0a9d32f4e90e4904454d2f4b8c63542d996a016b` (master, M14 atmosphere merge).  
**Durum:** E3–E10 planlı; bu doküman implementasyon veya test sonucu değildir.

Bu yol haritası [motor roadmap'inin](roadmap.md) tamamlayıcısıdır. Motorun M/P/A/N fazları korunur; GDX fazları geliştiricinin o teknolojileri kullanarak oyun üretmesini sağlar. E0/E1/E2 mevcut editör temelleridir; bunların durumunu yeniden yazmaz. M14 merge'i görüldü ancak yeni commit'in tüm yetenekleri bu doküman kapsamında tekrar derlenip doğrulanmadı.

## 1. Ürün vizyonu

Astral, AI-native, data-oriented, modüler ve native C++20 oyun geliştirme ekosistemidir. Geliştirici oyun tasarımının ve değişiklik onayının sahibidir. AI, motoru ve projeyi anlayan alternatif bir çalışma yüzeyidir; zorunlu bağımlılık değildir.

| Ürün katmanı | Görev | Sınır |
|---|---|---|
| Astral Engine | ECS, runtime, Vulkan, physics, audio, scripting, assets | Editör ve AI sağlayıcısına bağımlı olmaz |
| Astral Studio | Proje yönetimi, authoring, viewport, gameplay, play, profiling, packaging | App ve Editor_core hizmetlerini tüketir; runtime paketi editörü linklemez |
| Astral Intelligence | Bağlam, görev, araç keşfi, plan, öneri, test ve insan onayı | Sağlayıcı ve UI ayrıdır; ham/izinsiz mutasyon yapmaz |

Bu isimler ürün katmanı adlarıdır; mevcut CMake target veya namespace'lerini yeniden adlandırma talimatı değildir. Yeni modül ve bağımlılık ancak ADR ve layer check ile eklenir.

**Ürün denklemi:** Niyet → Bağlam → Ajan planı → Değişiklik önerisi → Önizleme/fark → İnsan kararı → Doğrulama → Oynanabilir sonuç.

### Değişmez prensipler

- AI olmadan yeni proje açılabilir, düzenlenebilir, oynanabilir ve paketlenebilir. AI'ın proje üzerinde gerçekleştirebildiği her düzenleme insan tarafından da erişilebilir olmalı; insanın authoring işlemleri güvenli, keşfedilebilir komutlarla AI tarafından da çağrılabilmeli. İzin gerektiren işlemler ve yerel sırlar bu eşitliğe istisnadır.
- Manuel GUI, CLI ve ajan aynı şema, işlem ve doğrulama servislerini kullanır; kopya implementasyon yok. EditorCommand/transaction/undo/redo ve belge revizyonu korunur.
- ECS: component veri, system tek sorumluluklu davranış, feature bağımsız paket, event/component iletişim. Luau önce; ölçümle gerekli sıcak yol C++'a taşınır. Oyun mantığı FixedUpdate ve replay edilebilir girdilerle test edilir. Bkz. [AGENTS.md](../AGENTS.md).
- Proje dosyaları metin tabanlı, taşınabilir, Git dostu ve AI tarafından okunabilir; üretilen cache, kullanıcı layout'u ve sırlar kaynak dosyadan ayrılır.
- Focus varsayılan: baskın 3D viewport, scene tree, bağlamsal inspector, tek aktif agent görevi. Advanced aynı modelleri kullanır; dockable content/world/material/console/profiler panelleri. Klasik, sade, premium görünüm; gösteriş için neon veya panel kalabalığı yok.
- Başarı kanıtı: derleme, test, çalışır oyun, gerçek ekran görüntüsü, ölçüm ve temiz paket. Sadece mockup, prompt veya README'deki ✅ tamamlanma değildir.

## 2. Bugünkü durum ve kanıt sınırı

| Yetkinlik | Mevcut giriş | Ürün açığı |
|---|---|---|
| Standalone editor / E2 | [Editör rehberi](editor/README.md), [entry point](../examples/world_editor/src/main.cpp) | Entry point sabit asset root, workshop.scene ve proje adı kullanıyor; genel proje açma/oluşturma gerekli |
| Oyun şablonu | [templates/blank](../templates/blank/README.md), [CMake](../templates/blank/CMakeLists.txt) | Şablon üretimi, sürüm seçimi, projeyi editörde açma |
| Oyun programlama | [Oyun rehberi](guides/writing-a-game.md), [AGENTS.md](../AGENTS.md), examples/pong | Feature ve script authoring, ortak API keşfi, editör ve gerçek runtime eşlemesi |
| Sahne/world | E1, scene/prefab/world, .ent, level instances, Inspector | Projeye bağlı içerik kataloğu, import ve referans yaşam döngüsü |
| AI | E2 offline ajan, Codex CLI opt-in, EditorCommand[], Before/After ve audit | Oyun kodu, asset, test ve build için denetimli araç protokolü |
| Play | E2 izole World ve PhysicsWorld | Harici oyunun gerçek FeatureSet, Luau, input, HUD ve audio çalıştırması henüz bu incelemede kanıtlanmadı |
| Paket | astral_add_game, astral-cook, .apak | Editör export akışı, bağımsız paket smoke testi |

Kaynaklar [capability matrisi](status/capabilities.md), [ADR-0035](decisions/0035-standalone-ai-assisted-editor-core.md), [ADR-0036](decisions/0036-world-authoring-and-level-instances.md), [ADR-0037](decisions/0037-ai-first-native-editor-v2.md). Bu tablo kaynak/doküman incelemesidir; yeni runtime testi değildir. Belge ile kod çelişirse sabitlenmiş commit'teki kod ve test kanıtı önceliklidir.

## 3. Geliştiricinin uçtan uca yolculuğu (hedef)

1. Astral Studio → New Project / Open Project. Proje adı, yol, motor sürümü, şablon, Luau/C++/Hybrid seçilir. Proje tek başına CMake ile de derlenebilir.
2. Focus viewport'ta başlangıç sahnesi açılır. Elle veya AI ile entity, ışık, kamera, fizik ve prefab düzenlenir; import edilen GLB/materyaller referanslanır.
3. Component + feature + Luau/C++ ile gameplay kurulur. Reflection, feature manifest, üretilmiş API.md ve astral.d.luau tek keşif sözleşmesini oluşturur. Ajan tahmini API uydurmaz.
4. AI önerisi veya manuel değişiklik doğrulanır, önizlenir ve kaydedilir. Geri al/yeniden yap, external change, autosave ve Git diff görünür.
5. Play, gerçek oyun koduyla ayrı bir runtime oturumu başlatır. Pause/Resume/Stop, input focus, console ve profiler çalışır. Stop editör belgesini kirletmez.
6. HUD, ana menü, pause, ayarlar, save/load ve bitiş ekranları eklenir. Oyun döngüsü baştan sona test edilir.
7. Build/Export → release, cook, package → editörden bağımsız exe + apak. Başka dizinde, kaynak ağaç olmadan başlatılır. Sürüm, log ve paket manifesti kaydedilir.

## 4. Fazlar ve bağımlılıklar

| Faz | Kapsam | Ön koşul | Durum |
|---|---|---|---|
| E3 | Project Manager, ProjectContext, templates | E2, M6, M11 | Sıradaki |
| E4 | Asset Browser, import, prefab yönetimi | E3, M5b/c, M6, E1 | Planlı |
| E5 | Gameplay authoring, component/script/feature | E3, E4, F1, M11 | Planlı |
| E6 | AI tool protocol, gameplay/asset/QA, MCP | E3–E5, E2 | Planlı |
| E7 | Gerçek Play-in-Editor / runtime bridge | E3, E5, F1, E6 test araçları | Planlı |
| E8 | UI/HUD, menü ve ayar authoring | E5, E7, M8 | Planlı |
| E9 | Build, cook, export, dağıtım | E3, E4, E7, M6 | Planlı |
| E10 | Tam oynanabilir referans oyun | E3–E9 | Planlı |

E10 oyunu E3'ten itibaren dogfood edilir; final kabulü E10'da. E4/E5/E6 küçük dikey dilimlerle ilerler; E7 runtime prototipi E3'ten sonra bağımsız araştırılabilir. M14–M23/N0 ve GDX paralel ilerler; bitmemiş motor özellikleri şablonlarda yanlış biçimde sunulmaz. A0 sonrası Third Person/Sword, V0 sonrası Vehicle, N0 sonrası multiplayer şablonu doğrulanır.

### E3 — Project Manager ve şablonlar

- Sürümlü proje manifesti ve ProjectContext: kimlik, asset root, başlangıç belgesi, engine version, build profile, game target, feature discovery. Proje kökü kanonik, referanslar göreli; makineye özel ayar ayrı. Manifest adı/formatı ADR ile sabitlenecek.
- New/Open/Recent/Close Project, boş dizin doğrulaması, isim ve yol kontrolleri, okunabilir hata, eski sürüm uyumluluk uyarısı; mevcut dosyaları sessizce overwrite etme.
- Blank şablonu mevcut templates/blank'dan üret; CLI ve GUI aynı generator. First Person ve Physics Sandbox minimal örnekleri E3/E5 arasında ekle; Open World mevcut yetenekleriyle desteklenebilir. Diğer şablonlar capability durumunu açıkça gösterir. Dil seçimi gerçekten üretilen dosyalarla eşleşir.
- Editor entry point'ten workshop sabitlerini ProjectContext'e taşı; workshop bir örnek proje olarak korunur. Önerilen CLI: --project <path>, --document <relative-path>; mevcut --advanced, --capture korunur.
- **Kabul:** GUI ve CLI ile aynı manifestli iki bağımsız oyun üret, projeleri değiştir ve yeniden aç; sahne düzenle/kaydet/yeniden yükle, proje root dışına erişimi reddet, hatalı/missing manifesti anlaşılır göster. Blank oyunu editör dışında configure/build/test/run; en az Windows ve Linux yolu test et. E3 planı: [uygulama planı](superpowers/plans/2026-11-25-e3-project-manager.md).

### E4 — İçerik ve prefab iş akışı

- Asset index, search/filter, tür simgeleri ve thumbnail cache; GLB/glTF, PBR texture, .material, .ashader, ses, scene, world, prefab. UI sadece motorun desteklediği formatları import eder.
- Copy/import, bağımlılık ve collision diagnostics, reimport/hot reload; source asset ve cooked asset ayrımı. Rename/move/delete referans analizi ve onay; bağımlı prefab/scene'leri transaction içinde düzelt veya işlemi engelle.
- Viewport'a sürükle bırak, instantiate, prefab oluştur, override/edit-in-context, revert; world cell/data layer politikası korunur.
- **Kabul:** asset import → instance → materyal değiştir → prefab kaydet → iki sahnede kullan → move/reimport → tüm referanslar geçerli; kayıp dosya, duplicate ve path traversal testleri. Import/thumbnail işlerinin p95'i ölçülür; UI beklemez. Skeletal import A0 doğrulanmadan açılmaz.

### E5 — Gameplay authoring

- Reflection inspector: component ekle/sil, tip ve enum alanları, doğrulama/diagnostics; feature manifest, dependencies ve exported headers görünür.
- Feature wizard: C++/Luau/hybrid, .actions, events, settings, tests; mevcut template ve game.add akışı temel alınır. C++ compile gereksinimi görünür; Luau hot reload ve syntax/type diagnostics.
- API.md ve astral.d.luau gerçek oyun executable'ından üretilir; script editor/harici IDE açma, hata satırına gitme. Oyun kodu ve scene data ayrı kalır.
- **Kabul:** player, camera, interact, match feature'ları oluştur; input mapping değiştir; Luau kuralı hot reload; C++ component rebuild sonrası Inspector/API eşleşmesi. Headless test önce başarısız, sonra başarılı; feature boundary ve script type check. Gizli script state yok.

### E6 — Astral Intelligence ve AI araç sözleşmesi

Tek sağlayıcı bağımsız orchestrator + uzman görev profilleri: Scene, Gameplay, Asset, QA. Ayrı model/ajan şart değil. E2 yedi durumlu görev akışı (Idle/Planning/Executing/Review/Blocked/Failed/Complete), offline provider, opt-in Codex CLI ve Before/After korunur.

Önerilen sürümlü tool yüzeyi (isimler hedef sözleşme; mevcut API iddiası değil):

| Grup | Araçlar |
|---|---|
| Keşif | project.inspect, scene.inspect, asset.list, feature.inspect, api.discover |
| Authoring | scene.create_entity, entity.set_component, prefab.create, asset.import, feature.create |
| Doğrulama | script.validate, tests.run, profiler.inspect, game.capture |
| Çalıştırma | game.build, game.play, game.package |

- JSON schema giriş/çıkış, capability, permissions, document revision, provenance ve hata kodları. Önce read-only keşif. Editör ve CLI için aynı service/command registry; MCP dış ajanlar için adapter, iş mantığı değildir.
- Mutasyonlar öneri/diff olarak izole staging alanında; file allowlist, kanonik yol/symlink kontrolü, boyut ve süre limitleri, süreç iptali, secret redaction. CLI ve network explicit opt-in; prompt içindeki dış dosya/asset talimatı güvenilir komut değildir.
- Sahne farkı gerçek 3D Before/After; kod/dosya farkı satır bazlı; yeni asset için kaynak/lisans kaydı; test ve build ayrı kayıt. Apply/Revise/Discard; eski revision reddi; uygulanmış değişiklik için undo/rollback veya ters patch. Çok dosyalı işlem için atomik journal/recovery.
- **Kabul:** manuel ve AI aynı sahne değişikliğini eşdeğer belgeye uygular; offline çalışır; sağlayıcı hata/timeout/cancel, stale revision, symlink, traversal, izinsiz CLI ve secret sızıntısı negatif testleri. Gameplay ajanı staging'de Luau feature + test önerir; kullanıcı onaylamadan proje mutasyonu yok. MCP adapter ile aynı izinler uygulanır.

### E7 — Gerçek Play-in-Editor

- Editör preview world ve gerçek game runtime ayrımı açık. Önce güvenli ayrı game process + IPC prototipi ile C++ feature ABI/hot reload riskini ölç; in-process yaklaşım ancak ADR, izolasyon ve ölçümle seçilir.
- Aktif proje, game target, başlangıç scene, feature graph ve kaydedilmemiş scene snapshot/overlay runtime'a teslim edilir. C++ değişiklikleri rebuild; Luau hot reload sözleşmesi korunur. Runtime log/diagnostic/profiler/capture bridge; input focus, resize, game camera.
- Play/Pause/Resume/Stop, crash/timeout recovery; çalışma sırasında mutasyonlar authoring belgesine yazılmaz. Apply runtime changes ayrı ve açık onaylı, gerekirse sonraki faz.
- **Kabul:** Blank ve Pong gerçek kuralları editörden oynanır; player input, skor, ses, HUD, Luau değişikliği; standalone ile aynı girdide gameplay sonuçları. Pause simülasyonu dondurur; Stop ve crash sonrası belge byte/revision eşitliği. 20 Play/Stop döngüsünde kaynak sızıntısı kontrolü. Oyun paketi editor/AI kütüphanesi linklemez.

### E8 — Oyun arayüzü ve HUD

- Debug ImGui ile oyun UI'si ayrıdır. Mevcut UiCanvas/SDF text'ten hareketle data-driven game UI: anchors/layout, scale, text/font, image, buttons, focus/navigation, theme tokens, bindings ve input events. Render UI shader yeteneği capability testine bağlı.
- UI authoring preview, farklı çözünürlük ve aspect ratio, keyboard/controller focus; main/pause/settings/win/lose ve HUD şablonları. UI oyun kurallarını kopyalamaz; event/state binding kullanır.
- **Kabul:** 1280x720 ve 1920x1080'de taşmayan menü/HUD; input focus ve pause/resume, ayar persist, keyboard navigation; golden/capture ve UI event testleri. Erişilebilir metin/kontrast ve font fallback kontrolü.

### E9 — Build, cook, package ve export

- Project build profile (Debug/Release, platform, renderer/quality preset); CMake configure/build, astral-cook incremental, .apak, output staging, progress/cancel/log; CLI ve UI aynı pipeline.
- Build manifest: engine/game revision, toolchain, asset hash, paket formatı, sürüm, gerekli runtime bağımlılıkları. Kaynak dosya, credential, editor/AI cache pakete girmez. Paket doğrulama ve temiz hedefte smoke test; hata çıkarsa başarı etiketi yok.
- **Kabul:** aynı oyunun CLI ve Studio paket çıktısı içerik manifestiyle eşleşir; değişen asset incremental cook; iptal/bozuk asset/eksik SDK hata senaryosu; kaynak ağacından bağımsız Windows/Linux release çalışır (platform iddiası yalnız ilgili ortamda kanıtlanır). Paket logları ve screenshot kaydedilir.

### E10 — İlk referans oyun: ASTRAL: TRIALS

Kapsam: single-player first-person fizik bulmaca oyunu. Önerilen geçici isim; hedef motorun ve üretim hattının tamamını kanıtlamak, teknoloji demosu değil bitmiş küçük ürün. Skeletal animation, multiplayer veya araç solver'ı gerektirmez.

- En az üç kısa oda: nesneyi taşı/yerleştir, trigger/kapı, fizik puzzle ve süre/başarı koşulu; tutorial, checkpoint, restart, tamamlanma.
- Main menu → New Game/Continue → oyun → Pause/Settings → Win/Fail → tekrar/çıkış; ses, görsel tutarlılık, input, HUD ve save/load.
- Feature paketleri: player, camera, interaction, puzzle, match/progression, hud, audio. C++ gerektiğinde, oyun kuralları öncelikle Luau. Scene/asset kaynakları versiyonlanır, lisans kayıtları tutulur.
- **Kabul:** yeni geliştirici dokümanla temiz clone'dan oyunu açar, bir sahne ve bir gameplay kuralını hem elle hem AI ile düzenler, Play'de test eder ve release paketini çıkarır. En az bir baştan sona insan playtest, headless kural testleri, save/load ve restart, temiz ortam package smoke, profil raporu ve gerçek ekran görüntüleri. Bug listesinde blocker/critical açık kalmaz. Sonrasında FPS arena; kılıç A0/S1, araç V0, multiplayer N0 kanıtlarıyla.

## 5. Ortak faz kapanışı ve ölçüm

[process.md](process.md) esas alınır: görev dalı/worktree, test-first, inceleme, doküman/ADR/CHANGELOG ve merge. GDX fazı için ek olarak:

- İlgili GUI eyleminin CLI veya servis eşdeğeri ve offline manuel akışı belgelenir.
- Normal, hata, iptal, undo, external change, güvenlik ve kayıp/bozuk veri senaryoları kapsanır.
- Gerçek editör ve oyun görüntüsü, test komutları/çıktısı, commit SHA, platform/toolchain ve bağımlılıklar kaydedilir. Mockup kanıt sayılmaz.
- API.md/astral.d.luau/Inspector/manifest drift testleri, editor isolation ve layer check; ilgili golden, debug/release, TSan, package smoke. Çalıştırılamayan test gerekçesiyle blocked; ölçülmemiş değer sonuç diye yazılmaz.
- UX ölçümleri: clean checkout→first Play, import→viewport, AI proposal→review, Play→ready, Stop→editor, build→package, başarısız görev kurtarma. p50/p95/p99, bellek, CPU/GPU, donanım ve sahne; RTX 3060 referansı ve düşük profil ayrı. İlk fazda baseline çıkar; bütçeler ölçüm sonrası ADR ile belirlenir.
- GDX fazı kapanınca bu belge ve ilgili guide güncellenir; [motor roadmap'i](roadmap.md) yalnız kesişen motor yetenekleri gerçekten değiştiğinde güncellenir. Tamamlanma koşulları sağlanmadan ✅ yok.

## 6. Öncelikli kararlar ve kapsam dışı

İlk teslim E3. E4–E10 planı hazır ancak her faz için kaynak incelemesi, ayrı uygulama planı ve gerekliyse ADR açılacak. GDX için motoru yeniden yazmak, yeni ECS kurmak, Unity/Unreal UI kopyalamak, zorunlu bulut/AI aboneliği ve tüm oyun türlerini bir kerede desteklemek kapsam dışı. Özellikle ARENA gibi geniş oyunlar, TRIALS ile doğrulanan aynı üretim hattını kullanır.

Açık kararlar: manifest adı/schema, process IPC veya in-process PIE, asset identity/rename stratejisi, UI document formatı, MCP transport ve izin modeli. Bunların implementasyonunu bu roadmap sabitlemez; alternatifler ve testle karar verilir.
