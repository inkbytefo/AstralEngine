# Terrain Lab

Bu örnek, bir `.scene` dosyasındaki `TerrainRef` ile 65 × 65 örnekli bir terrain tile’ı yükler. Render mesh’i ve statik Jolt collider’ı aynı yükseklik örneklerinden üretilir. `ground.ashader` metre cinsinden XZ UV’lerini kullanır.

```sh
cmake --build --preset debug --target terrain_lab_package
build/debug/examples/terrain_lab/terrain_lab --capture terrain-loose.ppm --width 640 --height 360
build/debug/packages/terrain_lab/terrain_lab --capture terrain-pack.ppm --width 640 --height 360
```

Bu aşamada tile’lar ilk senkronizasyonda tam çözünürlükte yüklenir. Terrain entity’si öteleme destekler. Döndürme, ölçek ve parent kullanımı uyarı üretir. Asimetrik 65 × 65 test verisinde render üçgenlerinin merkezleri ile collider raycast yüksekliği arasında en fazla 2,5 cm fark kabul edilir. Bu eşik test sahnesine aittir. Jolt sıkıştırma hatası yükseklik dağılımına göre değişebilir.
