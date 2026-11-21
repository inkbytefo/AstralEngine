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
| `SubmitTicket` | Gönderim numarası; `wait(ticket)` / `is_complete(ticket)`. |
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

`examples/hello_triangle/main.cpp` — Window + Device + Swapchain + pipeline, ~60 satır.

## Shader'lar

GLSL yazılır, CMake'te `astral_add_shaders(<target> SOURCES a.vert b.frag)` ile derlenip
programa gömülür; `#include <shaders/a.vert.hpp>` → `astral::shaders::a_vert`. Her shader
`#include <astral/bindless.glsl>` ile ortak tabloyu ve `ASTRAL_BUFFER(Ad, Tip)` makrosunu alır.
Push constant sınırı 128 byte.

## Senkronizasyon ve yaşam süresi

- Tek **timeline semaphore**; her `submit` değeri bir artırır.
- `destroy(handle)` handle'ı hemen öldürür, belleği GPU o ana kadarki işleri bitirince siler
  (gecikmeli silme). Bindless indeks de o zaman yeniden kullanılabilir olur.
- Texture durumları **kayıt anında** izlenir: `transition(tex, state)` önceki durumdan bariyer
  üretir. Bu yüzden komut listeleri kaydedildikleri sırayla gönderilmeli. (M3'te render graph
  bariyerleri otomatik yönetecek.)
- `write_buffer` / `read_buffer` kolaylık içindir: staging gerekiyorsa bekler (her karede değil,
  yükleme sırasında kullan).

## Test

GPU testleri gerçek Vulkan ile koşar; makinede GPU yoksa Mesa **lavapipe** (yazılımsal Vulkan) yeter:
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ctest --preset debug`. Cihaz yoksa testler
atlanır. Offscreen üçgen render edilip piksel okunarak doğrulanır; validation katmanı açık.

## Bilinen sınırlar

- API tek thread'den kullanılmalı (çok thread'li komut kaydı sonraki fazda).
- Storage image'lar shader'da şimdilik `rgba8` olarak tanımlı.
- Mip zinciri üretimi, cube/3D texture, MSAA, sorgu (timestamp) yok.
- Ray tracing / mesh shader sadece `supports()` ile sorgulanabiliyor, kullanılmıyor.
- `acquire()` sonrası `submit()` yapılmazsa bekleyen semaphore'lar sonraki submit'e kalır.
