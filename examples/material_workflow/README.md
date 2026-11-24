# Materyal kabul sahnesi

Soldan sağa aynı GLB modelinin meşe ve mavi override örnekleri, çift taraflı maskeli yaprak ve transparan cam bulunur. Arka duvar, standart PBR `stone.material` dosyası uygulanmış bir kutudur. İki kapı aynı `door.glb` dosyasından gelir. Yalnızca mavi kapı, sahne belgesinde `oak` slotunu değiştirir.

```sh
cmake --build --preset debug --target material_workflow material_workflow_package
build/debug/examples/material_workflow/material_workflow --raster --capture loose.ppm
build/debug/packages/material_workflow/material_workflow --raster --capture packed.ppm
```

Hibrit RT için `--raster` seçeneğini kaldır. PT için `--path-tracer` kullan. RT özellikleri Vulkan ray query desteği gerektirir. Ham PNG ile cook edilmiş BC texture’lar arasında renk farkı oluşabilir.

## UV projeksiyonları

Standart PBR materyalinin `values.mapping` alanı `MeshUV`, `PlanarXY`, `PlanarXZ`, `PlanarYZ` ve `Triplanar` modlarını destekler.

```json
{
  "format": "astral.material",
  "version": 1,
  "type": "pbr",
  "values": {
    "base_color_texture": "textures/stone.png",
    "mapping": {
      "projection": "Triplanar",
      "tile_size_m": 2.0,
      "blend_sharpness": 4.0,
      "world_space": true
    }
  }
}
```

`MeshUV` için `uv_scale`, `uv_rotation` (radyan) ve `uv_offset` kullanılır. Planar ve triplanar modları için `tile_size_m` ve `uv_offset` kullanılır. `world_space: false`, izdüşümü nesneye bağlar ve pozisyonu metre cinsinden nesnenin eksenlerinde ölçer. Projeksiyon, ana PBR texture’larına uygulanır. Detail ve parallax katmanları mesh UV’sini kullanmaya devam eder.

`materials/uv_inspect.material` geçici tanı materyalidir. `second_uv` ikinci kanalı gösterir. `show_density` rasterda metredeki texel yoğunluğunu renklendirir. Mavi hedefin altı, yeşil hedefe yakın, kırmızı hedefin üstüdür. `reference_resolution` kare bir texture’ın piksel kenar ölçüsüdür.
