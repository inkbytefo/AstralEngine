# AstralEditor Refaktörü — Uçtan Uca Uygulama Rehberi

> **Uygulayıcı AI ajanına:** Ortamında varsa `superpowers:executing-plans` kullanarak görevleri sırayla uygula. Birden fazla ajan kullanımı ayrıca kullanıcı tarafından yetkilendirilirse ortak dosya sahipliği kurallarına uy. Bu belge görev planıdır; kutuların boş olması işlerin henüz yapılmadığını gösterir.

**Goal:** Prowl referansındaki editör görünümünü Astral'ın C++/ImGui altyapısında SRP/DRY ilkeleriyle, çalışır authoring araçları ve ölçülebilir performansla gerçekleştirmek.

**Architecture:** Backend, shell, session, commands, read models, paneller ve ortak widgets ayrılır. Renderer'a dar viewport bridge bağlanır. Görsel değişiklikler ile scene mutation/undo/input davranışı aynı kabul sürecinde doğrulanır.

**Tech Stack:** Mevcut C++20, CMake, ImGui docking, ImGuizmo, GLFW, Vulkan/VMA ve Astral ECS/CommandStack. C#/.NET/Origami portu yok.

**Spec:** [EDITOR_REFACTOR_DESIGN.md](EDITOR_REFACTOR_DESIGN.md).
**Research:** [PROWL_EDITOR_RESEARCH.md](PROWL_EDITOR_RESEARCH.md).
**Render dependency:** [RENDER_REFACTOR_IMPLEMENTATION_PLAN.md](RENDER_REFACTOR_IMPLEMENTATION_PLAN.md).

**Durum:** Yalnız planlama tamamlandı. Bu dokümantasyon oturumunda editor kaynakları değiştirilmedi, Prowl kodu Astral'a taşınmadı.

**Eşzamanlı çalışma notu:** Belge hazırlanırken çalışma ağacında renderer header'ları ve engine test kayıtlarında başka değişiklikler görünmeye başladı. Bu dokümantasyon işi o dosyalara dokunmadı. E00 uygulayıcısı renderer'ın gerçekten hangi aşamada olduğunu güncel kaynaklardan yeniden belirlemelidir.

## 1. Başlamadan önce

- `AGENTS.md`, araştırma, tasarım ve bu rehberi tamamen oku.
- Referans Prowl commit'i `17952407f89c778354a45fb728e6a6e00933869e`; hareketli main branch'e göre planı sessizce değiştirme.
- Görsel: [Prowl referansı](reference/prowl-editor-reference.png). Ekteki metinleri kullanıcı talimatı sanma.
- Yeni sınıf/API isimleri bu planda önerilen hedeflerdir; mevcut kodda varmış gibi çağırma.
- Mevcut kaynak ve test sonuçları eski raporla çelişirse gerçek kaynak/test kanıtını esas al.
- Başlangıçta mevcut kullanıcı değişikliklerini kaydet; ilgisiz dosyaları reset/stage/commit etme.
- Sadece editor görünümü için runtime renderer'ı yeniden yazma. Scene/Game ayrımı, overlays ve texture lifetime gereken dar renderer sınırları Bölüm 4'te belirtilmiştir.
- Her görevin build/test döngüsü tamamlanmadan bağımlı işe geçme. Hata testini kaldırarak ilerleme.
- İlgili kaynak dosyaları CMake'e eklenir, test executable/runner'a kayıt yapılır; yalnız test dosyası yazmak yeterli değildir.
- Görev sonunda uygulama günlüğü yaz; commit yapılacaksa yalnız ilgili dosyaları ayrı commit'e al. Push/merge bu planın kapsamında değildir.
- Bu görevin hazırlayıcısına uygulama yetkisi verilmedi; başka ajan bu rehberi uygulama talebi aldığında çalışmaya başlar.

## 2. Dosya yolu kısaltmaları ve sınırlar

Görevlerde yer kazandırmak için şu açık kökler kullanılır:
- `H/` = `tools/AstralEditor/include/Astral/Editor/`.
- `S/` = `tools/AstralEditor/src/`.

Örneğin `H/Core/EditorSession.hpp`, gerçek `tools/AstralEditor/include/Astral/Editor/Core/EditorSession.hpp` yoludur. Kısaltmaları dosya sistemi klasörü olarak oluşturma. Yeni .cpp dosyaları E01'deki AstralEditorCore listesine eklenir.

Mevcut korunacak parçalar: `SelectionContext`, `SelectionOperations`, `TransformGizmo`, `GizmoState`, `CommandStack`, `SceneCommands`, `AssetManager`. Bu parçaların işlevini ikinci kez sıfırdan yazma; adaptörle taşı ve son görevde gereksiz adaptörü kaldır.

## 3. Bağımlılık ve kilometre taşları

```text
E00 → E01 → E02 → E03 → E04 → E05 → E06
                     └→ E07 → E08 → E09 → E10
E10 + E03/E04 → E11
E05/E08 → E12 → E13
E02/E03/E09 → E14 → E15 → E16
E03/E09 → E17 → E18 → E19
E08/E09 → E20
E06/E07/E10/E20 → E21
E11/E13/E16/E18/E20/E21 → E22 → E23
```

Görev sırası aşağıdaki numara sırasıdır. Bağımlılık tablosu yalnız bağımsız çalışma alanlarını gösterir; çoklu ajan başlatma talimatı değildir.

| Kapı | Sonuç | Kanıt |
|---|---|---|
| A / E09 | Backend/session/command/panel sınırları | Mevcut CPU/GPU davranışı korunmuş |
| B / E21 | Layout/tema/ana paneller/ayarlar | Prowl benzeri kompozisyon ve çalışan kontroller |
| C / E22 | Asset/console/view kaynakları ve görsel test | Lifecycle ve screenshot review |
| D / E23 | Tam teslim | Performans, regresyon, son mimari rapor |

## 4. Render planıyla ortak sözleşme

| Editor işi | Render tarafı bağımlılığı | Geçiş stratejisi |
|---|---|---|
| E14 image lease | G05 RenderTargets, G14 FrameResources | Eski senkron API adapter'ı; generation cache |
| E14 picking | G08 PickingReadback, G15 completion | Scene/view/request/frame kimliği; eski cevabı düşür |
| E15 editor camera | RenderFrameSettings / RenderCamera | Kamera değeri girdi; authoring ECS'ye yazma |
| E16 Scene/Game | TemporalState ve view ownership | İlk teslim tek aktif view; eşzamanlı view için ayrı history |
| E19 thumbnails | Ayrı offscreen hedef ve GPU completion | Main renderer history'sini thumbnail için resetleme |
| Resize | G16 kaynak geçişi | Frame sınırında uygulama; ImGui lease tüketimi tamamlanır |

Render planı uygulanmış kabul edilmez. E00'da gerçek API kontrol edilir. İki ajan aynı anda `Application.cpp`, `SDFRenderer` veya `VulkanContext` değiştirmez. Editor bridge sahibi contract önerir, renderer sahibi uygular ve birlikte test eder. Mevcut tek renderer'a her frame Scene ve Game için iki farklı kamera sırayla verip tek history paylaşma.

## 5. Kararlı API ve davranış taslakları

Aşağıdaki C++ tanımlar arayüz niyetini netleştirir; gerekli include'lar ilgili header'da doğrudan eklenir. `UUID`, `Scene`, `Entity`, `CommandStack` mevcut Astral tipleridir. Adı yeni olan tipler ilgili görevde oluşturulur.

