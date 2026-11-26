# Editor (`Astral::EditorCore`, `Astral::Editor`)

> **Olgunluk:** `editor_core` Kullanılabilir · `editor` (UI) Deneysel · **Bağımlılıklar:** `editor_core`: Astral::Core, World, Assets (`modules/editor_core/CMakeLists.txt:8-10`);
> `editor`: Astral::Core, World, Assets, App, Editor_core, ImGui (`modules/editor/CMakeLists.txt:14-19`). Gerçek CMake takma adı `Astral::Editor_core` (ve `Astral::Editor`):
> `astral_add_module` ilk harfi büyütür (`cmake/AstralModule.cmake:10-13`); belge başlığındaki `Astral::EditorCore` adı bir CMake hedefi değildir. ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

İki ayrı modül, tek amaç: Astral sahnelerini (`.scene`/`.prefab`/`.world`/`.ent`) düzenleyen, yapay zekâ önerilerini insan onayından geçiren bağımsız bir masaüstü editörü.
- **`Astral::Editor_core`** (katman 3): GPU/pencere/ImGui'den bağımsız belge modeli, sürümlü komutlar, işlem (transaction), geri al/yinele, otomatik kayıt, denetim günlüğü, yapay zekâ öneri yaşam döngüsü ve `.world` yazarlığı.
- **`Astral::Editor`** (katman 4): bunların üstünde Dear ImGui ile çalışma alanı: sahne ağacı, Inspector, canlı ekran-dışı 3B viewport, seçme/gizmo, "Play" önizlemesi, komut paleti.

**Ne değildir:** Bu bir proje yöneticisi, derleme/paketleme aracı ya da gerçek oyunu çalıştıran bir stüdyo değildir (aşağıda "Gerçek durum ve plan"). Oyunlar ve sunucular bu modüllere bağlanmaz
(`tools/check_editor_isolation.py`, CTest `editor_isolation_check`: `../../CMakeLists.txt:95-97`; `tests/consumer/CMakeLists.txt:26-38`). Çalıştırılabilir editör `examples/world_editor` altındadır
(`AstralEditor` ve `world_editor` aynı kaynaktan iki hedef: `examples/world_editor/CMakeLists.txt:2-10`).

## Gerçek durum ve plan

Eski `docs/archive/legacy/editor/studio-intelligence-target-architecture.md` belgesi bir **hedef tasarımdır**, uygulama değildir (kendi ifadesiyle "implementasyon, onaylanmış ADR ya da çalışma zamanı doğrulaması değildir").
Aşağıdaki tablo kod incelemesine göre neyin var olduğunu ayırır.

