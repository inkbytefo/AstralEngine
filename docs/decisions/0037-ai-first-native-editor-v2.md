# ADR-0037: AI-First Native Editör Mimarisi, Viewport Hedefi ve İnceleme Sözleşmesi (E2)

**Tarih:** 2026-11-23 · **Durum:** Kabul edildi · **Faz:** E2

## Bağlam

E0 ([ADR-0035](0035-standalone-ai-assisted-editor-core.md)) ve E1 ([ADR-0036](0036-world-authoring-and-level-instances.md)) ile GPU'suz belge/transaction/AI çekirdeği (`Astral::Editor_core`), `.world`/Level Instance yazarlığı ve ilk `examples/world_editor` çalışma alanı kuruldu. Depo denetimi, mevcut prototipi ürün düzeyinde **AI-First Native Editör**'e taşımak için beş temel eksik ortaya çıkardı:

1. **Docking ve ImGui entegrasyonu:** `#ifdef ImGuiConfigFlags_DockingEnable` (enum adı, makro değil) nedeniyle docking çalışma anında devre dışı kalıyordu; layout kalıcılığı (`IniFilename`), klavye navigasyonu, clipboard ve eksik tuşlar (`Delete`, `Home`, `End` vb.) yoktu.
2. **Gerçek Viewport hedefi:** 3D sahne ve UI tek pass'te doğrudan swapchain'e çiziliyordu; dock edilebilir, ölçeklenebilir, DPI-güvenli ve Önce/Sonra AI önizlemesi gösterebilen bir offscreen viewport dokusu yoktu.
3. **Kayıt, Undo ve Play semantiği:** Inspector her sürükleme karesinde diske atomik yazım ve tam sahne reload yapıyordu; `WorldAuthoringSession` undo/redo taşımıyordu; izole Play/Pause/Stop dünyası yoktu.
4. **Progresif açılım (Focus vs Advanced):** Açılışta 6 panel üst üste biniyordu; sakin, viewport baskın **Focus** çalışma alanı, sol ikon rayı, bağlamsal Inspector, komut paleti (`Ctrl+K`) ve merkezi tasarım token'ları yoktu.
5. **Asenkron AI iş akışı:** Ajan senkron ve sabit mock'tu; 7 durumlu görev yaşam döngüsü, harici sağlayıcı adaptörü (Codex CLI) ve 3D viewport'ta gerçek Önce/Sonra önizlemesi eksikti.

## Karar

### 1. Modül ve Hedef Sınırları (İzolasyon Korunur)
- **`Astral::Editor_core` (`modules/editor_core`, Katman 3):** GPU/pencere/ImGui bağımsız kalır. Birleşik belge oturumu (`IDocumentSession`, dirty state, sürükleme birleştirme, `.autosave` kurtarma, `.scene` + `.world`/`.ent` undo/redo), asenkron `AgentTaskManager` (7 durumlu görev makinesi + yetki kapısı), `CodexCliProvider` ve yerleşik çevrimdışı `PrefabAssistantAgent` burada yaşar.
- **`Astral::Editor` (`modules/editor`, Katman 4):** Monolitik `panels.cpp` yerine `EditorPanel` kayıt defteri, `UIStyle` tema token'ları, `Focus` / `Advanced` yerleşim preset'leri, komut paleti (`Ctrl+K`), viewport kontrolcüsü (picking, kamera, gizmo) ve izole Play modu host'u küçük, tek sorumluluklu dosyalara bölünür (`tools/check_sizes.py` 600/80/25 bütçesine tam uyum).
- **Uygulama hedefi:** `examples/world_editor` mevcut test ve CMake uyumluluğunu koruyarak `AstralEditor` ürün kabuğuna evrilir; oyun ve dedicated server hedefleri `editor_core` / `editor`'ı asla linklemez (`editor_isolation_check`).

### 2. Dear ImGui Sürümü ve Tek Context Kuralı
- Tek `ImGuiContext` `Astral::App` / `DebugUi` içinde kalır (ikinci context yasak).
- Docking koruması `#ifdef IMGUI_HAS_DOCK` olarak düzeltilir; Dear ImGui `v1.93.0-docking` doğrulaması yapılır (uyumluluk sorunu çıkarsa `v1.92.9-docking` korunur).
- `Platform::Window` / `Input` katmanına eksik düzenleme tuşları, clipboard okuma/yazma ve temel imleç biçimleri üçüncü parti tipleri public header'a sızdırmadan eklenir.
- İlk sürümde tek OS penceresi içinde dock edilen paneller kullanılır; çoklu OS penceresi (`ImGuiConfigFlags_ViewportsEnable`) çoklu swapchain desteği eklenene kadar kapalı kalır.

### 3. Offscreen Viewport Hedefi ve UI-Only Pass (Motor Dikişi)
- `Renderer`, editör veya ImGui bilgisi içermeden genel amaçlı offscreen render target (`create_render_target` → `RenderTargetHandle`, `TextureHandle` olarak örneklenebilir `ColorTarget | Sampled` doku) ve yalnızca UI katmanlarını çizen `render_ui()` API'sini sunar.
- Resize kare başında uygulanır; eski GPU dokusu `gpu::Device` ertelenmiş silmesiyle in-flight kareler tamamlanınca serbest bırakılır.
- Aynı mekanizma canlı sahne, **Play dünyası** ve AI **Önce/Sonra** aday 3D önizlemesi için ortak kullanılır.

### 4. Bellek İçi Düzenleme, `Ctrl+S`, Auto-Save ve Play İzolasyonu
- Inspector, gizmo ve sahne ağacı düzenlemeleri bellek içi transaction + undo/redo yığınına yazılır ve canlı ECS'e anında yansır; sürüklemeler tek undo adımında birleştirilir.
- Diske kalıcı yazım `Ctrl+S` (atomik temp+rename) ile yapılır; dışarıdan değiştirilen dosyalar `DocumentRevision::hash` ile yakalanır ve kullanıcıya çakışma uyarısı sunulur.
- Periyodik/çökme kurtarma amaçlı `.autosave` yan dosyası desteklenir (temiz kayıtta silinir).
- Play modu, açık belgenin kaydedilmemiş değişiklikler dâhil anlık görüntüsünden ayrı bir oyun `World` / `PhysicsWorld` / `ScriptHost` kopyası başlatır; Stop'ta editör belgesine temiz dönülür.

### 5. AI Sağlayıcı ve Yetki Politikası
- **Otomatik (düşük risk, salt-okunur):** Proje/sahne okuma, bağlam toplama, izole `PreviewSandbox` doğrulaması ve aday 3D önizleme.
- **Açık İnsan Onayı Zorunlu:** Canlı belgeye veya diske geçen her değişiklik (`Uygula` / `Seçileni Onayla` / `Revize Et` / `Reddet`); uygulama öncesi `DocumentRevision` tekrar doğrulanır.
- **Varsayılan Kapalı / Açık İzinli:** Harici komut çalıştırma ve ağ erişimi (`AgentPolicy`).
- İlk harici sağlayıcı yerel `CodexCliProvider` adaptörüdür; çevrimdışı veya sağlayıcı kapalıyken yerleşik `PrefabAssistantAgent` ve tüm manuel editör araçları eksiksiz çalışır.

## Sonuçlar
- Public motor header'ları üçüncü parti tiplerden temiz kalır (`astral_public_headers_clean`).
- `.scene`, `.prefab`, `.world`, `.ent` dosya biçimleri geriye uyumlu kalır.
- Tüm editör çekirdek testleri GPU'suz çalışmaya devam eder; görsel fazlar Windows MSVC üzerinde RTX 3060 ile doğrulanır.