```cpp
// H/Core/EditorSelection.hpp
struct EditorEntityKey {
    uint64_t sceneInstance = 0;
    UUID entityId{};
};
struct AssetSelection { UUID assetId{}; };

// H/Core/EditorCommandRouter.hpp
struct CommandResult { bool executed = false; std::string reason; };
struct EditorCommand {
    std::string id;
    std::string label;
    std::function<bool()> canExecute;
    std::function<CommandResult()> execute;
};

// H/Inspector/PropertyRegistry.hpp
using PropertyValue = std::variant<float, int32_t, bool, glm::vec3, UUID>;
struct PropertyAddress {
    EditorEntityKey entity;
    std::string componentId;
    std::string propertyId;
};
// Registry: getter/setter callback hedefi her erişimde çözümleyecek.
// Bool olmayan ECS enabled değerini bool* cast ederek yazma.
```

Enum property ilk sürümde int32_t + enum seçenek metadata'sı kullanır; Color3 ve Vector3 aynı value tipini farklı drawer ile çizer. Asset UUID ile int property karışmaması için PropertyValue tip çözümlemesini test et. Entity UUID alanını mevcut Identity/Scene API'sinden doğrula; `EntityHandle`'ı UUID yerine cast etme.

### 5.1 Transaction algoritması

```text
Begin(addresses):
  tüm hedefleri çöz; before değerlerini kopyala; biri yoksa başlamadan reddet
Preview(values):
  tüm yeni değerleri validate et; sonra hedeflere uygula
Commit:
  after değerlerini oku
  değişiklik yoksa işlemi bitir
  önce before'a geri dön
  before/after hedefli ICommand'ı CommandStack::PushAndExecute ile uygula
Cancel:
  yaşayan ve aynı sceneInstance'a ait hedeflere before'u geri yükle
  undo kaydı ekleme
```

Bu yaklaşım preview'nin iki kez uygulanmasını önler. Setter side effect'leri pahalıysa `PushExecuted` alternatifini açık kontratla eklemek mümkündür; testleri aynı davranışı doğrulamalı. Transaction iki undo sistemi yaratmaz.

### 5.2 Texture lease protokolü

```text
renderer view hazır → surface {viewId,generation,extent,frameSerial}
backend register/cache → ImGui texture ID
panel draw → texture tüketimini frame serial'a bağla
resize/close → generation retired
GPU tüketimi tamamlandı → descriptor kaldır → view/image serbest bırak
```

ImGui texture ID GPU image handle değildir. Lease state'i panel içinde rastgele silinmez. Thumbnail descriptor'ları da aynı retire kuralına uyar.

### 5.3 CPU test hedefi örneği

```cmake
add_executable(EditorArchitectureTests tests/EditorArchitectureTests.cpp)
target_link_libraries(EditorArchitectureTests PRIVATE AstralEditorCore)
add_test(NAME Editor.Architecture COMMAND EditorArchitectureTests)
set_tests_properties(Editor.Architecture PROPERTIES LABELS "CPU;Editor" TIMEOUT 30)
```

Test için Release'te derlenmeyen `assert` kullanma; mevcut test framework veya açık failure counter/exit code kullan. E01 yeni main'i oluştururken bağımsız suite fonksiyonlarını çağırır. Her görev aşağıda belirtilen senaryoyu gerçek public service API'si üzerinden çalıştırır; kaynak metninde sınıf adı arayan test yeterli değildir.

## 6. Görevler

### E00 — Başlangıç kaydı ve değişiklik sınırı

**Bağımlılık:** Yok.

**Ne / neden:** Mevcut kullanıcı ve renderer ajanı değişikliklerini korumak; UI farkını ölçülebilir yapmak.

**Dosyalar:** docs/editor-refactor/BASELINE.md; artifacts/editor-refactor/baseline/; oku: tools/AstralEditor/CMakeLists.txt, src/Core/Application.cpp.

**Üretilen sözleşme:** Kaynak commit'i, kullanılan font/DPI, test sonucu ve mevcut editör screenshot'ı.

**Nasıl uygulanacak:**

- [ ] 1. git status --short ve git rev-parse HEAD sonucunu kaydet; WORK_PLAN.md ve mevcut render planı dosyalarını kendi uygulaman sayma.
- [ ] 2. cmake --build --preset mingw-release -j 4 komutunu çalıştır; ardından ctest --preset test-release --output-on-failure ve ./build-release/EngineTests.exe --contract çalıştır. Eski konuşmadaki test sonuçlarını yeni baseline yerine kullanma.
- [ ] 3. AstralEditor'ı mevcut desteklenen başlangıç yolu ile aç; 1920×1080 ve 1366×768 görüntülerini, 100% DPI durumunu al. CLI bayrağı yoksa varmış gibi komut yazma.
- [ ] 4. Hierarchy selection, inspector drag, gizmo, asset gezinme, play/pause/stop ve layout reset akışlarını kaydet. Başlangıçtaki kusurları ayrı listele.
- [ ] 5. Render refaktörü başka ajanla sürüyorsa başlangıçta mevcut renderer API'si ve dokunulmayacak dosyalar konusunda çalışma kaydı oluştur.

**Doğrulama:** Build exit code, çalışan test sayısı, screenshot boyutu/DPI ve source kimliği kayıtlı.

**Tamamlanma koşulu:** Baseline belgeli; kullanıcı değişiklikleri korunmuş.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E00 başlığına yazılır. Test başarısızsa E00 tamamlandı işaretlenmez.

### E01 — Test edilebilir editör hedeflerini ayır

**Bağımlılık:** E00.

**Ne / neden:** Aynı editor kaynaklarının test için elle tekrar listelenmesini ve yeni panelin test dışı kalmasını önlemek.

**Dosyalar:** tools/AstralEditor/CMakeLists.txt; yeni tools/AstralEditor/tests/EditorArchitectureTests.cpp.

**Üretilen sözleşme:** AstralEditorCore static library, AstralEditor executable ve EditorArchitectureTests CPU executable.

**Nasıl uygulanacak:**

- [ ] 1. main.cpp dışındaki mevcut editor kaynaklarını AstralEditorCore hedefinde topla; AstralEditor main.cpp bu hedefe bağlansın.
- [ ] 2. Mevcut EditorSelectionTests'i aynı core'a bağla; SceneHierarchy/Theme/Gizmo cpp'lerini test hedefinde ikinci kez derleme. Engine target'a ImGui bağımlılığı ekleme.
- [ ] 3. EditorArchitectureTests için CTest adı Editor.Architecture ve CPU;Editor etiketlerini ekle. main ve exit code gerçek başarısızlığı yansıtsın.
- [ ] 4. Editor core'un linklenmesinin CPU testte pencere/device yaratmadığını doğrula; static initialization içinde UI veya Vulkan init bırakma.
- [ ] 5. Yeni source dosyası ekleyen her görev core CMake listesini aynı değişiklikte güncellesin.

**Doğrulama:** Editor.Selection, Editor.Architecture ve mevcut engine CPU testleri geçiyor.

**Tamamlanma koşulu:** Test hedefleri ortak kaynağı kullanıyor; editor kapalıyken CPU test çalışıyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E01 başlığına yazılır. Test başarısızsa E01 tamamlandı işaretlenmez.

### E02 — ImGui Vulkan backend sahipliğini ayır

**Bağımlılık:** E01.

**Ne / neden:** EditorUI'nin GPU backend ve panel sorumluluklarını ayırmak.

**Dosyalar:** yeni H/Backend/ImGuiVulkanBackend.hpp ve S/Backend/ImGuiVulkanBackend.cpp; S/EditorUI.cpp; H/EditorUI.hpp.

