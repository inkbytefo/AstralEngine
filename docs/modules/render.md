# Render (`Astral::Render`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core, Astral::Gpu, Astral::Shader (`modules/render/CMakeLists.txt:60-63`); derleme zamanında stb_truetype ve Dear ImGui'nin Roboto fontu indirilir (`modules/render/CMakeLists.txt:4-15`) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Render modülü, ECS'i bilmeyen bir kare çizicidir: çağıran taraf her kare `RenderScene` (mesh örnekleri, SDF şekilleri, ışıklar, güneş,
ortam, decal, duman, partikül, debug çizgisi, 2D katman) adlı düz veriyi verir, `Renderer::render()` bunu bir `gpu::CommandList`'e
kaydeder (`modules/render/include/astral/render/renderer.hpp:245-265`, `:425`). İki ana yol vardır: cihaz donanımsal ray query destekliyorsa
hibrit ışın izlemeli yol (RT gölge, DDGI, yansıma, denoiser, TAA, bloom), değilse raster yol; ayrıca RT donanımı gerektiren bir path tracer
referans modu vardır. Modül sahne yönetimi, culling hiyerarşisi, animasyon/skinning, LOD, ses ya da asset yükleme yapmaz; dosyadan materyal
okuma `Astral::App` içindeki `MaterialLibrary`'dedir (`modules/app/src/material_library.cpp`), bu modül yalnız `.material` çözümleme ve
`.ashader` derleme yardımcılarını verir.

## Public API

Hepsi `modules/render/include/astral/render/` altındadır. Renderer'ın tüm yüzeyi tek sınıftadır (`Renderer`); diğer tipler düz veridir.

