# Astral::Render

Katman 2. Gpu üzerinde çizim. **ECS'i bilmez**: bir `DrawItem` listesi çizer; listeyi App,
`MeshRenderer` + `WorldTransform` component'lerinden üretir (tek sorumluluk).

```cpp
astral::Renderer renderer(device, swapchain->format());
MeshHandle cube = renderer.create_mesh(astral::make_cube());
renderer.render(cmd, target, camera, items, light);
```

## M2 durumu: forward renderer

- Instanced çizim: aynı mesh'i kullanan tüm nesneler **tek draw call** (1000 küp = 1 çağrı).
- Vertex pulling: vertex/instance/frame verisi buffer adresleriyle shader'a; vertex input yok.
- Derinlik testi, back-face culling, Lambert + ambient yönlü ışık.
- Kare başına veriler 3'lü halka buffer'da (en fazla 2 kare uçuşta).
- Primitifler: `make_cube`, `make_plane`, `make_sphere`.

## Sonraki faz (M3)

Render graph; deferred G-buffer, gölgeler, PBR, tonemap/TAA ve eski v2'deki SDF hibrit teknikler bu
API'nin arkasına taşınacak. `Renderer` arayüzü oyun tarafında aynı kalacak.

## Bilinen sınırlar

- Doku/materyal yok (renk başına instance); normal matrisi uniform ölçek varsayar.
- Gölge, şeffaflık sıralaması, frustum culling yok.