**Üretilen sözleşme:** ImGuiVulkanBackend::BeginFrame(); EndFrame(vk::CommandBuffer, vk::ImageView, vk::Extent2D); WantsCaptureMouse/Keyboard.

**Nasıl uygulanacak:**

- [ ] 1. InitImGui, ShutdownImGui, descriptor pool, Vulkan function loading ve dynamic rendering kısmını tek RAII backend'e taşı.
- [ ] 2. EditorUI aynı dış API'yle delegasyon yapsın. Bu görevde tema/layout veya panel sırasını değiştirme.
- [ ] 3. Başlatma hatasında GLFW/ImGui/Vulkan kaynaklarının yalnız kurulmuş olanları kapansın. Device backend'den uzun yaşasın.
- [ ] 4. Swapchain format/image count yeniden kurulumunda ImGui backend bilgilerinin nasıl güncellendiğini açık handler'a taşı.
- [ ] 5. Texture ekleme/çıkarma için backend sınırı oluştur; raw ImGui_ImplVulkan çağrılarını yeni panellere yayma.

**Doğrulama:** Aç/kapat 20 tur; resize/minimize/restore; validation açık editor çizimi; UI CPU testleri.

**Tamamlanma koşulu:** EditorUI içinde descriptor pool ve backend init ayrıntısı yok; görüntü baseline ile aynı.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E02 başlığına yazılır. Test başarısızsa E02 tamamlandı işaretlenmez.

### E03 — EditorSession ve seçim yaşam döngüsü

**Bağımlılık:** E02.

**Ne / neden:** UI state'in paneller arasında dağılmasını ve stale entity erişimini önlemek.

**Dosyalar:** yeni H/Core/EditorSession.hpp, S/Core/EditorSession.cpp; H/Core/EditorSelection.hpp, S/Core/EditorSelection.cpp; mevcut H/SelectionContext.hpp; S/EditorUISubsystem.cpp.

**Üretilen sözleşme:** EditorEntityKey(sceneInstance, UUID), EditorSelection ve EditorSession; mevcut SelectionContext geçiş adaptörü.

**Nasıl uygulanacak:**

- [ ] 1. Entity selection'u sceneInstance + UUID ile adresle; primary ve ordered multi-select davranışını koru.
- [ ] 2. Asset selection'u UUID ile ayrı variant olarak ekle. Inspector entity veya asset hedefini açıkça çözsün.
- [ ] 3. Selection değişiminde revision artır; aynı seçimi yeniden atamak gereksiz notification üretmesin.
- [ ] 4. Scene değişiminde eski anahtarları temizle, panel lock açıksa bile ölü entity çözümleme.
- [ ] 5. EventBus aboneliklerinin sahipliğini session veya panelde tek yerde tut; kapanışta abonelikler kalksın.

**Doğrulama:** Aynı handle başka scene'de seçili sayılmaz; delete/undo sonra UUID çözümü; Ctrl toggle; asset/entity geçişi.

**Tamamlanma koşulu:** Selection için tek kaynak var; UI ve renderer highlight aynı primary'den besleniyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E03 başlığına yazılır. Test başarısızsa E03 tamamlandı işaretlenmez.

### E04 — Command router ile tek eylem yolu

**Bağımlılık:** E03.

**Ne / neden:** Menü, toolbar ve shortcut'ın farklı davranış üretmesini engellemek.

**Dosyalar:** yeni H/Core/EditorCommandRouter.hpp, S/Core/EditorCommandRouter.cpp; S/EditorMenuBar.cpp; S/EditorUI.cpp; S/EditorUISubsystem.cpp.

**Üretilen sözleşme:** Register(EditorCommand); CanExecute(id); Execute(id) → CommandResult. Mevcut CommandStack kalır.

**Nasıl uygulanacak:**

- [ ] 1. scene.save, edit.undo, edit.redo, entity.delete/duplicate, play.start/pause/stop/step ve layout.reset ID'lerini kayıt altına al.
- [ ] 2. Her command label, enabled predicate ve action içersin; CanExecute gerçek execute sırasında yeniden kontrol edilsin.
- [ ] 3. Toolbar/menu doğrudan scene mutation yapmasın; router çağrısına geçsin.
- [ ] 4. Undo gereken action ICommand üretip mevcut stack'e verilsin. Router ikinci undo stack oluşturmasın.
- [ ] 5. Başarısız action açıklamasını kullanıcıya taşınabilir sonuç olarak döndür; error string'i tooltip/console tüketebilsin.
- [ ] 6. Save/Save As, yeni sahne ve proje/sahne değiştirme için dirty-state akışını tanımla. Kaydet/Değişiklikleri at/Vazgeç seçeneklerini gerçek serializer sonucuna bağla; disk yazımı başarısızsa dirty flag temizlenmez ve eski dosya korunur. Undo save noktasına dönünce dirty durumu doğru hesaplanır.

**Doğrulama:** Menu ve shortcut aynı state değişimini üretiyor; disabled action programatik çağrıda da reddediliyor; duplicate ID hata. Save başarısızlığı, Save As iptali ve kaydedilmemiş sahneyi kapatma seçenekleri veri kaybettirmiyor.

**Tamamlanma koşulu:** Her kullanıcı eylemi bir command ID üzerinden yürütülüyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E04 başlığına yazılır. Test başarısızsa E04 tamamlandı işaretlenmez.

### E05 — EditTransaction ve property undo temeli

**Bağımlılık:** E04.

**Ne / neden:** Inspector sürüklemelerini tek geri alınabilir işlem yapmak.

**Dosyalar:** yeni H/Core/EditTransaction.hpp, S/Core/EditTransaction.cpp; include/Astral/Core/CommandStack.hpp gerekirse dar uyum; tools/AstralEditor/tests/EditorArchitectureTests.cpp.

**Üretilen sözleşme:** Begin(PropertyAddress), Preview(PropertyValue), Commit(), Cancel(); transaction session başına bir aktif edit.

**Nasıl uygulanacak:**

- [ ] 1. Before değerlerini stable entity key + component/property ID üzerinden al; raw component pointer saklama.
- [ ] 2. Preview değerini tipli setter ile uygula. Commit sırasında after=before ise undo kaydı oluşturma.
- [ ] 3. Mevcut PushAndExecute semantiğine uy: preview uygulanmışsa önce before'a dön ve tek command'ı execute et veya açık PushExecuted ekle; iki yöntemi karıştırma.
- [ ] 4. Multi-edit before/after listesini tek composite ICommand yap. Partial validation hatasında bütün hedefleri önceki değere döndür.
- [ ] 5. Escape, panel kapanışı, scene switch ve entity silinmesi için cancel davranışını test et.

**Doğrulama:** 100 preview update → 1 undo; cancel → 0 undo; undo/redo doğru; component yeniden yerleşince pointer geçersizliği yok.

**Tamamlanma koşulu:** Property düzenleme yaşam döngüsü UI'den bağımsız CPU testli.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E05 başlığına yazılır. Test başarısızsa E05 tamamlandı işaretlenmez.

### E06 — Kısayol ve input önceliğini merkezileştir

**Bağımlılık:** E04,E05.

**Ne / neden:** Yazı yazarken entity silinmesi veya gizmo tıklamasının picking olması gibi çakışmaları önlemek.

**Dosyalar:** yeni H/Core/ShortcutRouter.hpp, S/Core/ShortcutRouter.cpp; S/EditorUISubsystem.cpp; S/Panels/ViewportPanel.cpp; S/Gizmo/TransformGizmo.cpp.

