# ADR-0035: Bağımsız AI Destekli Masaüstü Editör Çekirdeği (E0)

**Tarih:** 2026-11-23 · **Durum:** Kabul edildi · **Faz:** E0

## Bağlam

M8 ile App içine gömülü Dear ImGui debug UI (F1) ve reflection Inspector eklendi; F1 ile de tek keşif yüzeyi (`ApiDiscoverySummary`), `TypeRegistry` şema sürümü ve `DocumentEntityId` belge kimliği sözleşmesi oturtuldu. Yol haritası E0'da üç temel gereksinim bulunur:

1. **Bağımsız editör hedefi ve paket izolasyonu:** Editör `Astral::App`'i tüketebilir; ancak oyunlar ve dedicated server paketleri editör veya ajan entegrasyonunu asla linklemez ve paketlemez.
2. **Ortak, sürümlü, keşfedilebilir belge komutları ve insan onay akışı:** Kullanıcı ve AI ajan aynı sürümlü komut şemasıyla öneri üretir; her öneri kalıcı belge kimliği (`DocumentEntityId`) ve revizyon (`DocumentRevision`) ile doğrulanır, alan düzeyinde fark (`DocumentDiff`) ve izole geçici ECS `World` önizlemesi (`PreviewSandbox`) gösterilir, onayda tek atomik transaction ile diske kaydedilir, hata halinde rollback yapılır, undo/redo bayt-bayt aynı belgeyi üretir ve audit log tutulur.
3. **Dear ImGui docking ve multi-viewport değerlendirmesi:** Mevcut `v1.92.9` non-docking tag'i, `v1.92.9-docking` ile mevcut `DebugUi` / `UiLayer` (SDL3 + Vulkan RHI) ve DPI ölçekleme üzerinde prototiplendi.

## Karar

### 1. İki Katmanlı Editör Modül Ayrımı
- **`Astral::EditorCore` (`modules/editor_core`, Katman 3):** Bağımlılıklar: `Astral::Core`, `Astral::World`, `Astral::Assets`. Pencere, GPU, Vulkan veya ImGui bağımlılığı **yoktur**. Belge deposu (`EditorDocument`, `DocumentRevision`), atomik dosya G/Ç (`IEditorFileSystem` / `LocalEditorFileSystem`), ortak sürümlü komut kataloğu (`EditorCommand`, `editor_command_schema`), transaction/rollback/undo-redo/audit log (`EditorSession`), alan farkı (`diff_documents`), geçici ECS önizleme (`PreviewSandbox`) ve ajan sağlayıcı arayüzünü (`IAgentProvider`, `PrefabAssistantAgent`, `ProposalReview`) taşır. Tüm komut/şema/transaction/ajan testleri 100% GPU'suz çalışır.
- **`Astral::Editor` (`modules/editor`, Katman 4):** Bağımlılıklar: `Astral::Core`, `Astral::World`, `Astral::Assets`, `Astral::App`, `Astral::EditorCore`, `Astral::ImGui`. Masaüstü çalışma alanı (`EditorWorkspace`), dockable paneller (Viewport, Asset Browser, Hierarchy, Inspector, AI Assistant & Diff Review, Console, Profiler) ve `examples/world_editor` yürütülebilir hedefini sağlar.
- **Paket izolasyonu:** `Astral::App`, `astral_add_game`, `templates/blank` ve `tests/consumer` hiçbir şekilde `Astral::EditorCore` veya `Astral::Editor`'a bağlanmaz. CTest `editor_isolation_check` ve `tests/consumer` bu izolasyonu sürekli doğrular.

### 2. ImGui Docking ve Multi-Viewport Kararı
- `modules/render/CMakeLists.txt` Dear ImGui pin'i `v1.92.9` → `v1.92.9-docking` olarak güncellenir.
- **Docking (`ImGuiConfigFlags_DockingEnable`):** Tek ana pencere içi dockspace (`ImGui::DockSpaceOverViewport`), mevcut `UiLayer` ve `FontScaleDpi` ile ek swapchain gerektirmeden Windows ve Linux'ta (Xvfb dahil) doğrudan çalışır — **E0'da etkinleştirilir**.
- **Multi-viewport (`ImGuiConfigFlags_ViewportsEnable`):** İkincil OS pencereleri için platform çoklu-pencere yaşam döngüsü (`Platform::Window`) ve pencere başına Vulkan swapchain + DPI koordinat eşlemesi gerektirir. Mevcut `Astral::Platform` tek pencere ve `Astral::Render` tek swapchain üzerine kurulu olduğundan multi-viewport E0'da kapalı bırakılır (`ConfigViewportsNoAutoMerge` / eklenti noktası olarak saklanır) ve platform çoklu-pencere desteği açıldığında ayrı iş olarak eklenir.

### 3. Atomik Kayıt, Rollback ve Audit Log
- `LocalEditorFileSystem::write_atomic` içeriği `<dosya>.tmp` yoluna yazar, flush eder ve atomik `rename` ile hedef dosyayla değiştirir. Kısmi yazma / I/O hatasında geçici dosya silinir, hedef dosya değişmez ve `EditorSession` bellek içi belge revizyonunu geri alır (rollback).
- `EditorSession::undo()` ve `redo()` önceki/sonraki tam belge anlık görüntüsünü aynı deterministik `Json::dump(2)` biçimiyle yeniden yazar; böylece `undo → redo` döngüsü bayt-bayt aynı dosya içeriğini üretir.

## Sonuçlar
- `tools/module_layers.json` ve `docs/architecture.md` `editor_core` (Katman 3) ve `editor` (Katman 4) kenarlarıyla güncellenir.
- Oyun ve sunucu ikilileri editör/ajan kodu taşımaz.
- E1 (dünya düzenleme ve Level Instance) aynı `EditorSession` transaction, undo/redo ve AI onay akışını `.world`/`.ent` belgelerine genişletebilir.