### Renderer cephesi

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Renderer(device, target_format, settings)` | Store'ları, pass'leri ve RenderGraph'ı kurar. `ray_tracing` ayarı açık ve cihaz destekliyorsa RT pipeline'larını hazırlar; başaramazsa raster yola düşer | `modules/render/include/astral/render/renderer.hpp:365`, `modules/render/src/renderer.cpp:81-86` |
| `render(cmd, target, camera, scene)` | Kareyi `cmd`'ye kaydeder, `target` ColorTarget durumunda biter. Halka tamponlar GPU ile kendiliğinden senkronlanır | `modules/render/include/astral/render/renderer.hpp:425`, `modules/render/src/renderer.cpp:217-233` |
| `render_ui(cmd, target, ui, clear)` | Hedefi temizleyip yalnız 2D katmanları çizer (3B sahne başka bir render target'a çizildiyse) | `modules/render/include/astral/render/renderer.hpp:429`, `modules/render/src/renderer.cpp:235-246` |
| `create_mesh` / `update_mesh` / `destroy` | Mesh yükler (bloklayıcı). `update_mesh` aynı handle'ın arkasındaki geometriyi değiştirir. İndeks vertex sayısını aşıyorsa reddeder | `modules/render/include/astral/render/renderer.hpp:371-374`, `modules/render/src/stores/mesh_store.cpp:397-403` |
| `create_texture` / `update_texture` / `destroy` | RGBA8 görüntü (mip zinciri otomatik) ya da hazır BC7/BC5 zinciri yükler | `modules/render/include/astral/render/renderer.hpp:377,407-408` |
| `create_render_target` / `resize_render_target` / `native_target` | Offscreen renkli hedef; UI katmanında örneklenebilir | `modules/render/include/astral/render/renderer.hpp:379-383` |
| `create_material` / `update_material` / `destroy` / `default_material` | Materyal handle'ı; geçersiz handle varsayılan materyale düşer | `modules/render/include/astral/render/renderer.hpp:410-416` |
| `create_shader(SurfaceShaderBuild \| ScreenShaderBuild)` / `destroy` | Derlenmiş `.ashader` için pipeline'lar; materyal `MaterialDesc::shader` ile bağlanır | `modules/render/include/astral/render/renderer.hpp:387-390` |
| `set_shader_compile_services` / `set_job_system` / `set_busy_callback` | SPIR-V önbelleği + iş sistemi, mip işçileri, uzun yüklemede pencereyi canlı tutma kancası (hepsi ödünç alınır) | `modules/render/include/astral/render/renderer.hpp:394-400` |
| `shader_error()` | Özel yüzeylerin RT shader'larına girememe nedeni ("" = sorun yok) | `modules/render/include/astral/render/renderer.hpp:403` |
| `supports(ImageFormat)` | BC için `gpu::Feature::TextureCompressionBC` gerekir | `modules/render/include/astral/render/renderer.hpp:405`, `modules/render/src/stores/texture_store.cpp:271-273` |
| `set_settings` / `settings` | Çalışma anında ayar değişimi; ilgili geçmiş sıfırlanır | `modules/render/include/astral/render/renderer.hpp:420-421`, `modules/render/src/renderer.cpp:183-205` |
| `stats` / `gpu_timings` / `gpu_frame_ms` / `ray_tracing` / `reset_history` | İstatistik, pass başına GPU süresi (birkaç kare gecikmeli), aktif yol, kamera kesmesinde geçmiş sıfırlama | `modules/render/include/astral/render/renderer.hpp:432-439` |
| `build_mips(image, jobs)` | Kutu filtreli RGBA8 mip zinciri; renk görüntüleri doğrusal uzayda ortalanır, iş sistemiyle aynı baytları üretir | `modules/render/include/astral/render/renderer.hpp:44`, `modules/render/src/stores/texture_store.cpp:233-260` |

### Sahne verisi ve ayarlar

| Tip | Ne yapar | Kaynak |
|---|---|---|
| `RenderScene` | Kare verisi: `meshes`, `sdf`, `lights`, `rect_lights`, `decals`, `sun`, `environment`, `debug_lines`, `ui`, `time`, `sky`, `post`, `world_origin`, `smoke`, `particles`. Span'ler `render()` dönene kadar geçerli kalmalı | `modules/render/include/astral/render/renderer.hpp:245-265` |
| `MeshInstance` | Mesh + materyal + dönüşüm; `previous_transform` verilirse nesne başına hareket vektörü | `modules/render/include/astral/render/renderer.hpp:143-149` |
| `MaterialDesc` | PBR faktörleri + dokular + M13 katmanları + `TextureMapping` + özel shader parametre bloğu | `modules/render/include/astral/render/renderer.hpp:98-127` |
| `ClearcoatDesc`, `SheenDesc`, `AnisotropyDesc`, `TransmissionDesc`, `SubsurfaceDesc`, `DetailDesc` | M13 gelişmiş yüzey katmanları | `modules/render/include/astral/render/renderer.hpp:47-83` |
| `TextureProjection`, `TextureMapping` | `MeshUV`, `PlanarXY/XZ/YZ`, `Triplanar` | `modules/render/include/astral/render/renderer.hpp:85-95` |
| `Decal` | Kutu izdüşümlü deferred decal (boya, ıslaklık, normal) | `modules/render/include/astral/render/renderer.hpp:130-140` |
| `SdfShape`, `SdfPrimitive`, `SdfOperation` | Liste sırasıyla birleşen SDF şekilleri (küre, kutu, torus, kapsül, silindir; birleşim/çıkarma/kesişim/yumuşak varyantlar) | `modules/render/include/astral/render/renderer.hpp:151-168` |
| `PointLight`, `RectLight`, `DirectionalLight` | Işık tipleri. `RectLight` yalnız ışın izlemeli yolda; **SpotLight yok** | `modules/render/include/astral/render/renderer.hpp:170-203` |
| `Environment` | Gök/zemin rengi, ortam gücü, pozlama, aerial perspective, froxel sis | `modules/render/include/astral/render/renderer.hpp:205-214` |
| `SmokeVolumeView`, `ParticleBurst` | Yerel duman hacmi, GPU partikül patlaması | `modules/render/include/astral/render/renderer.hpp:217-242` |
| `GiSettings` | DDGI ızgarası (probe sayısı, ışın sayısı, histerezis, hacim) | `modules/render/include/astral/render/renderer.hpp:268-274` |
| `RenderSettings` | Tüm kalite/teknik ayarları (varsayılanlar aşağıda tablo) | `modules/render/include/astral/render/renderer.hpp:280-318` |
| `RenderStats`, `PathTracerReset` | Kare istatistiği; path tracer birikiminin neden yeniden başladığı | `modules/render/include/astral/render/renderer.hpp:322-352` |
| `DebugView` | `None, Albedo, Normals, DiffuseLight, Specular, Motion` (RT yolu için) | `modules/render/include/astral/render/renderer.hpp:278` |

### Shader, materyal dosyası, RenderGraph, yardımcılar

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `compile_surface_shader(source, name, services)` | `.ashader` yüzeyini G-buffer, gölge, (alfa testliyse) gölge fragment'i ya da forward (`blend_*`) aşamalarına çevirir; iş parçacığı güvenli | `modules/render/include/astral/render/surface_shader.hpp:68` |
| `compile_screen_shader` | `post`, `sky`, `particle`, `volume` shader türleri | `modules/render/include/astral/render/surface_shader.hpp:63` |
| `compose_rt_surfaces`, `compile_rt_variants` | Özel `surface()` fonksiyonlarını RT compute shader'larına gömer (5 shader) | `modules/render/include/astral/render/surface_shader.hpp:81,93` |
| `resolve_material`, `evaluate_uniforms` | `.material` ebeveyn zinciri (en çok 16 derinlik, döngü hatadır) ve değerlerin parametre bloğuna çevrilmesi; saf veri, GPU/dosya sistemi yok | `modules/render/include/astral/render/material_file.hpp:40,56`, `modules/render/src/material_file.cpp:12,25-60` |
| `RenderGraph`, `PassBuilder`, `PassContext`, `RGTexture` | Her kare yeniden kurulan kare grafiği; bariyer, render pass açma, geçici texture havuzu, kullanılmayan pass'leri ayıklama | `modules/render/include/astral/render/render_graph.hpp:71`, `:37`, `:60`, `:17` |
| `DebugDraw` | Kare içi debug çizgi toplayıcı (kutu, küre, kapsül, ok...) | `modules/render/include/astral/render/debug_draw.hpp:31` |
| `UiLayer`, `UiCommand`, `UiCanvas`, `Font` | Piksel uzayında 2D geometri; SDF font atlası (Latin-1 + Latin Extended-A + ₺) | `modules/render/include/astral/render/ui.hpp:43,31`, `modules/render/include/astral/render/text.hpp:98,52` |
| `MeshData`, `make_cube/plane/sphere`, `ImageData` | Mesh ve görüntü veri tipleri, ilkel mesh üreticileri | `modules/render/include/astral/render/mesh.hpp:18,24-28`, `modules/render/include/astral/render/image.hpp:13` |