**Üretilen sözleşme:** InputContext(modal,textEdit,propertyDrag,gizmoDrag,focusedPanel,gameCapture) ve command ID binding.

**Nasıl uygulanacak:**

- [ ] 1. Öncelik sırasını tasarım belgesi 6.2'ye göre uygula; bir event en fazla bir tüketiciye gider.
- [ ] 2. F5/F6 mevcut semantiğini named command'a taşı; global polling kopyalarını kaldır.
- [ ] 3. Ctrl+Z text edit aktifken text widget'a ait olsun; Delete rename alanındayken sahneyi silmesin.
- [ ] 4. Viewport toolbar/gizmo/modal hit'i picking request üretmesin; mouse capture bırakılırken stuck drag temizlensin.
- [ ] 5. Kullanıcı override'larını stable shortcut ID üzerinden sakla; conflict durumunu preferences'ta göster.

**Doğrulama:** Modal > text > gizmo > viewport > panel > global tablo testleri; aynı key tek execute.

**Tamamlanma koşulu:** Klavye ve mouse yönlendirmesi merkezi ve deterministik.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E06 başlığına yazılır. Test başarısızsa E06 tamamlandı işaretlenmez.

### E07 — ThemeTokens, font ve ikon sözleşmesi

**Bağımlılık:** E02.

**Ne / neden:** Prowl'a yakın kompakt görsel dili bütün panellere aynı ölçüyle uygulamak.

**Dosyalar:** yeni H/UI/ThemeTokens.hpp, S/UI/ThemeTokens.cpp; H/UI/EditorIcons.hpp, S/UI/EditorIcons.cpp; S/EditorTheme.cpp; docs/editor-refactor/THIRD_PARTY_NOTICES.md.

**Üretilen sözleşme:** Tasarım belgesi 3.2 token'ları; ApplyTheme(tokens, effectiveScale); stable IconId.

**Nasıl uygulanacak:**

- [ ] 1. Semantic renk ve ölçüleri tek veri nesnesine taşı; EditorPalette geçişte yeni token'lara delegasyon yapsın.
- [ ] 2. Font lisansını doğrula, gerekli font/icon asset'lerini lisanslarıyla paketle; OS font varlığına bağımlı kalma.
- [ ] 3. Türkçe glyph, bold, mono ve ikon baseline hizasını test et. Prowl repo font dosyasını lisans kaydı olmadan kopyalama.
- [ ] 4. DPI ve userScale tek kaynaktan hesaplanır; style'ı değişmemiş base değerlerinden uygula.
- [ ] 5. Play theme varyantı sadece token override'dır; Stop bütün authoring token'larını geri getirir.

**Doğrulama:** 100/150/200% ölçek, play/stop 20 kez, Türkçe metin; font bulunamazsa kontrollü fallback.

**Tamamlanma koşulu:** Panel içi yeni sabit renk/ölçü yok; paketli font ve notices mevcut.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E07 başlığına yazılır. Test başarısızsa E07 tamamlandı işaretlenmez.

### E08 — Ortak widget kataloğu

**Bağımlılık:** E05,E07.

**Ne / neden:** Aynı kontrolün farklı panellerde farklı görünmesini ve farklı commit davranışını kaldırmak.

**Dosyalar:** yeni H/UI/EditorWidgets.hpp, S/UI/EditorWidgets.cpp; yeni tools/AstralEditor/tests/EditorWidgetGallery.cpp; S/Panels/Inspector.cpp.

**Üretilen sözleşme:** IconButton, SearchField, PropertyRow, Vector3Field, ComponentHeader, EmptyState, StatusBadge helper'ları.

**Nasıl uygulanacak:**

- [ ] 1. Her helper visible label ile stable ID'yi ayırsın. Aynı label'lı iki component'te ImGui ID çakışmasın.
- [ ] 2. Property widget sonucu begin/changed/commit/cancel sinyali taşısın; transaction'a değeri nasıl yazacağını panel belirler.
- [ ] 3. Vector3 field'da ayrı eksen, mixed value, reset, direct input ve drag için tek kod yolu kullan.
- [ ] 4. Disabled reason ve keyboard focus görünümü bütün icon button'larda aynı olsun.
- [ ] 5. Gallery'de normal/hover/active/disabled/error/mixed/empty ve dar kolon durumlarını aynı sahnede göster.

**Doğrulama:** Gallery screenshot'ı; aynı label farklı ID; Escape cancel; 280px Inspector'da kesilme yok.

**Tamamlanma koşulu:** Yeni paneller ortak widgets kullanıyor; görünüm state'leri gözle doğrulandı.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E08 başlığına yazılır. Test başarısızsa E08 tamamlandı işaretlenmez.

### E09 — Panel registry ve editor shell

**Bağımlılık:** E03,E04,E08.

**Ne / neden:** Yeni panel eklemek için EditorUI'nin büyük switch/üye zincirini değiştirmeyi gereksiz kılmak.

**Dosyalar:** yeni H/Shell/PanelRegistry.hpp, S/Shell/PanelRegistry.cpp; H/Shell/EditorShell.hpp, S/Shell/EditorShell.cpp; S/EditorUI.cpp.

**Üretilen sözleşme:** PanelId → factory; IEditorPanel::Draw(), SaveState(), RestoreState(); constructor injection dar servislere.

**Nasıl uygulanacak:**

- [ ] 1. scene, game, hierarchy, inspector, project, console, preferences sabit panel ID'lerini kaydet.
- [ ] 2. Panel constructor'ı yalnız ihtiyaç duyduğu servis referanslarını alsın; EditorSession'ın her iç nesnesine erişen service locator verme.
- [ ] 3. EditorUI shell/backend facade olarak kalsın; eski paneller adapter ile çalışsın.
- [ ] 4. Panel kapanışı state ve subscription lifecycle'ını açık OnClose/dispose ile tamamlasın.
- [ ] 5. Menüde panel açma/odaklama komutlarını registry üzerinden bağla; duplicate açma politikası ilk sürümde her tür için tek örnek olsun.

**Doğrulama:** Aç/kapat/yeniden aç 50 kez; unknown panel ID güvenli reddi; kapanmış panele event gitmiyor.

**Tamamlanma koşulu:** Shell panel listesi composition yapıyor; backend scene bilmiyor. KAPI A.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E09 başlığına yazılır. Test başarısızsa E09 tamamlandı işaretlenmez.

### E10 — Prowl düzeni ve güvenli layout persistence

**Bağımlılık:** E09.

**Ne / neden:** Görsel kompozisyonu referansa yaklaştırırken kullanıcı düzenini korumak.

**Dosyalar:** yeni H/Shell/LayoutStore.hpp, S/Shell/LayoutStore.cpp; S/EditorWorkspace.cpp; S/Shell/EditorShell.cpp.

**Üretilen sözleşme:** schemaVersion=1; panel stable IDs; proje başına layout.ini + state.json; LayoutReset command.

**Nasıl uygulanacak:**

- [ ] 1. Varsayılan oranları 80/20, sol 70/30, alt 65/35, sağ 30/70 olarak DockBuilder'a aktar.
- [ ] 2. Scene/Game aynı tab grubunda; Hierarchy/Inspector sağda, Project/Console altta olsun.
- [ ] 3. ImGui title'da ###stable.id kullan; dil değişiminde dock kimliği değişmesin.
- [ ] 4. İlk açılış/reset dışında DockBuilder düzeni yeniden kurmasın. Bozuk/eksik/gelecek sürüm layout'a safe fallback uygula.
- [ ] 5. Kaydı temp dosya + replace ile yap; play/runtime panel state'i authoring state'in üzerine yazılmasın.
- [ ] 6. Dar ekranda minimum genişlikleri uygula; alt panelleri tab'a çeviren alternatif preset ekle.