| Alan | Durum | Kanıt |
|---|---|---|
| Belge modeli, revizyon+hash, atomik dosya yazma | **Var** | `modules/editor_core/include/astral/editor/document.hpp:17-68` |
| Sürümlü komutlar (9 tür), şema keşfi, doğrulama | **Var** | `modules/editor_core/include/astral/editor/command.hpp:19-60` |
| İşlem, bayat revizyon reddi, geri al/yinele (anlık görüntü tabanlı), sürükleme birleştirme, çakışma, otomatik kayıt, denetim günlüğü | **Var** | `modules/editor_core/include/astral/editor/transaction.hpp:36-139` |
| Yapay zekâ öneri → izole önizleme → insan onayı; 7 durumlu asenkron görev; seçmeli onay; politika kapısı | **Var** | `modules/editor_core/include/astral/editor/agent.hpp:69`, `modules/editor_core/include/astral/editor/agent_task.hpp:19-144` |
| Çevrimdışı "ajan" | **Var ama sahte zekâ:** üç anahtar sözcüğe bakan sabit bir şablon | `modules/editor_core/src/agent.cpp:59-113` |
| Codex CLI bağdaştırıcısı | **Var:** yerel `codex` süreci, `popen` ile | `modules/editor_core/src/agent_task.cpp:132-160` |
| `.world`/`.ent` yazarlığı, Level Instance override/edit-in-context, yerleştirme araçları, cook önizlemesi | **Var** | `modules/editor_core/include/astral/editor/world_authoring.hpp:75-129` |
| ImGui çalışma alanı, viewport, picking, gizmo, Play, komut paleti | **Var, test kapsamı zayıf** | `modules/editor/include/astral/editor/workspace.hpp:38` |
| Proje kavramı (manifest, proje açma/kapatma, birden çok belge sekmesi) | **Yok** | `ASTRAL_EDITOR_ASSETS` derleme zamanı sabiti; komut satırından proje yolu yok (`examples/world_editor/src/main.cpp:44-51`) |
| Gerçek oyunu çalıştıran Play (FeatureSet, Luau, ses, süreç dışı) | **Yok** | `PlayHost` yalnızca RigidBody fiziği + render (`modules/editor/src/services/play_host.cpp:13-103`) |
| Çok dosyalı işlem günlüğü / çökme kurtarma paketi | **Yok** (tek dosya yan dosyası var) | `modules/editor_core/src/transaction.cpp:199-205` |
| Derleme/cook/paketleme servisi, özellik sihirbazı, asset tarayıcı (bağımlılık grafiği) | **Yok** | kodda karşılığı yok |
| Terrain/malzeme/animasyon/UI yazarlığı | **Yok** | `modules/editor*` altında terrain/malzeme düzenleyici yok (Inspector genel JSON düzenleyicidir) |
| Sağlayıcıdan bağımsız "Intelligence" orkestratörü, araç kaydı, MCP, API sağlayıcıları | **Yok** | yalnızca `IAgentProvider` arayüzü + 2 sağlayıcı |
| "Astral Studio" yürütülebilir dosyası, çok pencere | **Yok** | tek pencere, `AstralEditor` = `world_editor` |

## Public API

### `editor_core` (`astral::editor` ad alanı)

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `DocumentRevision{number, hash}` | Bayat komut/öneriyi reddetmek için belge sürümü | `modules/editor_core/include/astral/editor/document.hpp:17` |
| `IEditorFileSystem`, `LocalEditorFileSystem` | Okuma, **atomik** yazma (geçici dosya + rename), dosya listeleme; testte hata enjeksiyonu için sanal | `modules/editor_core/include/astral/editor/document.hpp:32`, `modules/editor_core/include/astral/editor/document.hpp:43` |
| `EditorDocument`, `load_editor_document` | JSON belge + yol + revizyon; kayıt bulma/değiştirme/sonraki id | `modules/editor_core/include/astral/editor/document.hpp:52`, `modules/editor_core/include/astral/editor/document.hpp:68` |
| `EditorCommand`, `CommandKind` | `CreateEntity, DeleteEntity, AddComponent, RemoveComponent, SetComponentField, ReparentEntity, InstantiatePrefab, SetPrefabOverride, SavePrefab` | `modules/editor_core/include/astral/editor/command.hpp:35`, `modules/editor_core/include/astral/editor/command.hpp:19` |
| `build_command_schema`, `validate_command`, `apply_command`, `command_to/from_json` | Keşfedilebilir şema, doğrulama (bilinmeyen bileşen, alan türü, güvenli göreli yol), uygulama | `modules/editor_core/include/astral/editor/command.hpp:51`, `modules/editor_core/include/astral/editor/command.hpp:54`, `modules/editor_core/include/astral/editor/command.hpp:58` |
| `EditorSession` | Açık belge: `apply(Transaction)`, `undo/redo`, `save`, `reload`, `PersistPolicy` (`Immediate`/`ManualSave`), harici değişiklik çakışması, `.astral_autosave` yan dosyası | `modules/editor_core/include/astral/editor/transaction.hpp:79` |
| `Transaction`, `TransactionResult`, `DocumentDiff`, `diff_documents` | Yazar, özet, beklenen revizyon, `coalesce_key`; alan düzeyinde fark | `modules/editor_core/include/astral/editor/transaction.hpp:36`, `modules/editor_core/include/astral/editor/transaction.hpp:44`, `modules/editor_core/include/astral/editor/transaction.hpp:29` |
| `AuditLog`, `AuditEntry` | `apply/undo/redo/reject/rollback` kayıtları, `to_jsonl()` | `modules/editor_core/include/astral/editor/transaction.hpp:66` |
| `IAgentProvider`, `AgentProposal`, `PrefabAssistantAgent` | Sağlayıcı arayüzü; çevrimdışı şablon ajan | `modules/editor_core/include/astral/editor/agent.hpp:36`, `modules/editor_core/include/astral/editor/agent.hpp:27`, `modules/editor_core/include/astral/editor/agent.hpp:46` |
| `run_proposal_preview`, `ProposalReview` | Öneriyi yalıtılmış geçici `World`'de önizler; onayda tek atomik işlem | `modules/editor_core/include/astral/editor/agent.hpp:66`, `modules/editor_core/include/astral/editor/agent.hpp:69` |
| `AgentTaskManager`, `AgentTaskState`, `AgentPolicy`, `CodexCliProvider` | Asenkron görev (`Idle/Planning/Executing/Review/Blocked/Failed/Complete`), seçmeli onay, revizyon isteği, politika (dış komut ve ağ **varsayılan kapalı**) | `modules/editor_core/include/astral/editor/agent_task.hpp:83`, `modules/editor_core/include/astral/editor/agent_task.hpp:19`, `modules/editor_core/include/astral/editor/agent_task.hpp:32`, `modules/editor_core/include/astral/editor/agent_task.hpp:62` |
| `WorldAuthoringSession`, `PlacementToolDef`, `IHeightSampler` | `.world` oluştur/aç, `.ent` yaz, Level Instance yerleştir/override/edit-in-context, araç ile yerleştirme (yol/spline), cook ve hücre sahipliği önizlemesi, kendi geri al/yinele | `modules/editor_core/include/astral/editor/world_authoring.hpp:75`, `modules/editor_core/include/astral/editor/world_authoring.hpp:47`, `modules/editor_core/include/astral/editor/world_authoring.hpp:26` |

