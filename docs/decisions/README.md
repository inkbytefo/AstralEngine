# Mimari Karar Kayıtları (ADR)

Her önemli karar bir dosya: bağlam, karar, sonuçlar. Kararlar silinmez; değişirse yeni
ADR eskisini "yerini aldı" diye işaretler.

| No | Karar | Durum |
|---|---|---|
| [0001](0001-library-not-framework.md) | Motor bir kütüphane, oyun main loop'un sahibi | Kabul |
| [0002](0002-module-per-folder.md) | Her modül kendi klasöründe, `include/astral/<modül>/` | Kabul |
| [0003](0003-vulkan-only-own-rhi.md) | Kendi RHI'mız, ilk ve tek backend Vulkan 1.3 | Kabul |
| [0004](0004-helping-wait-own-group.md) | Yardımlı `wait()` sadece kendi grubunu çalıştırır | Kabul |
| [0005](0005-own-ecs.md) | Kendi ECS'imiz; EnTT kullanılmaz | Kabul |