**Doğrulama:** Aç→dock değiştir→kapat→aç eşdeğer; bozuk JSON; project A/B izolasyonu; dil değişimi.

**Tamamlanma koşulu:** Referans yerleşim ve kullanıcının düzeni birlikte çalışıyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E10 başlığına yazılır. Test başarısızsa E10 tamamlandı işaretlenmez.

### E11 — Hierarchy read model ve görünür satır çizimi

**Bağımlılık:** E03,E04,E10.

**Ne / neden:** Büyük sahnede tüm ağacı her frame yeniden işlemek yerine doğru seçim/drag davranışı sağlamak.

**Dosyalar:** S/Panels/SceneHierarchy.cpp; H/Panels/SceneHierarchy.hpp; yeni H/Panels/HierarchyModel.hpp ve S/Panels/HierarchyModel.cpp.

**Üretilen sözleşme:** HierarchyRow(key, depth, expanded, visible); revision+filter cache; command tabanlı rename/reparent/delete.

**Nasıl uygulanacak:**

- [ ] 1. Açık dallardan flat visible row list üret; scene revision, expand veya filter değişince yeniden kur.
- [ ] 2. Aramada eşleşen çocukların ancestor yolunu koru; collapsed state'i kalıcı UI state'te tut.
- [ ] 3. ImGuiListClipper veya eşdeğer görünür aralık çizimi uygula; selection index yerine key sakla.
- [ ] 4. Rename Enter commit/Escape cancel; reparent self/descendant cycle kontrolü yap; dünya transformu koruma politikasını test et.
- [ ] 5. Multi-delete ancestor+child seçildiyse aynı alt ağacı iki kez silmesin; command tek undo olsun.

**Doğrulama:** 10k entity; filter ancestor; Ctrl/Shift seçim; reparent cycle; delete/undo identity.

**Tamamlanma koşulu:** Hierarchy draw maliyeti görünür satırlarla ölçekleniyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E11 başlığına yazılır. Test başarısızsa E11 tamamlandı işaretlenmez.

### E12 — Typed property registry

**Bağımlılık:** E05,E08.

**Ne / neden:** Ham offset tabanlı property yazımını güvenli ve genişletilebilir hale getirmek.

**Dosyalar:** yeni H/Inspector/PropertyRegistry.hpp, S/Inspector/PropertyRegistry.cpp; H/Inspector/PropertyDrawers.hpp, S/Inspector/PropertyDrawers.cpp; mevcut H/EditorReflection.hpp.

**Üretilen sözleşme:** PropertyAddress, PropertyValue variant, typed Get/Set/Validate callbacks; ComponentDescriptor stable ID.

**Nasıl uygulanacak:**

- [ ] 1. Float/int/bool/vec3/color/enum/asset UUID için PropertyValue variant tanımla; schema ID display label'dan bağımsız olsun.
- [ ] 2. Callback her erişimde scene/entity/component çözer; component pointer'ını frame'ler arası cache'leme.
- [ ] 3. Transform, SDF, Velocity ve Health mevcut alanlarını kayıt et; unsupported component için açık fallback ver.
- [ ] 4. Min/max ve type validation setter sınırında; NaN ve geçersiz scale gibi değerleri reddet veya belgeli clamp uygula.
- [ ] 5. Offset registry'yi legacy adapter'a al; yeni property kaydı raw byte cast yapmasın.

**Doğrulama:** Yanlış variant reddi; silinen component; aynı isim farklı component; min/max ve NaN.

**Tamamlanma koşulu:** Yeni property eklemek registry+drawer üzerinden mümkün.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E12 başlığına yazılır. Test başarısızsa E12 tamamlandı işaretlenmez.

### E13 — Inspector panelini yeniden kur

**Bağımlılık:** E12,E11.

**Ne / neden:** Görsel olarak referanstaki hizalı component kartlarını gerçek undo ve multi-edit ile birleştirmek.

**Dosyalar:** S/Panels/Inspector.cpp; H/Panels/Inspector.hpp; Core/EditTransaction; Inspector/PropertyDrawers.

**Üretilen sözleşme:** Inspector selection read model; component sections; mixed-value multi-selection; lock UI state.

**Nasıl uygulanacak:**

- [ ] 1. Object header, enable/rename ve component bölümlerini ortak widgets ile çiz.
- [ ] 2. Transform X/Y/Z, rotation ve scale'ı EditTransaction'a bağla; bir drag tek undo.
- [ ] 3. Birden fazla entity'de ortak component'leri göster; mixed alanı düzenleyince yalnız düzenlenen alan/eksen değişsin.
- [ ] 4. Add/remove/reset component işlemleri command üretir; desteklenmeyen tipler listede yok veya açıklamalı disabled.
- [ ] 5. Inspector lock varsa hedef key sakla; scene kapanışı key'i geçersiz kılar. Asset inspector ile entity inspector davranışı ayrı drawer olur.

**Doğrulama:** Multi-transform mixed X edit Y/Z'yi korur; drag cancel; remove/undo; reset; dar panel screenshot.

**Tamamlanma koşulu:** Prowl'a yakın Inspector görünümü ve doğrulanmış düzenleme davranışı.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E13 başlığına yazılır. Test başarısızsa E13 tamamlandı işaretlenmez.

### E14 — Viewport bridge ve GPU texture lease

**Bağımlılık:** E02,E03,E09.

**Ne / neden:** Paneli renderer iç üyelerinden ayırmak ve render refaktörüyle kaynak ömrünü uyumlu yapmak.

**Dosyalar:** yeni H/Viewport/IEditorViewportBridge.hpp, S/Viewport/EditorViewportBridge.cpp, H/Viewport/EditorViewportBridge.hpp; S/Panels/ViewportPanel.cpp.

**Üretilen sözleşme:** ViewId, ViewRequest, ViewSurface(viewId,generation,extent,frameSerial), PickTicket; lease release/completion protokolü.

**Nasıl uygulanacak:**

- [ ] 1. Mevcut SDFRenderer pointer erişimini bridge'e taşı; panel yalnız ViewSurface ve request görsün.
- [ ] 2. Render planı henüz uygulanmadıysa eski senkron API için adapter yap. G14/G15 uygulanmışsa frame-slot metadata'sını tüket.
- [ ] 3. Texture descriptor cache key'i viewId+generation olsun; her frame remove/add yapma.
- [ ] 4. Resize talebini frame başına uygula; çizim sürerken kullanılan view'u yok etme.
- [ ] 5. Picking ticket sceneInstance/viewId/frameSerial/requestId taşısın; cevap o frame entity eşlemesine ait olsun.

**Doğrulama:** Resize sırasında draw/selection; stale generation; kapanan view cevabı; validation açık.

**Tamamlanma koşulu:** Panel GPU handle ömrünü kendisi yönetmiyor; render planıyla tek sözleşme.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E14 başlığına yazılır. Test başarısızsa E14 tamamlandı işaretlenmez.

### E15 — Scene kamera ve viewport araçları

**Bağımlılık:** E06,E14.

**Ne / neden:** Editör kamerasını oyun kamerasından ayırıp referanstaki sahne çalışma deneyimini sağlamak.

**Dosyalar:** yeni H/Viewport/EditorCameraController.hpp, S/Viewport/EditorCameraController.cpp; S/Panels/ViewportPanel.cpp; S/Gizmo/ViewportGizmoToolbar.cpp; S/Gizmo/TransformGizmo.cpp.

