# Materyal kabul sahnesi

Soldan sağa aynı GLB modelinin meşe ve mavi override örnekleri, çift taraflı maskeli yaprak ve transparan cam bulunur. Arka duvar, standart PBR `stone.material` dosyası uygulanmış bir kutudur. İki kapı aynı `door.glb` dosyasından gelir. Yalnızca mavi kapı, sahne belgesinde `oak` slotunu değiştirir.

```sh
cmake --build --preset debug --target material_workflow material_workflow_package
build/debug/examples/material_workflow/material_workflow --raster --capture loose.ppm
build/debug/packages/material_workflow/material_workflow --raster --capture packed.ppm
```

Hibrit RT için `--raster` seçeneğini kaldır. PT için `--path-tracer` kullan. RT özellikleri Vulkan ray query desteği gerektirir. Ham PNG ile cook edilmiş BC texture’lar arasında renk farkı oluşabilir.
