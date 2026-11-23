# ADR-0036: Dünya Yazarlığı, Level Instance Sahipliği ve Veri Güdümlü Yerleştirme (E1)

**Tarih:** 2026-11-23 · **Durum:** Kabul edildi · **Faz:** E1

## Bağlam

M9 ile `.world` (`astral.map` v1), nesne başına `.ent` (`astral.entity` v1), hücre cook'u (`.cell`), `CellStreamer`, `MapLayers`, `MapState` ve `FloatingOrigin` kuruldu; E0 ile de GPU'suz `Astral::Editor_core` ve `Astral::Editor` eklendi. E1 fazında büyük açık dünyaları (2×2 km ve üzeri) düzenlemek, yeniden kullanılabilir sahneleri Level Instance olarak yerleştirmek ve veri güdümlü içerik üretimi sağlamak gerekir.

## Karar

### 1. Level Instance Kimliği, Dönüşüm ve Sahiplik Ayrımı
- **Üç ayrı kimlik katmanı korunur:**
  1. **Kaynak sahne:** `scene_path` (`astral.scene` v1).
  2. **Yerleştirilen instance:** `.ent` kaydındaki benzersiz `instance_id` (`MapEntity::id`).
  3. **İç entity kimliği:** kaynak sahnedeki kararlı `local_path` (örn. `"1"` veya iç içe instance'ta `"2/1"`). Genişletildiğinde her iç entity deterministik ve instance'lar arası çarpışmasız kalıcı bir `MapEntity::id` (FNV-1a tabanlı 31-bit pozitif kimlik + çarpışma koruması) ve `LevelInstanceMember { instance_id, source_scene, local_path }` bileşeni alır.
- **Placement, entity parenting'e bağlanmaz:** Bir Level Instance yerleştirildiğinde (`position`, `rotation`, `scale`), yerleşim dönüşümü her iç kök entity'nin dünya `Transform`'una matematiksel olarak birleştirilir (`placement * local`). Yapay bir üst `Parent` entity oluşturulmaz; böylece her iç entity kendi hücre sahipliğini, fizik gövdesini ve bağımsız streaming yaşam döngüsünü korur.
- **Tam alt-harita (`.world` içinde `.world`) kapsamı:** E1'de Level Instance kaynağı yalnızca `astral.scene` v1 belgeleridir (iç içe `level_instance` ve `prefab` içerebilir). Birden fazla `.world` streaming ızgarasını iç içe gömmek (sub-world) E1 kapsamı dışındadır; gerekirse ayrı ADR ile ele alınır.

### 2. Edit-in-Context vs Instance Override
- **Instance Override:** Yalnızca hedef instance'ın `.ent` dosyasındaki `level_instance.overrides[local_path]` yamasına yazar. Aynı kaynak sahnenin diğer instance'ları etkilenmez ve dosya diff'i yalnızca o `.ent` dosyasında çıkar.
- **Edit-in-Context:** Kaynak `.scene` belgesini doğrudan atomik transaction ile değiştirir. Hot reload / yeniden genişletme sırasında tüm instance'lar yeni kaynak sahneyi alır, her instance'ın kendi `overrides` ve `deleted` listesi kaynak üzerine yeniden birleştirilir.

### 3. Nesting, Döngü Teşhisi, Hücre Sahipliği, Cross-Cell Refs ve Sınırlar
- `LevelInstanceLimits` (`max_depth = 8`, `max_entities = 10000`) özyinelemeli genişletmeyi sınırlar; döngüsel referanslar (`a.scene -> b.scene -> a.scene`) ziyaret yığınıyla yakalanıp açık teşhis üretir.
- Genişletilen her iç entity kendi dünya konumuna göre `assign_cells()` ve `cook_map()` ile hücrelere dağıtılır; geniş bir Level Instance birden fazla hücreye yayılabilir.
- **`CrossCellRef` bileşeni (`target_id`, `target_label`):** Farklı hücrelerdeki veya farklı Level Instance'lardaki entity'ler arasında kalıcı `MapEntity::id` referansları taşır. Hedef hücre yüklü değilken referans `MapEntityStatus::Unloaded` olarak korunur; hücre unload/reload edildiğinde `MapState::lookup()` üzerinden yeniden bağlanır.

### 4. Sahiplik Ayrımı ve Veri Güdümlü Yerleştirme
- **Üç ayrı çalışma alanı:**
  - `Scene → World` (`load_scene`),
  - `WorldView / PreviewSandbox` (geçici önizleme dünyası),
  - Oynanabilir harita (`MapRuntime` / `MapState`) birbirinden ayrı kalır.
- **`WorldAuthoringSession` (`Astral::Editor_core`):** `.world` ve `.ent` dosyalarını atomik yazar, undo/redo ve `AuditLog` tutar, cook hücreleri / veri katmanları / floating origin incelemesi sunar.
- **Veri güdümlü yerleştirme araçları (`PlacementToolDef`):** Yol (`Road`), bina (`Building`), bitki (`Foliage`), kaya (`Rock`), `Prefab` ve `LevelInstance` araçları JSON tanımlıdır; `IHeightSampler` kancasıyla M15 arazi yükseklik çarpışması hazır olduğunda doğrudan birleşir.

## Sonuçlar
- 2×2 km haritada iki bağımsız Level Instance, kaynak düzenleme, instance override, hot reload override birleşimi, cook tutarlılığı ve hücre unload/reload sonrası kimlik/referans korunumu uçtan uca test edilir.
- Mevcut `MapDesc` / `MapSource` / `cook_map` / `MapState` sözleşmeleri geriye uyumlu kalır.