**Üretilen sözleşme:** EditorCameraState(position,orientation,projection); FrameSelection command; scene-only overlay flags.

**Nasıl uygulanacak:**

- [ ] 1. Orbit/pan/fly/zoom için editor camera state tut; active runtime camera component'ine yazma.
- [ ] 2. Focus selection bounding box üzerinden kamera mesafesi hesapla; boş seçimde no-op.
- [ ] 3. Sol tool rail move/rotate/scale; sağ üst orientation widget; local/world ve snap kontrolleri ortak IconButton kullansın.
- [ ] 4. Mevcut ImGuizmo işlemini transaction ve input router'a bağla; gizmo drag sırasında kamera/picking çalışmasın.
- [ ] 5. Dünya grid'i camera projection ile çizilir; depth davranışını bridge render overlay yolunda uygula. Overlay Game view'a girmez.

**Doğrulama:** Orbit sahne dosyasını dirty yapmıyor; gizmo undo; snap/local/world; kamera değişiminde TAA reset.

**Tamamlanma koşulu:** Scene view profesyonel navigasyon ve araç davranışı sağlıyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E15 başlığına yazılır. Test başarısızsa E15 tamamlandı işaretlenmez.

### E16 — Game view ve PlaySession

**Bağımlılık:** E04,E06,E14,E15.

**Ne / neden:** Scene/Game sekmelerini doğru kamera ve input ile ayırmak; play kontrollerini tek durum makinesine toplamak.

**Dosyalar:** yeni H/Core/PlaySession.hpp, S/Core/PlaySession.cpp; H/Panels/GameViewPanel.hpp, S/Panels/GameViewPanel.cpp; S/EditorUISubsystem.cpp.

**Üretilen sözleşme:** Edit/Playing/Paused state; Start/Pause/Resume/Stop/Step; tek aktif view başlangıç politikası.

**Nasıl uygulanacak:**

- [ ] 1. Mevcut authoring clone/restore akışını PlaySession'a taşı; davranış eşdeğerliğini önce doğrula.
- [ ] 2. Game viewport runtime kamerayı kullanır; free/aspect/fixed resolution seçenekleri gerçek render extent'e bağlanır.
- [ ] 3. Step sadece Paused'da tek simulation adımı yapar; Application'da gerekli dar API'yi ekle ve event sırasını test et.
- [ ] 4. Scene/Game arasında view change temporal history reset yapar. İlk sürüm yalnız aktif sekmeyi render eder.
- [ ] 5. İki paneli aynı anda görünür açma özelliği sunulacaksa önce ayrı render history/target sahipliğini uygula; aksi halde Game/Scene tek tab group politikası açık kalır.
- [ ] 6. Stop sonrası authoring scene, selection ve history politikası geri yüklenir; runtime mutation sahne dosyasına yazılmaz.

**Doğrulama:** Play→Pause→Step→Stop; runtime kamera yok; resize/aspect letterbox input dönüşümü; iki view history izolasyonu.

**Tamamlanma koşulu:** Scene ve Game semantik olarak ayrı; sahte step veya çift singleton render yok.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E16 başlığına yazılır. Test başarısızsa E16 tamamlandı işaretlenmez.

### E17 — AssetIndex ve çizim dışı dosya işi

**Bağımlılık:** E03,E09.

**Ne / neden:** ContentBrowser Draw içindeki tarama/sort maliyetini kaldırmak.

**Dosyalar:** yeni H/Assets/AssetIndex.hpp, S/Assets/AssetIndex.cpp; S/Panels/ContentBrowser.cpp; include/Astral/Asset/AssetManager.hpp gerekirse read API.

**Üretilen sözleşme:** AssetSnapshot(revision, entries); mevcut AssetManager UUID kaynağı; queued refresh.

**Nasıl uygulanacak:**

- [ ] 1. Mevcut AssetManager registry'sini editor snapshot'a çevir; ikinci meta/UUID sistemi oluşturma.
- [ ] 2. DrawDirectoryTree/DrawContentGrid/DrawFooter içindeki directory_iterator ve sort'u model refresh'e taşı.
- [ ] 3. Filesystem işi worker veya açık refresh aşamasında yapılır; sonuç UI thread'de atomik snapshot swap ile görünür olur.
- [ ] 4. Refresh event'lerini debounce et; project switch eski sonuç generation'ını iptal etsin.
- [ ] 5. Permission denied/missing directory/broken symlink durumlarını diagnostic entry olarak göster; recursive cycle'a girme.

**Doğrulama:** 10k metadata; ardışık refresh; project switch; erişilemeyen dizin; draw sırasında I/O sayacı sıfır.

**Tamamlanma koşulu:** Browser draw snapshot tüketiyor; dosya hatası UI'yi çökertmiyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E17 başlığına yazılır. Test başarısızsa E17 tamamlandı işaretlenmez.

### E18 — Project panel ve güvenli drag/drop

**Bağımlılık:** E17,E08,E04.

**Ne / neden:** Referanstaki tree/breadcrumb/thumbnail grid'i gerçek asset kimliğiyle sağlamak.

**Dosyalar:** S/Panels/ContentBrowser.cpp; H/Panels/ContentBrowser.hpp; yeni H/Assets/EditorDragPayload.hpp.

**Üretilen sözleşme:** Versioned typed payload(asset UUID/project generation veya entity key); type doğrulamalı drop.

**Nasıl uygulanacak:**

- [ ] 1. Tree + breadcrumb + back/forward + search + grid/list + size slider yerleşimini kur.
- [ ] 2. Filtre/sort cache'i asset revision/query ile invalid olur; görünür grid hücreleri dışında widget üretme.
- [ ] 3. Folder selection Inspector hedefini gereksiz temizlemeyecek açık politika uygula; file selection asset inspector'a gider.
- [ ] 4. Drag payload'a process pointer veya serbest path string koyma; stable ID/project generation kullan.
- [ ] 5. Destination field asset type kontrolü yapar; uyumsuz drop hata tooltip'i verir. Unsupported scene mesh drop'u sahte entity oluşturmasın.

**Doğrulama:** Asset rename/refresh kimliği korunuyor; farklı proje payload reddi; search clear; grid/list aynı seçim.

**Tamamlanma koşulu:** Project panel referans kompozisyonunda ve doğru asset akışında.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E18 başlığına yazılır. Test başarısızsa E18 tamamlandı işaretlenmez.

### E19 — Thumbnail queue/cache ve completion

**Bağımlılık:** E14,E17,E18.

**Ne / neden:** Asset önizlemelerini UI frame'ini dondurmadan üretmek.

**Dosyalar:** yeni H/Assets/ThumbnailService.hpp, S/Assets/ThumbnailService.cpp; S/Panels/ContentBrowser.cpp; Backend texture registry.

**Üretilen sözleşme:** ThumbnailKey(UUID,sourceRevision,configRevision,size); Request/GetReady; bounded LRU.

**Nasıl uygulanacak:**

- [ ] 1. Visible asset'leri önceleyen dedup queue kur; aynı key iki iş üretmesin.
- [ ] 2. İlk kapsam desteklenen görüntü/env/scene preview ve tip ikonlarıdır; mevcut olmayan mesh renderer'ı bu göreve gizlice ekleme.
- [ ] 3. CPU decode worker'da, GPU upload/completion render thread protokolünde; UI synchronous readback beklemesin.
- [ ] 4. Cache için byte üst sınırı ve LRU kullan; descriptor release GPU tüketimi sonrasına ertelenir.
- [ ] 5. Asset revision veya project generation değişince eski sonuç yayınlanmaz; hata negative cache'i tekrar denemeyi sınırlasın.
- [ ] 6. İlk bütçe frame başına en fazla 1 yeni GPU preview işi; ayrıca süre ölç ve görünür render'ın bütçesini koru.

