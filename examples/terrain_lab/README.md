# Terrain Lab

Bu örnek, bir `.scene` dosyasındaki `TerrainRef` ile 2 × 2 komşu terrain tile’ını (tile başına 65 × 65 örnek) yükler. Render mesh’i ve statik Jolt collider’ı aynı yükseklik örneklerinden üretilir. `ground.ashader` metre cinsinden XZ UV’lerini kullanır.

```sh
cmake --build --preset debug --target terrain_lab_package
build/debug/examples/terrain_lab/terrain_lab --capture terrain-loose.ppm --width 640 --height 360
build/debug/packages/terrain_lab/terrain_lab --capture terrain-pack.ppm --width 640 --height 360
```

Bu aşamada tile’lar ilk senkronizasyonda yüklenir. Yakın tile’lar 65 × 65, uzak tile’lar 33 × 33 render vertex’i kullanır. Kamera geçişleri için hysteresis vardır. Fizik her zaman 65 × 65 örneği kullanır. Varsayılan kamera iki LOD seviyesini birlikte gösterir. Paketli çalıştırmaya `--far` ekleyerek uzak görünümü alabilirsin. Terrain entity’si öteleme destekler. Döndürme, ölçek ve parent kullanımı uyarı üretir. Asimetrik 65 × 65 test verisinde render üçgenlerinin merkezleri ile collider raycast yüksekliği arasında en fazla 2,5 cm fark kabul edilir. Bu eşik test sahnesine aittir. Jolt sıkıştırma hatası yükseklik dağılımına göre değişebilir.

`TerrainRef.render_radius_m` ve `collision_radius_m` değerleri streaming’i açar. Her iki değer de varsayılan olarak sıfırdır ve ilgili kaynağı yüklü tutar. Pozitif collider yarıçapı, pozitif görsel yarıçaptan küçükse görsel yarıçapa yükseltilir. Mevcut `StreamingSource` konumları kullanılır. Aktif kaynak yoksa birinci aktif kamera kullanılır. Karakterler yakın collider’ları yüklü tutar. `ctx.maps().finish_world_loading()` mevcut odak çevresini, yükleme ekranı gibi beklemenin uygun olduğu bir yerde, eşzamanlı bitirir.

Eksik bir tile uyarı verir. Kaynak bölgeden çıkıp yeniden girerse tile tekrar istenir. Karakter, collider’ı bekleyen tanımlı tile’a adım atamaz. Bu koruma `CharacterController` için uygulanır. Serbest rijit gövde simülasyonu için ayrı bir streaming politikası gerekir.
