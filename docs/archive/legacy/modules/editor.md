# Astral::Editor_core ve Astral::Editor

Bağımsız, AI-First Native masaüstü editörü ([ADR-0035](../decisions/0035-standalone-ai-assisted-editor-core.md), [ADR-0036](../decisions/0036-world-authoring-and-level-instances.md), [ADR-0037](../decisions/0037-ai-first-native-editor-v2.md)). Kullanıcı rehberi: [docs/editor/README.md](../editor/README.md).

## Katman ve İzolasyon

- **`Astral::Editor_core` (`modules/editor_core`, Katman 3):** `Astral::Core`, `Astral::World`, `Astral::Assets` bağımlılıklarını kullanır. GPU, pencere veya ImGui bağımlılığı **yoktur**; tüm belge, komut, şema, transaction, undo/redo, autosave, audit log, dünya yazarlığı ve asenkron AI görev testleri GPU'suz koşar.
- **`Astral::Editor` (`modules/editor`, Katman 4):** `Astral::Editor_core`, `Astral::App` ve `Astral::ImGui` (`v1.93.0-docking`) üstüne modüler `EditorPanel` kayıt defteri, merkezi `UIStyle` tema token'ları, **Focus / Advanced** yerleşimleri, gerçek offscreen 3D viewport, CPU ray picking, 3D gizmo, izole Play host'u ve komut paletini (`Ctrl+K`) kurar.
- Oyunlar (`Astral::App`, `astral_add_game`, `templates/blank`, `tests/consumer`) ve dedicated server hedefleri editör modüllerine asla bağlanmaz (`editor_isolation_check` + `tests/consumer` CMake doğrulaması).

## Temel API Özeti

- **Belge ve atomik G/Ç (`astral/editor/document.hpp`):** `DocumentRevision`, `EditorDocument`, `IEditorFileSystem`, `LocalEditorFileSystem` (geçici dosya + flush + atomik `rename`), `load_editor_document`.
- **Ortak sürümlü komutlar (`astral/editor/command.hpp`):** `EditorCommand` (`schema_version = 1`), path-traversal koruması, `build_command_schema(TypeRegistry)`, `command_to_json` / `command_from_json`, `validate_command`, `apply_command`.
- **Transaction, Kirli Durum, Sürükleme Birleştirme & Kurtarma (`astral/editor/transaction.hpp`):** `PersistPolicy` (`Immediate` / `ManualSave`), `coalesce_key`, `has_external_conflict()`, `save()`, `.astral_autosave` kurtarma, `diff_documents`, `AuditLog` (`to_jsonl`), bayt-bayt deterministik undo/redo.
- **Asenkron AI Görev Yaşam Döngüsü (`astral/editor/agent.hpp`, `astral/editor/agent_task.hpp`):** `IAgentProvider`, `PrefabAssistantAgent` (çevrimdışı), `CodexCliProvider` (yerel CLI adaptörü), `AgentPolicy`, 7 durumlu `AgentTaskManager` (`Idle`/`Planning`/`Executing`/`Review`/`Blocked`/`Failed`/`Complete`), seçili komut onayı (`approve_selected`), revizyon akışı (`request_revision`) ve izole `PreviewSandbox` 3D önizlemesi.
- **Dünya Kompozisyonu ve Yerleştirme (`astral/editor/world_authoring.hpp`):** `WorldAuthoringSession` (`.world` ve `<harita>.entities/<id>.ent` düzenleme, atomik **Undo/Redo**, cook hücreleri/katmanlar/floating origin, **Instance Override** vs **Edit-in-Context**, `IHeightSampler` ve `PlacementToolDef`).
- **Masaüstü Çalışma Alanı (`astral/editor/workspace.hpp`):** PIMPL tabanlı `EditorWorkspace` (public header'da sıfır ImGui), `WorkspacePreset::Focus` / `Advanced`, `GizmoMode`, `PlayState`, `ViewportPreviewMode::Live` / `CandidateAfter`; hedefler: `AstralEditor` ve `world_editor`.

## Bilinen Sınırlar

- Dear ImGui `v1.93.0-docking` ile tek pencere içi docking etkin ve `astral_editor_layout.ini` ile kalıcıdır; çoklu OS penceresi (`ImGuiConfigFlags_ViewportsEnable`) çoklu swapchain platform desteği eklenene kadar kapalıdır ([ADR-0037](../decisions/0037-ai-first-native-editor-v2.md)).
- CPU ray picking şu an entity dünya sınır kutusu (OBB/AABB) üzerinden çalışır; çok yoğun üçgen hassasiyeti için GPU Object-ID pass ilerleyen aşamada eklenebilir.
- Tam harita alt örneği (`.world`-in-`.world`) ADR-0036 kapsamında dışarıda tutulmuştur.
