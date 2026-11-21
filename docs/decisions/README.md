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
| [0006](0006-app-layer-for-games.md) | Oyunlar App + Game + System ile yazılır | Kabul |
| [0007](0007-sdl3-platform.md) | Platform katmanı SDL3 | Kabul |
| [0008](0008-bindless-bda-glsl.md) | Bindless texture, buffer adresleri, GLSL | Kabul |
| [0009](0009-render-graph-hybrid-deferred.md) | Render graph üzerinde hibrit deferred renderer | Kabul |
| [0010](0010-jolt-physics.md) | Fizik Jolt üzerine, ECS'ten bağımsız modül | Kabul |
| [0011](0011-hybrid-ray-traced-lighting.md) | Donanımsal ray query ile hibrit fotogerçekçi aydınlatma | Kabul |
| [0012](0012-render-api-contract.md) | Render API sözleşmesi; ECS, sahne ve paketleme yönü | Kabul |
| [0013](0013-ecs-change-detection-reflection.md) | Değişiklik takibi, reflection ve JSON serileştirme | Kabul |
