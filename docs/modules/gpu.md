# Astral::Gpu

Katman 1. Modern Vulkan 1.3 üzerine kendi RHI'mız ([ADR-0003](../decisions/0003-vulkan-only-own-rhi.md),
[ADR-0008](../decisions/0008-bindless-bda-glsl.md)). Public API'de Vulkan tipi yok.

```cmake
target_link_libraries(MyGame PRIVATE Astral::Gpu)
```

## Temel kavramlar

| Kavram | Açıklama |
|---|---|
| `Device` | GPU. Tüm kaynakların sahibi, komut listesi verir, gönderir. `Device::create()` cihaz yoksa `nullptr`. |
| Handle'lar | `BufferHandle`, `TextureHandle`, `PipelineHandle` — ham pointer yok; silinen handle hiçbir şeye çözülmez. |
| `CommandList` | Komut kaydı. `begin_commands()` → kaydet → `submit()` → `SubmitTicket`. |
| `SubmitTicket` | Gönderim numarası; `wait(ticket)` / `is_complete(ticket)`; `cmd.on_submitted(fn)` listenin numarasını bildirir. |
| `Swapchain` | Pencereye çizim; boyut değişimini kendisi yönetir. |
| Bindless | Texture'lar tek bir dev tabloda: shader'a sadece `bindless_index(texture)` verilir. |
| Buffer adresleri | Shader'lar buffer'lara `buffer_address(buffer)` ile pointer gibi erişir (BDA). |

## Örnek: compute

```cpp
auto device = astral::gpu::Device::create({.validation = true});
auto buffer = device->create_buffer({.size = 4000});
auto pipeline = device->create_compute_pipeline({.shader = {astral::shaders::fill_comp}});

struct Push { uint64_t address; uint32_t count; } push{device->buffer_address(buffer), 1000};
auto cmd = device->begin_commands();
cmd.bind_pipeline(pipeline);
cmd.push_constants(&push, sizeof(push));
cmd.dispatch(16);
device->wait(device->submit(cmd));
```

## Örnek: pencereye çizim

```cpp
auto image = swapchain->acquire();          // geçersizse bu kare çizme (küçültülmüş pencere)
auto cmd = device->begin_commands();
cmd.transition(image, gpu::ResourceState::ColorTarget);
// ... çizim ...
cmd.transition(image, gpu::ResourceState::Present);
swapchain->present(cmd);                    // gönderir (görüntüyü bekleyerek) ve gösterir
```

Alt katmanı doğrudan kullanan örnek `tests/integration` içinde (pencere + swapchain + present). Swapchain, Device'tan önce yok edilmeli. `examples/hello_triangle` GPU'yu değil `App` + özel `.ashader` materyalini kullanan en küçük resimdir.

## Shader'lar

GLSL yazılır, CMake'te `astral_add_shaders(<target> SOURCES a.vert b.frag)` ile derlenip
programa gömülür; `#include <shaders/a.vert.hpp>` → `astral::shaders::a_vert`. Her shader
`#include <astral/bindless.glsl>` ile ortak tabloyu ve `ASTRAL_BUFFER(Ad, Tip)` makrosunu alır.
Push constant sınırı 128 byte.

## Sıkıştırılmış texture'lar (M6)

`Format::BC5Unorm`, `BC7Unorm`, `BC7Srgb`: 4×4 blok, blok başına 16 bayt. Yalnız `Sampled` olabilir
(render hedefi veya storage olamaz); `supports(Feature::TextureCompressionBC)` gerekir (masaüstü GPU'ların
hepsinde ve lavapipe'ta var). `write_texture` her mip'e `texture_data_size(format, w, h)` bayt bekler;
kısmi bloklar yukarı yuvarlanır (2×2 mip = 1 blok). `is_compressed(format)`.

## Ray tracing ([ADR-0011](../decisions/0011-hybrid-ray-traced-lighting.md))

`supports(Feature::RayTracing)` ise (`VK_KHR_acceleration_structure` + `VK_KHR_ray_query`; lavapipe dahil):

```cpp
auto blas = device->create_blas({.vertices = vb, .vertex_count = n, .vertex_stride = 32,
                                 .indices = ib, .index_count = m});      // bir kez, bloklayan
auto tlas = device->create_tlas({.max_instances = 1024});
cmd.build_tlas(tlas, instances);   // her kare; öncesi/sonrası bariyer otomatik
push.tlas = device->acceleration_structure_address(tlas);
```

Shader'da TLAS'a **adresle** ulaşılır, descriptor yok:
`rayQueryInitializeEXT(q, accelerationStructureEXT(pc.tlas), ...)`. Ray tracing pipeline'ı (SBT) yok;
ışınlar compute/fragment içinde ray query ile atılır. `TlasInstance::custom_index` (24 bit) shader'da
`rayQueryGetIntersectionInstanceCustomIndexEXT` ile okunur (ör. materyal indeksi).

## Senkronizasyon ve yaşam süresi

- Tek **timeline semaphore**; her `submit` değeri bir artırır.
- `destroy(handle)` handle'ı hemen öldürür, belleği GPU o ana kadarki işleri bitirince siler
  (gecikmeli silme). Bindless indeks de o zaman yeniden kullanılabilir olur.
- Texture durumları **kayıt anında** izlenir: `transition(tex, state)` önceki durumdan bariyer
  üretir. Bu yüzden komut listeleri kaydedildikleri sırayla gönderilmeli. (M3'te render graph
  bariyerleri otomatik yönetecek.)
- `write_buffer` / `read_buffer` kolaylık içindir: staging gerekiyorsa bekler (her karede değil,
  yükleme sırasında kullan).

## Hatalı kullanım

Geçersiz handle, bağlı pipeline olmadan draw/dispatch, sınır dışı kopya, gönderilmiş listeyi
tekrar kullanma: **loglanır ve atlanır**, çökme yok. Gönderilmeden bırakılan liste, kaydettiği
durum değişikliklerini geri alır. Kayıt sürerken bloklayan yükleme (`write_texture`, GPU-only
`write_buffer`/`read_buffer`) reddedilir.

## Test

GPU testleri gerçek Vulkan ile koşar ve **validation hatası testi başarısız yapar**; makinede GPU
yoksa Mesa **lavapipe** (yazılımsal Vulkan) yeter:
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset debug`. Cihaz yoksa testler
atlanır. Offscreen üçgen render edilip piksel okunarak doğrulanır; validation katmanı açık.

## Bilinen sınırlar

- API tek thread'den kullanılmalı (çok thread'li komut kaydı sonraki fazda).
- GPU'da mip zinciri üretimi, cube/3D texture, MSAA, sorgu (timestamp) yok. Sıkıştırılmış formatlardan
  yalnız BC5/BC7 (BC1/BC4/BC6H, ASTC yok).
- Mesh shader sadece `supports()` ile sorgulanabiliyor, kullanılmıyor.
- BLAS'lar statik (refit/compaction yok); skinned mesh'ler için güncellenebilir BLAS sonraki iş.

## Timestamp sorguları (M8)

`Feature::Timestamps`; `create_timestamp_pool(n)`, `CommandList::reset_queries` / `write_timestamp`,
`read_timestamps` (hazır değilse bekletmez). `hostQueryReset` zorunlu özellik. Render graph bunları pass
başına kullanır; oyun kodu doğrudan görmez.