**Doğrulama:** 1000 request/dedup; revision değişimi; cache eviction; panel kapanışı; GPU validation.

**Tamamlanma koşulu:** Gerçek preview ve güvenli fallback var; cache sonsuz büyümüyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E19 başlığına yazılır. Test başarısızsa E19 tamamlandı işaretlenmez.

### E20 — Console, status ve bildirim veri yolu

**Bağımlılık:** E09,E08.

**Ne / neden:** Referanstaki log panelini tek veri kaynağıyla sunmak ve yüksek log hacminde UI'yi korumak.

**Dosyalar:** yeni H/Diagnostics/LogStore.hpp, S/Diagnostics/LogStore.cpp; H/Diagnostics/NotificationCenter.hpp, S/Diagnostics/NotificationCenter.cpp; H/Panels/ConsolePanel.hpp, S/Panels/ConsolePanel.cpp; S/EditorStatusBar.cpp.

**Üretilen sözleşme:** LogEntry(id,severity,source,time,message,count); thread-safe bounded ingest; panel-local filter.

**Nasıl uygulanacak:**

- [ ] 1. Mevcut engine stdout loglarını sihirli biçimde yakalandı varsayma; mevcut logging/event kaynağına dar sink ekle veya ilk kapsam editor diagnostics olarak açıkça belirt.
- [ ] 2. LogStore üst sınırını 5000 giriş ve toplam byte limiti ile yapılandır; taşma sayısını göster.
- [ ] 3. Severity/search/collapse filtre cache'ini panel revision ile yönet; görünür satır çiz; unformatted text kullan.
- [ ] 4. Status bar sayaçları ve son log aynı store'u okur. Toast'lar tekrarlı event'leri dedup eder.
- [ ] 5. Mesajı kopyala, clear, autoscroll pause ve detay görünümü ekle; clear sonrası sıra index'i stale kalmasın.

**Doğrulama:** Çok thread 10k log; overflow; aynı mesaj count; filter/clear; %s metni; panel kapalıyken status.

**Tamamlanma koşulu:** Console/status tutarlı ve sınırlı bellek kullanıyor.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E20 başlığına yazılır. Test başarısızsa E20 tamamlandı işaretlenmez.

### E21 — Preferences ve kalıcılık

**Bağımlılık:** E07,E10,E06,E20.

**Ne / neden:** Kullanıcı tema/layout/shortcut tercihlerini güvenli saklamak.

**Dosyalar:** yeni H/Panels/PreferencesPanel.hpp, S/Panels/PreferencesPanel.cpp; Shell/LayoutStore; Core/ShortcutRouter.

**Üretilen sözleşme:** Versioned settings; theme scale, shortcuts, thumbnails, reduced motion; per-user vs per-project ayrımı.

**Nasıl uygulanacak:**

- [ ] 1. Preferences sayfalarını registry ile aç; unsupported özellik toggle'ı gösterme.
- [ ] 2. Theme preview Apply/Cancel ile çalışsın; cancel eski token'ları geri yükler.
- [ ] 3. Kısayol çakışmasını göster ve reset-to-default sun; key chord string serialization deterministik olsun.
- [ ] 4. Global kullanıcı tercihi ile proje layout state'ini ayrı dosyada tut; eksik/bozuk/gelecek schema sürümünü korumacı fallback ile aç.
- [ ] 5. Autosave layout yalnız değişince debounce ile çalışır; runtime play state authoring kaydına girmez.

**Doğrulama:** Roundtrip; corrupt/truncated file; Apply/Cancel; shortcut conflict; project A/B.

**Tamamlanma koşulu:** Ayarlar yeniden açılışta aynı ve güvenli. KAPI B.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E21 başlığına yazılır. Test başarısızsa E21 tamamlandı işaretlenmez.

### E22 — Görsel doğrulama ve etkileşim harness'i

**Bağımlılık:** E11,E13,E16,E18,E20,E21.

**Ne / neden:** Kalitenin yalnız geliştiricinin göz kararına veya bir güzel screenshot'a bağlı kalmamasını sağlamak.

**Dosyalar:** yeni tools/AstralEditor/tests/EditorVisualTests.cpp; tests/fixtures/editor/; docs/editor-refactor/VISUAL_REVIEW.md; tools/AstralEditor/CMakeLists.txt.

**Üretilen sözleşme:** Deterministik fixture + UI action sequence + screenshot artifact; GPU etiketi.

**Nasıl uygulanacak:**

- [ ] 1. Sabit scene/entity adları, seçili öğe, inspector değerleri ve fake log/asset read model ile fixture kur.
- [ ] 2. 1920×1080/1366×768 ve 100/150/200% DPI ekran görüntüsü al; font/render koşullarını kaydet.
- [ ] 3. Chrome ROI'lerini viewport 3D içeriğinden ayır. Prowl'a layout oranı/alignment/görsel yoğunluk üzerinden kıyas yap.
- [ ] 4. Empty/error/loading/mixed/disabled durumlarını ayrı capture et; hover/focus için deterministic action sequence kullan.
- [ ] 5. Editor action testini selection→property edit→undo→play→pause→step→stop→save→reopen sırasıyla çalıştır.
- [ ] 6. İkon/font eksikliği, clipped labels, overlap ve unreadable selection için açık başarısızlık listesi tut; reference image'i değiştirme.

**Doğrulama:** Panel oranları ±2 yüzde puanı, clipping yok, bütün action sequence sonuçları doğru.

**Tamamlanma koşulu:** KAPI C: Görsel kanıt ve davranış testleri birlikte mevcut.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E22 başlığına yazılır. Test başarısızsa E22 tamamlandı işaretlenmez.

### E23 — Performans, lifecycle ve son teslim

**Bağımlılık:** E22,E19.

**Ne / neden:** Refaktörün büyük projede kullanılabilirliğini ve sürdürülebilirliğini kanıtlamak.

**Dosyalar:** docs/editor-refactor/PERFORMANCE.md; docs/editor-refactor/FINAL_REPORT.md; bu rehber; mimari doküman; ilgili benchmark test kaynağı.

**Üretilen sözleşme:** CPU/GPU ölçüm raporu, son dosya/API haritası, tamamlanan görev kanıtları.

**Nasıl uygulanacak:**

- [ ] 1. 10k hierarchy+10k asset metadata+5k log fixture'ında UI CPU p50/p95, allocation, cache hit/byte ve thumbnail GPU maliyetini ölç.
- [ ] 2. 60 warmup+300 ölçüm karesi, 3 tekrar; aynı donanım/DPI/resolution/build ve validation durumu ile baseline/yeni kıyasla.
- [ ] 3. 50 panel open/close, 100 resize, 20 project/play switch testinde subscription, descriptor ve texture count'unu izleyerek sızıntı olmadığını doğrula.
- [ ] 4. Mevcut ve yeni CPU/GPU testleri, contract, AstralEditor/Sandbox/EmptyGameTemplate build'lerini çalıştır.
- [ ] 5. Kullanılmayan legacy adaptörleri tüketici taramasıyla kaldır; iki selection veya undo state kaynağı bırakma.
- [ ] 6. Gerçek tamamlanma, ölçümler, deferred runtime özellikleri ve çalıştırılamayan testleri FINAL_REPORT'a yaz; bütün işaretleri kanıta göre güncelle.