### `editor` (tek public başlık, ImGui türü yok)

`EditorWorkspace : Game` (`modules/editor/include/astral/editor/workspace.hpp:38`) PIMPL'dir: yaşam döngüsü (`on_start/on_update/on_debug_ui/on_shutdown`), programatik iş akışı API'si
(`open_document`, `open_world_map`, `reload_live_scene`, `save_document`, `request_ai_proposal`, `approve_proposal`, `reject_proposal`, `undo/redo`, seçim, `pick_entity_at`, `focus_selected`, gizmo modu/uzay/yakalama,
`enter_play_mode`/`toggle_pause_play_mode`/`stop_play_mode`, `set_viewport_preview_mode`) ve tanılama erişimcileri (`session()`, `world_authoring()`, `review()`, `agent_tasks()`, `console_lines()`)
(`modules/editor/include/astral/editor/workspace.hpp:45-107`). `EditorWorkspaceDesc` alanları: `asset_root`, `initial_document`, `project_name`, `initial_preset` (`Focus`/`Advanced`), `manual_save`
(`modules/editor/include/astral/editor/workspace.hpp:27-33`). Dahili: panel kayıt defteri, tema belirteçleri, `PlayHost`, görünüm matematiği (`modules/editor/src/`; yüklenmez).

## Nasıl çalışır

### Belge ve komut hattı

Editör sahne dosyasını **ham JSON** (`Json`) olarak tutar; ECS `World`'ü yalnızca onun bir izdüşümüdür. Her düzenleme (Inspector, gizmo, sahne ağacı, yapay zekâ onayı) bir `EditorCommand` listesinden oluşan `Transaction`'dır
ve `EditorSession::apply` içinden geçer (`modules/editor_core/src/transaction.cpp:234-305`): bayat revizyon denetimi → aday kopya üzerinde her komutu doğrula+uygula → (`Immediate` ise) önce staged prefab'lar, sonra belge **atomik** yazılır,
hata olursa geri alınır → fark hesaplanır → revizyon artar → anlık görüntü (snapshot) yığınına eklenir → denetim kaydı. Ardışık sürüklemeler aynı `coalesce_key` ile tek geri alma adımına birleşir
(`modules/editor_core/src/transaction.cpp:291-299`). **Geri alma, belgenin tam JSON kopyalarına** dönmektir (`restore_snapshot`: `modules/editor_core/src/transaction.cpp:307-339`), komut tersleme yoktur.
`ManualSave` politikasında değişiklikler bellekte kalır, `Ctrl+S` (`EditorSession::save`) kaydeder; harici değişiklik algılanırsa kayıt reddedilir ve arayüz "External File Change" penceresi açar (`modules/editor_core/src/transaction.cpp:166-190`;
`modules/editor/src/shell/editor_shell.cpp:348`). Kirli belge 30 sn'de bir `<belge>.astral_autosave` yan dosyasına yazılır (`modules/editor/src/workspace.cpp:145-151`).

