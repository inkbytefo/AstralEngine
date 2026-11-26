# ADR-0053 — Editör: oluşturulan dosyaların geri alınması, ebeveyn döngüsü ve silmede çocuklar

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `editor_core` (`IEditorFileSystem::remove`, `persist_staged_files`, `ReparentEntity`, `DeleteEntity`)
- **İlişkili:** [ADR-0037](0037-ai-first-native-editor-v2.md)

## Bağlam

- Sahne yazımı başarısız olan bir transaction daha önce var olan prefab'ları geri yazıyordu. Kendi **oluşturduğu** prefab dosyalarını ise diskte
  bırakıyordu. `IEditorFileSystem`'de silme işlemi yoktu.
- `ReparentEntity` yalnız kendine ebeveynliği reddediyordu. Bir entity kendi torununun altına taşınabiliyordu (ebeveyn döngüsü).
- `DeleteEntity` çocukları silinen kayda işaret eden `astral::Parent` ile bırakıyordu.

## Karar

- `IEditorFileSystem::remove(path, error)` eklendi (public arayüz; uygulayanlar güncellenmeli). Rollback, transaction'dan önce var olmayan
  dosyaları siler.
- `ReparentEntity` doğrulaması yeni ebeveynden yukarı yürür. Hedefe ulaşırsa `... (cycle)` hatası verir. Yürüyüş kayıt sayısıyla sınırlıdır,
  elle bozulmuş bir dosyadaki döngü sonsuz döngüye yol açmaz.
- `DeleteEntity` alt ağacı silmez. Doğrudan çocukların `astral::Parent`'ı kaldırılır, çocuklar köke taşınır. Alt ağacı silmek isteyen
  istemci çocukları ayrıca siler.

## Sonuçlar

- Yalnız `astral::Parent` temizlenir. Başka component'lerdeki entity referansları silinen kayda işaret etmeye devam edebilir (bilinen sınır).
- Rollback'in kendi yazma/silme hataları hâlâ yok sayılır. Disk tamamen dolduğunda geri alma da başarısız olabilir.
