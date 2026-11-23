# Astral Editor — Kullanıcı ve Mimar Rehberi

Astral Editor (`AstralEditor` / `world_editor`), **AI-First Native** tasarım felsefesiyle oluşturulmuş bağımsız masaüstü editörüdür ([ADR-0035](../decisions/0035-standalone-ai-assisted-editor-core.md), [ADR-0036](../decisions/0036-world-authoring-and-level-instances.md), [ADR-0037](../decisions/0037-ai-first-native-editor-v2.md)).

> **Ürün denklemi:** `Niyet → Ajan Planı → Önerilen Değişiklik → 3D Önizleme & Fark → İnsan Kararı → Doğrulanmış Sonuç`

---

## 1. İlk Açılış ve Çalıştırma

### Derleme ve Başlatma
```bash
cmake --preset debug
cmake --build --preset debug --target AstralEditor
./build/debug/examples/world_editor/AstralEditor
```

### Komut Satırı Seçenekleri
- `--advanced`: Editörü doğrudan **Advanced** çalışma alanı düzeninde başlatır.
- `--width <px> --height <px>`: Pencere veya yakalama çözünürlüğü (varsayılan `1600×900`).
- `--capture <dosya.ppm>`: Başsız offscreen GPU görüntüsü yakalar (görsel doğrulama için).

---

## 2. Çalışma Alanları ve Yerleşim Kalıcılığı

- **Focus (Varsayılan):** İlk açılışta yalnızca üst araç çubuğu, dar sol ikon rayı (`50 px`), aramalı sahne ağacı (`Scene`), baskın canlı **3D Viewport**, tek aktif görevli **Astral Agent** paneli ve ince durum çubuğu görünür. `1280×720` ve altındaki ekranlarda alt içerik rafı otomatik kapalı başlar.
- **Bağlamsal Inspector:** Focus modunda kalıcı değildir; sahne ağacında veya viewport'ta bir nesneye **çift tıklayarak**, `I` kısayoluyla veya sol raydaki Inspector ikonuyla açılır.
- **Advanced:** Üst çubuktaki `Focus ▾` menüsünden veya `Ctrl+K` komut paletinden açılır. Inspector, Content Shelf, World Authoring, Console, Profiler (CPU + GPU pass süreleri), Render & Environment ve Materials panellerini aynı editör örnekleriyle görünür kılar.
- **Yerleşim Kaydı & Sıfırlama:** Pencere düzeni `astral_editor_layout.ini` dosyasına kaydedilir. Üst menüden veya `Ctrl+K` → **Reset Workspace Layout** ile her zaman temiz varsayılan düzene dönülür.

---

## 3. Sahne, Dünya ve Asset Düzenleme

### Viewport Etkileşimi ve Kısayollar

| Kısayol | Eylem |
|---|---|
| `RMB + WASD / QE` (`Shift` hızlandırır) | Viewport içinde uçuş kamerası |
| Fare tekerleği | Viewport ileri/geri yaklaşma |
| `LMB` / Çift Tık | 3D nesne seçimi / Bağlamsal Inspector aç |
| `Q` / `W` / `E` / `R` | Seç / Taşı / Döndür / Ölçekle gizmosu |
| `F` | Seçili nesneye odaklan |
| `I` | Bağlamsal Inspector'ı aç/kapat |
| `Ctrl+K` | Komut Paleti (eylemler ve entity araması) |
| `Ctrl+S` | Atomik kayıt (`● Saved` / `● Unsaved`) |
| `Ctrl+Z` / `Ctrl+Y` | Geri Al / Yinele (gizmo/slider sürüklemeleri birleştirilir) |
| `Del` | Seçili nesneyi sil |

### Kayıt, Çakışma ve Kurtarma
- Etkileşimli düzenlemeler (gizmo, Inspector, sahne ağacı) bellek içi transaction + undo yığınına yazılır ve canlı sahneye anında yansır.
- `Ctrl+S` geçici dosya + flush + atomik `rename` ile diske yazar. Dosya dışarıdan değiştirilmişse **External File Change** penceresi açılarak *Reload* veya *Overwrite* seçeneği sunulur.
- Kirli belgeler periyodik olarak `<belge>.astral_autosave` yan dosyasına yedeklenir; temiz kayıtta silinir ve bir sonraki açılışta kurtarma seçeneği sunulur.

### Play Modu İzolasyonu
Üst çubuktaki **Play** düğmesi, kaydedilmemiş düzenlemeler dâhil mevcut belgeden **ayrı bir ECS `World` ve `PhysicsWorld`** başlatır. **Pause/Resume** ile simülasyon dondurulabilir; **Stop** ile editör dünyasına sıfır veri kirliliğiyle dönülür.

### `.world` ve Level Instance Düzenleme
İçerik rafından veya üst seçiciden bir `.world` haritası açıldığında **World Authoring** paneli cook hücrelerini, veri katmanlarını, floating origin kaydırmasını, veri güdümlü yerleştirme araçlarını, **Instance Override** vs **Edit-in-Context** işlemlerini ve `.world`/`.ent` Undo/Redo desteğini sunar.

---

## 4. AI Görev Akışı, Önizleme ve Güvenlik

Astral Agent bir sohbet penceresi değil, **7 durumlu görev makinesidir**:
`Idle` → `Planning` → `Executing` → `Review` → `Blocked` / `Failed` / `Complete`.

1. **Bağlam:** Seçili nesne otomatik olarak görünür ve kaldırılabilir bir bağlam rozetiyle eklenir.
2. **İzole 3D Önizleme:** Ajan önerisi (`EditorCommand[]`) şema ve yol güvenliği (`..` / path traversal reddi) kontrolünden geçer, geçici ECS `PreviewSandbox` dünyasında doğrulanır ve viewport'taki **Before / After (AI)** düğmesiyle gerçek 3D sahne olarak incelenebilir.
3. **İnsan Kararı:** **Apply** (seçili komutları uygular), **Revise** (düz dilde geri bildirimle yeni öneri ister) veya **Discard** (hiçbir dosyaya dokunmadan vazgeçer).
4. **Sağlayıcılar & Politika:**
   - **Built-in Offline:** Çevrimdışı, deterministik yerleşik ajan (AI/ağ olmadan da editörün tamamı çalışır).
   - **Codex CLI (Local):** `Policy` menüsünden `Allow external CLI commands` açıkça etkinleştirildiğinde yerel `codex` ikilisini yapılandırılmış JSON şema protokolüyle arka plan iş parçacığında çalıştırır.
   - Uygulama öncesi `DocumentRevision` tekrar doğrulanır; bayat öneriler reddedilir ve her karar **History / Audit Log** sekmesinde saklanır.

---

## 5. Sorun Giderme

- **Docking sıfırlama:** Düzen bozulursa `Ctrl+K` → `Reset Workspace Layout` veya `astral_editor_layout.ini` dosyasını silin.
- **Codex CLI çalışmıyor:** `Policy` → `Allow external CLI commands` kutusunun işaretli olduğunu ve `codex --version` komutunun PATH'te bulunduğunu doğrulayın.
- **Yüksek DPI ölçekleme:** Editör pencere yoğunluğunu (`pixel_density`) otomatik algılar ve fontları `FontScaleDpi` ile fiziksel çözünürlükte net çizer.