### Yapay zekâ döngüsü

`AgentTaskManager::submit` önce politika kapısına bakar (dış süreç gerektiren sağlayıcı ve `allow_external_commands == false` ise `Blocked`: `modules/editor_core/src/agent_task.cpp:211-219`), sonra belge anlık görüntüsü
+ komut şemasıyla bir `AgentContext` kurar ve sağlayıcıyı **ayrı iş parçacığında** çağırır; `poll()` ana iş parçacığında sonucu `Review` durumuna taşır (`modules/editor_core/src/agent_task.cpp:183-256`). Öneri, `run_proposal_preview` ile geçici bir
`World`'e uygulanır; arayüz "Önce/Sonra" 3B önizlemesi için viewport'un `set_render_world`'üne bu dünyayı verir (`modules/editor/src/workspace.cpp:162-170`). Onay (`approve_all`/`approve_selected`) tek bir atomik işlemdir.
`CodexCliProvider`, belgenin tamamını ve komut şemasını bir istem dosyasına yazar ve `codex exec --skip-git-repo-check - < dosya` komutunu `popen` ile kabukta çalıştırır; çıktıdan düz ya da ```` ``` ````-çitli JSON'u ayrıştırır
(`modules/editor_core/src/agent_task.cpp:132-160`).

### Viewport, seçim, Play

Viewport, `Context::viewport()` (App'in `ViewportService`'i) ile sahneyi bir `TextureHandle`'a çizer; ImGui bunu panel olarak gösterir (`modules/app/include/astral/app/viewport_service.hpp:14`).
Canlı dünya, belgenin her değişiminde **tamamen yeniden kurulur**: kamera dışındaki tüm varlıklar silinir ve sahne yeniden yüklenir (`sync_scene_to_world_live`: `modules/editor/src/workspace.cpp:88-108`).
Seçim, her varlığın dönüşüm matrisinden türetilen bir eksen-hizalı kutuya ışın atar (kutunun yarı boyutu en az 0,25 m; mesh/model sınırı kullanılmaz: `modules/editor/src/viewport/viewport_math.cpp:40-48`, `modules/editor/src/viewport/viewport_math.cpp:90-105`).
Gizmo sürükleme her kare bir birleştirilmiş işlem uygular (`modules/editor/src/panels/viewport_panel.cpp:16-23`, `modules/editor/src/panels/viewport_panel.cpp:76-96`).
"Play", belgenin (kaydedilmemiş düzenlemeler dahil) kopyasını **ayrı** bir `World` + `PhysicsWorld`'e yükler, yalnızca `RigidBody`'leri fiziğe ekler, 60 Hz'de adımlar ve render dünyasını ona çevirir
(`modules/editor/src/services/play_host.cpp:13-103`). Betik, FeatureSet, ses, karakter denetleyici, eklem ve alan sistemleri **çalışmaz**: gerçek oyun değil, fizik önizlemesidir.

## Güçlü yanlar

- **`editor_core` tamamen GPU'suz ve testli.** Belge, komut, işlem, geri al, otomatik kayıt, yapay zekâ döngüsü ve dünya yazarlığı başsız testlerle koşar; `check_editor_isolation.py` bu bağımsızlığı (`gpu`, `platform`, `render`, `app`, ImGui yok) CI'da zorlar.
- **Tek mutasyon yolu.** İnsan arayüzü ve yapay zekâ aynı komut/işlem hattını kullanır; her değişiklik revizyon denetimli, farkı hesaplanmış, denetim günlüğüne yazılmış bir işlemdir.
- **Yazma güvenliği.** Atomik yazma (geçici dosya + rename), hata enjeksiyonlu dosya sistemi arayüzü, bayat revizyon reddi, harici değişiklik çakışma denetimi, güvenli göreli yol doğrulaması (`modules/editor_core/src/command.cpp:65-74`).
- **İnsan onayı yapıdan gelir.** Yapay zekâ doğrudan belgeyi/diski değiştiremez; yalnızca öneri üretir, önizleme yalıtılmış dünyadadır, dış süreç ve ağ varsayılan kapalıdır (`modules/editor_core/include/astral/editor/agent_task.hpp:32-37`).
- **Eşitlik testi.** Önerinin onaylanması, canlı yeniden yükleme ve geri al/yinele sonrası belgenin bayt bayt aynı olması bir kabul testiyle sabitlenmiş (`examples/world_editor/tests/world_editor_test.cpp:36`).
- **Dünya yazarlığı ayrıntılı.** Level Instance için "instance override" ile "edit-in-context" ayrımı, 2×2 km harita testiyle cook eşitliği ve hücre boşaltma/yükleme sonrası referans korunumu denenmiş
  (`examples/world_editor/tests/world_authoring_e1_test.cpp:56`).

## Zayıf yanlar ve bilinen sınırlar

- **"GPU'suz" doğrudan bağımlılık düzeyindedir.** `editor_core` Astral::Assets'e bağlıdır ve Assets, Astral::Render'a PUBLIC bağımlıdır (`modules/assets/CMakeLists.txt:53-56`); bu nedenle `editor_core` ikilisi dolaylı olarak render/gpu modüllerini link eder. Kod GPU'yu kullanmaz ve `check_editor_isolation.py` yalnızca doğrudan kenarlara bakar (`tools/check_editor_isolation.py:30-33`); geçişli bağımlılık denetlenmez.
- **Arayüz kodu otomatik testlerde çalışmaz.** `world_editor_tests` `headless` App kullanır (`examples/world_editor/tests/world_editor_test.cpp:98`, `examples/world_editor/tests/world_editor_test.cpp:189`); `editor_core` testleri ise App/ImGui kullanmaz. Başsızda ImGui kurulmadığı için `on_debug_ui`, paneller, gizmo sürükleme,
  komut paleti, kısayollar ve Inspector hiç çalıştırılmaz. Yalnızca programatik API (`pick_entity_at`, `enter_play_mode`, `set_viewport_preview_mode`…) denenir. Görsel doğrulama elle `AstralEditor --capture` ile yapılır (`examples/world_editor/src/main.cpp:57-62`).
- **Editör tek sabit demo projeyi açar.** Varlık kökü derleme zamanı `ASTRAL_EDITOR_ASSETS` (= `examples/world_editor/assets`), başlangıç belgesi `scenes/workshop.scene`; komut satırında proje yolu yok (`examples/world_editor/src/main.cpp:44-55`).
  Kendi oyununuzun asset klasörünü açmak için kaynağı değiştirmek gerekir.
- **Play gerçek oyun değildir.** Betikler, `FeatureSet`, ses, karakter, eklem, alan ve atmosfer çalışmaz; yalnızca `RigidBody` gövdeleri 60 Hz'de adımlanır; hareketsiz (`Static`) gövdeler dahil değildir, dönüşüm yazımı yalnızca dinamik/kinematik izleme listesinde
  (`modules/editor/src/services/play_host.cpp:47-56`, `modules/editor/src/services/play_host.cpp:83-103`). Kamera yoksa editör kamerası kopyalanır.
- **Her düzenleme tüm sahneyi yeniden kurar ve tüm belgeyi kopyalar.** `sync_scene_to_world_live` her başarılı işlemden sonra tüm varlıkları siler ve yeniden yükler (`modules/editor/src/workspace.cpp:88-108`); gizmo/slider sürüklemesi her karede bunu tetikler
  (`modules/editor/src/panels/inspector_panel.cpp:221-224`, `modules/editor/src/panels/viewport_panel.cpp:76-96`). Geri alma yığını tam JSON kopyalarıdır ve **sınırsızdır** (üst sınır yok: `modules/editor_core/src/transaction.cpp:297-298`; `AgentTaskManager` geçmişi de sınırsız: `modules/editor_core/include/astral/editor/agent_task.hpp:137`).
  Büyük sahnelerde ölçeklenmesi ölçülmedi.
- **Geri alma prefab dosyalarını geri almaz.** `restore_snapshot` yalnızca belgeyi geri yükler; `SavePrefab` ile diske yazılmış prefab dosyası geri alma sonrası diskte kalır (`modules/editor_core/src/transaction.cpp:307-339`; `Immediate` politikada
  yazma `modules/editor_core/src/transaction.cpp:273-279`). Başarısız bir kayıtta, o işlemde yeni oluşturulan prefab dosyaları silinmez: geri sarma yalnızca önceki içeriği olan dosyaları geri yazar (`modules/editor_core/src/transaction.cpp:45-49`).
  Hedef mimari belgesi bunu "A02/A03" olarak listeler ve açık sayar; çok dosyalı işlem günlüğü yoktur.
- **Silme ve yeniden ebeveynleme tutarlılık denetlemez.** `DeleteEntity`, silinen varlığa başka kayıtlardan yapılan `astral::Parent`/varlık başvurularını temizlemez; `ReparentEntity` yalnızca "kendine ebeveyn" durumunu reddeder, dolaylı döngü
  (A, B'nin ebeveyni iken B'yi A'nın ebeveyni yapmak) denetlenmez (`modules/editor_core/src/command.cpp:238-241`, `modules/editor_core/src/command.cpp:269-280`; bu durumlar için test yok: `editor_core/tests` altında `reparent`/döngü araması boş; kod okumasından çıkarım, yeniden üretilmedi).
- **Çevrimdışı ajan bir model değil, şablondur.** Metinde "move/lift/raise" geçer ve varlık seçiliyse seçili varlığı `position = (0, 2, 0)` yapar (özet metni "by +1m" der ama komut mutlak konum yazar, göreli değil: `modules/editor_core/src/agent.cpp:70-85`);
  aksi halde her zaman aynı "AI Crate" prefab'ını önerir (`modules/editor_core/src/agent.cpp:87-111`). Hedef belge de bunu "relative transform" eksikliği olarak anar (A08).
- **`CodexCliProvider` sağlam bir süreç yönetimi sunmaz.** Komut `popen` ile kabuğa verilir (kaçışsız dize birleştirme: `modules/editor_core/src/agent_task.cpp:147`); zaman aşımı yok; çıktı sınırsız toplanır (`modules/editor_core/src/agent_task.cpp:44-69`);
  istem dosyası sabit adlı geçici dosyadır (`astral_codex_prompt_<n>.txt`) ve tüm belge istemde gönderilir (`modules/editor_core/src/agent_task.cpp:30-40`, `modules/editor_core/src/agent_task.cpp:141-146`). Yürütülebilir adı kodda `"codex"` sabitidir (`modules/editor/src/editor_state.hpp:120`).
- **Görevi iptal etmek arayüzü bloklayabilir.** `AgentTaskManager::cancel()` bayrağı ayarlar ve ardından işçi iş parçacığına `join` yapar (`modules/editor_core/src/agent_task.cpp:313-315`); işçi, `propose` içinde zaman aşımsız çalışan bir dış süreçte bekliyorsa çağıran iş parçacığı
  (editörde ana iş parçacığı) süreç bitene dek takılır. `on_shutdown` da `cancel()` çağırır (`modules/editor/src/workspace.cpp:185-191`). Kod okumasından çıkarım; yeniden üretilmedi.
- **Seçme yaklaşık.** Isı: dönüşüm matrisinden türeyen kutu, gerçek geometri değil; üçgen duyarlılıklı ya da GPU ID-pass seçimi yok (`modules/editor/src/viewport/viewport_math.cpp:40-48`). Prefab parçaları belge kimliğinde (`record/part_path`) var, ama `pick_world_entity` `part_path` boş bir kimlik döner (`modules/editor/src/viewport/viewport_math.cpp:101`).
- **Tek OS penceresi.** ImGui çoklu viewport bayrağı hiç ayarlanmıyor (kodda `ViewportsEnable` yok); yerleşim `astral_editor_layout.ini` dosyasına yazılır (`examples/world_editor/src/main.cpp:48`). Bu dosya çalışma klasörüne yazılır.
- **Boyut bütçesi sınırında.** `SceneTreePanel::draw` 75/80, `AgentTaskManager::submit` 73/80, `apply_command` 72/80, `EditorSession::apply` 72/80 satır (`python tools/check_sizes.py` `[NEAR]`).
- **README/CHANGELOG başarıyı "✅ E2" yazar;** bu, yukarıdaki sınırlarla birlikte okunmalıdır. Hedef mimari (Studio/Intelligence) planlıdır ve henüz kod içermez.

## Testler

| Test | Kapsam | Çalışma |
|---|---|---|
| `modules/editor_core/tests/editor_core_test.cpp` (5 `TEST_CASE`) | komut şeması ve JSON gidiş-dönüş; bayat revizyon + kısmi yazma hatasında geri sarma; yapay zekâ önerisi izole önizleme → atomik onay → bayt-bayt geri al/yinele; ManualSave (kirli durum, sürükleme birleştirme, harici çakışma, otomatik kayıt, yol güvenliği); `AgentTaskManager` politikası, asenkron inceleme, seçmeli onay, revizyon, Codex JSON ayrıştırma (çevrimdışı) | başsız, GPU'suz |
| `modules/editor_core/tests/world_authoring_test.cpp` (2) | `.world`/`.ent`/veri katmanı/yerleştirme araçları; instance override ile edit-in-context ayrımı | başsız, GPU'suz |
| `examples/world_editor/tests/world_editor_test.cpp` (2) | uçtan uca: öneri → onay → canlı yeniden yükleme → geri al/yinele; E2: Focus ön ayarı, ManualSave, picking, izole Play, asenkron öneri + Önce/Sonra | `headless` App; yürütülebilir `world_editor_tests` |
| `examples/world_editor/tests/world_authoring_e1_test.cpp` (1) | 2×2 km harita, iki Level Instance, yerleştirme, override/edit-in-context, cook eşitliği, hücre boşaltma/yükleme | `headless` App |
| `tools/check_editor_isolation.py` | çalışma zamanı modüllerinin editöre, `editor_core`'un GPU/ImGui'ye bağımlı olmaması | CTest `editor_isolation_check` |

**Kapsanmayanlar:** tüm ImGui arayüzü (paneller, gizmo etkileşimi, kısayollar, komut paleti, modallar); `CodexCliProvider::propose` (gerçek süreç, yalnızca `parse_response` çevrimdışı test edilir); `cancel()` bloklaması; silme/ebeveynleme tutarlılığı;
geri almanın prefab dosyalarına etkisi; büyük sahne ölçeği. Bu PC'de ctest durumu (ana ajan): tüm testler geçer (7 `golden_hw_*` dışında; editörle ilgili değil).

## Performans

Ölçülmedi. (Her düzenlemede tam sahne yeniden kurulumu ve tam belge kopyası olduğu için sahne büyüklüğüne doğrusal bir maliyet beklenir; sayı yok.)

## Kararlar

- [ADR-0035 Bağımsız yapay zekâ destekli editör çekirdeği](../decisions/0035-standalone-ai-assisted-editor-core.md)
- [ADR-0036 Dünya yazarlığı ve Level Instance](../decisions/0036-world-authoring-and-level-instances.md)
- [ADR-0037 Yapay zekâ öncelikli yerel editör v2](../decisions/0037-ai-first-native-editor-v2.md)
- [ADR-0034 Gameplay API](../decisions/0034-gameplay-api-and-physics-step-contract.md) (`DocumentEntityId`)
- [ADR-0030 Refaktör kuralları ve boyut bütçesi](../decisions/0030-refactor-rules-and-size-budget.md)
- Plan (uygulama değil): `docs/archive/legacy/editor/studio-intelligence-target-architecture.md`; kullanım rehberi (eski): `docs/archive/legacy/editor/README.md`.
