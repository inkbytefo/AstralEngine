# Astral::Editor_core ve Astral::Editor

Bağımsız, AI destekli masaüstü editör çekirdeği ([ADR-0035](../decisions/0035-standalone-ai-assisted-editor-core.md)).

## Katman ve İzolasyon

- **`Astral::Editor_core` (`modules/editor_core`, Katman 3):** `Astral::Core`, `Astral::World`, `Astral::Assets` bağımlılıklarını kullanır. GPU, pencere veya ImGui bağımlılığı yoktur; tüm belge, komut, şema, transaction, undo/redo, audit log ve geçici ECS önizleme testleri GPU'suz koşar.
- **`Astral::Editor` (`modules/editor`, Katman 4):** `Astral::Editor_core`, `Astral::App` ve `Astral::ImGui` (`v1.92.9-docking`) üstüne dockable masaüstü çalışma alanını (`EditorWorkspace`) kurar.
- Oyunlar (`Astral::App`, `astral_add_game`, `templates/blank`, `tests/consumer`) ve dedicated server hedefleri editör modüllerine asla bağlanmaz (`editor_isolation_check` + `tests/consumer` CMake doğrulaması).

## Temel API Özeti

- **Belge ve atomik G/Ç (`astral/editor/document.hpp`):** `DocumentRevision`, `EditorDocument`, `IEditorFileSystem`, `LocalEditorFileSystem` (geçici dosya + flush + atomik `rename`), `load_editor_document`.
- **Ortak sürümlü komutlar (`astral/editor/command.hpp`):** `EditorCommand` (`schema_version = 1`), `build_command_schema(TypeRegistry)`, `command_to_json` / `command_from_json`, `validate_command`, `apply_command` (`create_entity`, `delete_entity`, `add_component`, `remove_component`, `set_component_field`, `reparent_entity`, `instantiate_prefab`, `set_prefab_override`, `save_prefab`).
- **Transaction, Undo/Redo ve Audit Log (`astral/editor/transaction.hpp`):** `diff_documents`, `Transaction`, `AuditLog` (`to_jsonl`), `EditorSession` (eski revizyon reddi, I/O hatasında tam rollback, bayt-bayt deterministik undo/redo).
- **AI Ajan Öneri ve Önizleme (`astral/editor/agent.hpp`):** `IAgentProvider`, `PrefabAssistantAgent`, `run_proposal_preview` (ana belgeye ve canlı dünyaya dokunmadan geçici `astral::World` örneği üzerinde önizleme), `ProposalReview` (`request` → `diff`/`preview` → `approve` / `reject`).
- **Masaüstü Çalışma Alanı (`astral/editor/workspace.hpp`):** `EditorWorkspace` (dockable Viewport, Assets tarayıcı, Hierarchy, Inspector, AI Assistant & Diff Review, Console & Profiler); örnek uygulama: `examples/world_editor`.

## Bilinen Sınırlar

- Dear ImGui `v1.92.9-docking` ile tek pencere içi docking etkin; multi-viewport (`ImGuiConfigFlags_ViewportsEnable`) çoklu pencere ve çoklu swapchain desteği eklenene kadar kapalıdır ([ADR-0035](../decisions/0035-standalone-ai-assisted-editor-core.md)).
- Inspector doğrudan tek-düzeyli skaler/vektör alanları düzenler; iç içe struct ve harita/Level Instance düzenlemesi E1 kapsamındadır.
- Harici CLI/LLM ajan süreç adaptörü için `IAgentProvider` arayüzü ve JSON komut şeması hazırdır; yerleşik örnek `PrefabAssistantAgent` çevrimdışı ve deterministiktir.