**Doğrulama:** 4ms UI p95 öneri hedefi ölçülür; aşılıyorsa bottleneck ve gerçek değer raporlanır. Açıklanmayan >%5 regresyon çözülmeden tamamlandı denmez.

**Tamamlanma koşulu:** KAPI D: Görsel hedef, iş akışları, kaynak ömrü, test ve performans kanıtı tamam.

**Görev kaydı:** Değişen dosyalar, çalıştırılan komut/exit code ve artifact yolu `docs/editor-refactor/EXECUTION_LOG.md` içinde E23 başlığına yazılır. Test başarısızsa E23 tamamlandı işaretlenmez.

## 7. Görsel kabul matrisi

| Senaryo | Beklenen sonuç |
|---|---|
| 1920×1080 / 100% | Referans oranları; büyük viewport; sağ hierarchy/inspector; altta project/console |
| 1366×768 | Min width kuralları; taşmayan Inspector; gerektiğinde alt dock sekmeli |
| 150% / 200% DPI | Metin/ikon bulanıklığı veya çift ölçek yok; hit alanları doğru |
| Selection | Tüm satır vurgusu, okunur foreground, hierarchy/Inspector/viewport tutarlı |
| Property drag | Preview akıcı, bırakınca tek undo; Escape eski değer |
| Play | Toolbar state açık, authoring verisi korunur |
| Game view | Runtime kamera, letterbox koordinat dönüşümü; editor grid/gizmo yok |
| Project loading | Placeholder + yükleniyor; UI donmuyor |
| Missing asset | Açık hata/fallback ikon; sahte preview yok |
| Console burst | Bounded store; doğru dropped count; arama ve scroll kullanılabilir |
| Resize | Eski GPU texture kullanım ömrü korunur; layout kirlenmez |

Görsel toleranslar [tasarım belgesi](EDITOR_REFACTOR_DESIGN.md) Bölüm 7'de. Referanstaki karakter/ağaç/mesh aydınlatmasını UI screenshot puanına katma. 3D içerik kalite karşılaştırması render planının kendi fixture'ına aittir.

## 8. Hata ve geri dönüş rehberi

| Belirti | Önce kontrol et |
|---|---|
| Dil değişince layout kayboluyor | Visible title ile stable ImGui ID ayrılmış mı? |
| Slider undo yüzlerce adım | Begin/Preview/Commit sınırı ve PushAndExecute iki uygulama |
| Gizmo tıklaması entity seçiyor | Input consumption sırası ve viewport hit rect |
| Stop sonrası authoring değişmiş | Clone sahipliği; runtime transaction'ın authoring stack'e girmesi |
| Scene/Game TAA iz bırakıyor | View ID/history izolasyonu ve camera switch reset |
| Asset browser donuyor | Draw içinde disk tarama/decode/sort veya sync thumbnail readback |
| Resize crash | GPU completion öncesi ImGui descriptor/imageView silinmesi |
| Eski asset thumbnail çıkıyor | UUID+revision+generation cache key |
| Çok log RAM büyütüyor | Entry ve byte limit; ingest overflow politikası |
| Panel kapatınca callback crash | Abonelik RAII ve session/panel ömür sırası |

Son görevin kendi değişikliklerini izole et; tüm workspace'e reset yapma. Başarısızlık kaydını, yeniden üretim adımlarını ve son başarılı görevi EXECUTION_LOG'a yaz. Stil toleransını veya test expectation'ını açıklamasız değiştirme.

## 9. Uygulama günlüğü ve teslim biçimi

Her görev için:

```text
Görev ID / başlık:
Durum:
Kaynak commit:
Değişen dosyalar:
Mimari karar / gerekçe:
Davranış değişikliği:
Komutlar / exit code:
Test sonucu:
Screenshot / log / benchmark artifact:
Doğrulanamayan koşul:
Sonraki görev:
```

Son teslim dosyaları:
- `docs/editor-refactor/BASELINE.md`.
- `docs/editor-refactor/EXECUTION_LOG.md`.
- `docs/editor-refactor/THIRD_PARTY_NOTICES.md` — yalnız gerçekten alınmış içerikler.
- `docs/editor-refactor/VISUAL_REVIEW.md`.
- `docs/editor-refactor/PERFORMANCE.md`.
- `docs/editor-refactor/FINAL_REPORT.md`.

Bu dosyalar uygulamada gerçek kanıt oluştuğunda yazılır; plan yazılırken sahte tamamlanma raporu üretilmez.

## 10. Başka AI ajanına verilecek başlangıç metni

```text
AstralEditor refaktörünü uygulamanı istiyorum.
Önce AGENTS.md ve şu üç belgeyi tamamen oku:
docs/PROWL_EDITOR_RESEARCH.md
docs/EDITOR_REFACTOR_DESIGN.md
docs/EDITOR_REFACTOR_IMPLEMENTATION_PLAN.md

Prowl referansına çok yakın layout, tema ve etkileşim kalitesi hedefle.
C++20/ImGui/Vulkan altyapısını koru; C# veya Origami portu yapma.
E00'dan E23'e görevleri bağımlılık sırasıyla uygula.
Her görevde belirtilen testleri ve kabul koşullarını tamamla.
SRP/DRY: panel çizimi, mutation command'ı, session ve GPU kaynak sahipliği ayrı olsun.
Mevcut CommandStack/Selection/Gizmo altyapısını koruyarak dönüştür.
Render refaktörünün tamamlandığını varsayma; gerçek API'yi kontrol et,
viewport bridge üzerinden bağlan ve ortak dosyalarda diğer ajanla çakışma.
Scene/Game view için tek temporal history'yi yanlış paylaşma.
İşlevi olmayan buton veya desteklenmeyen asset için sahte preview üretme.
Her görev sonunda EXECUTION_LOG.md'ye gerçek test ve artifact sonuçlarını yaz.
Kullanıcının mevcut değişikliklerini koru; sadece ilgili dosyaları değiştir.
Görsel, lifecycle, CPU/GPU ve performans kanıtlarıyla FINAL_REPORT.md teslim et.
```

## 11. Son kabul kontrolü

- [ ] Backend/shell/session/commands/panels gerçek sahipliklerle ayrılmış.
- [ ] Menü, toolbar ve shortcut aynı command kaynağını kullanıyor.
- [ ] Inspector bütün kalıcı değişiklikleri transaction/undo üzerinden yapıyor.
- [ ] Typed property erişimi raw offset cast'lerini yeni yoldan kaldırmış.
- [ ] Layout stable ID ve schema ile kalıcı; play authoring kaydını ezmiyor.
- [ ] Theme/font/ikon/DPI ortak token'larla yönetiliyor.
- [ ] Scene camera runtime entity'sini değiştirmiyor; Game input doğru rect'e gidiyor.
- [ ] GPU view/thumbnail kaynakları completion sonrasında emekli ediliyor.
- [ ] Asset ve log read model'leri bounded, cache'li ve çizim dışı iş yapıyor.
- [ ] Referans kompozisyonu farklı çözünürlük/DPI'da doğrulanmış.
- [ ] Mevcut/new testler gerçekten çalıştırılmış, başarısızlıklar saklanmamış.
- [ ] Performans aynı koşullarda ölçülmüş.
- [ ] Runtime'da bulunmayan mesh/script/prefab özellikleri tamamlandı sayılmamış.
- [ ] Kullanıcı değişiklikleri ve alınan içeriklerin lisans kayıtları korunmuş.
