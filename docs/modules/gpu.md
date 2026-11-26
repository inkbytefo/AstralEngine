# GPU (`Astral::Gpu`)

> **Olgunluk:** Kullanılabilir · **Bağımlılıklar:** Astral::Core (public); Vulkan loader/başlıkları (private), VulkanMemoryAllocator v3.1.0 (header-only, private) ·
> **Doğrulama:** master a113b55 (2026-11-26) kaynak incelemesi

## Amaç

Vulkan 1.3 üzerine kendi küçük RHI'mız (ADR-0003): fiziksel cihaz seçimi, kaynak (buffer, texture, pipeline,
BLAS/TLAS, timestamp havuzu) yaşam döngüsü, komut listesi kaydı, gönderim/senkronizasyon ve swapchain. Public
başlıklarda Vulkan tipi yoktur. Tüm texture'lar tek bir bindless tabloda, tüm buffer'lar buffer device address ile
erişilir; tüm pipeline'lar aynı layout'u paylaşır (ADR-0008). Bu modül bir render graph, malzeme sistemi veya shader
derleyicisi **değildir**; bunlar `render` ve `shader` modüllerindedir. D3D12/Metal gibi ikinci bir backend yoktur
(ADR-0003 "ileride eklenebilir" der).

## Public API

| Tip / fonksiyon | Ne yapar | Kaynak |
|---|---|---|
| `Device::create(DeviceDesc)` | Vulkan 1.3 + gerekli özellikler yoksa `nullptr`; `validation`, `surface`, `logger` | `modules/gpu/include/astral/gpu/device.hpp:42`, `:30` |
| `Device::supports(Feature)` | `BufferDeviceAddress`, `Bindless`, `Validation`, `Presentation`, `RayTracing`, `TextureCompressionBC`, `Timestamps`, `MeshShaders` | `modules/gpu/include/astral/gpu/device.hpp:49`, `modules/gpu/include/astral/gpu/types.hpp:119` |
| `create_buffer`, `create_texture` | `MemoryUsage::{GpuOnly, CpuToGpu, GpuToCpu}`; texture yalnız 2D, tek katman | `modules/gpu/include/astral/gpu/device.hpp:54`, `:55` |
| `write_buffer`, `read_buffer`, `write_texture` | Host-visible buffer'a doğrudan, diğerlerine bloklayan staging kopyası | `modules/gpu/include/astral/gpu/device.hpp:83`, `:87`, `:91` |
| `create_compute_pipeline`, `create_graphics_pipeline` | SPIR-V `ShaderBlob`; grafikte vertex girişi yok (vertex pulling) | `modules/gpu/include/astral/gpu/device.hpp:56`, `:57`; `modules/gpu/include/astral/gpu/types.hpp:189` |
| `create_blas`, `create_tlas`, `acceleration_structure_address` | Ray query için hızlandırma yapıları | `modules/gpu/include/astral/gpu/device.hpp:61`, `:63`, `:65` |
| `create_timestamp_pool`, `read_timestamps` | GPU zaman damgaları; hazır değilse beklemez | `modules/gpu/include/astral/gpu/device.hpp:68`, `:71` |
| `destroy(...)` | Handle hemen ölür, bellek GPU bitirince silinir | `modules/gpu/include/astral/gpu/device.hpp:73-78` |
| `buffer_address`, `bindless_index` | Shader'a verilecek 64-bit adres ve bindless indeks | `modules/gpu/include/astral/gpu/device.hpp:94`, `:96` |
| `begin_commands`, `submit`, `wait`, `is_complete`, `wait_idle` | Komut kaydı ve zaman çizelgesi (timeline semaphore) bileti | `modules/gpu/include/astral/gpu/device.hpp:101-105` |
| `CommandList` | `transition`, `begin_rendering`, `bind_pipeline`, `push_constants`, `draw*`, `dispatch`, `build_tlas`, kopyalar, timestamp, debug etiketi | `modules/gpu/include/astral/gpu/command_list.hpp:45` |
| `Swapchain::create/acquire/present/invalidate` | Pencereye sunum; boyut değişimini kendisi yeniler | `modules/gpu/include/astral/gpu/swapchain.hpp:29`, `:41`, `:44`, `:46` |
| `Format` (14 değer), `ResourceState`, `Sampler` (3 sabit sampler) | Sabit numaralandırmalar | `modules/gpu/include/astral/gpu/types.hpp:48`, `:97`, `:116` |
| `max_push_constant_size` | Her pipeline için 128 bayt push constant | `modules/gpu/include/astral/gpu/types.hpp:232` |

## Nasıl çalışır

**Cihaz seçimi.** `meets_requirements` Vulkan 1.3 ve şu özelliklerin hepsini ister: dynamic rendering,
synchronization2, timeline semaphore, buffer device address, descriptor indexing (runtime array, partially bound,
update-after-bind), scalar block layout, host query reset, `shaderInt64`, `shaderStorageImageWriteWithoutFormat`
(`modules/gpu/src/device_init.cpp:50-65`). Uygun cihazlar arasında ayrık GPU > tümleşik > sanal > CPU puanıyla
seçilir (`modules/gpu/src/device_init.cpp:68-77`); kullanıcıdan cihaz seçme API'si yoktur. Yalnızca bir grafik+compute kuyruk ailesinden tek bir
kuyruk açılır (`modules/gpu/src/device_init.cpp:230-233`); ayrı transfer/async compute kuyruğu yoktur. Ray tracing
(`VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, `VK_KHR_deferred_host_operations`) yalnızca üçü de varsa ve özellik
bildirilmişse açılır (`modules/gpu/src/device_init.cpp:263-281`); yoksa `Feature::RayTracing` yanlıştır ve BLAS/TLAS çağrıları
hata loglayıp boş handle döner (`modules/gpu/src/ray_tracing.cpp:72`).

**Bellek.** Tüm buffer ve image'lar VMA ile ayrılır (`modules/gpu/src/device_init.cpp:292-298`, `modules/gpu/src/vma.cpp:13`). Her
buffer transfer src/dst, storage ve device address bayraklarını alır (`modules/gpu/src/resources.cpp:119-127`); RT
destekliyse AS-build-input bayrağı da eklenir (`modules/gpu/src/resources.cpp:169`). `GpuOnly` buffer'a yazma ve okuma, her çağrıda
yeni bir staging buffer + `Impl::immediate` (tek seferlik komut tamponu, tam bariyer, gönder ve **bekle**) ile
yapılır (`modules/gpu/src/resources.cpp:224-270`, `modules/gpu/src/device.cpp:186-225`). Aynı yol `write_texture` için de geçerlidir
(`modules/gpu/src/resources.cpp:383-437`).

**Bindless.** Tek descriptor set: binding 0 sampled image dizisi, binding 1 storage image dizisi (aynı indeks uzayı),
binding 2 üç sabit sampler (`modules/gpu/src/device_init.cpp:336-339`). Kapasite en çok 16384'tür ve cihazın update-after-bind
limitleriyle sınırlanır (`modules/gpu/src/device_init.cpp:315-326`). Tablo dolarsa `create_texture` loglayıp boş handle döner
(`modules/gpu/src/resources.cpp:350`). Texture yok edilince indeks yalnızca GPU işi bitince geri verilir (`modules/gpu/src/resources.cpp:367-370`).
Sabit sampler'lar `LinearClamp`, `LinearRepeat` (8x anizotropi, varsa) ve `NearestClamp`'tır
(`modules/gpu/src/device_init.cpp:370-385`). Shader tarafı sözleşmesi `modules/gpu/shaders/include/astral/bindless.glsl` içindedir.

**Yaşam süresi.** Bir `HandlePool` per kaynak türü (`modules/gpu/src/vk_device.hpp:125-129`). Tek `timeline semaphore`;
her `submit` değeri bir artırır (`modules/gpu/src/device.cpp:363`, `:399`). `destroy` handle'ı hemen öldürür ve gerçek silmeyi
`Impl::retire` ile kuyruğa alır; kuyruk `begin_commands`, `wait` ve `wait_idle` içinde boşaltılır (`modules/gpu/src/device.cpp:28-36`,
`:109-123`). Şu anda kayıt açık olan her komut slot'u çöp girdisine eklenir; o slot submit/drop edilene kadar kaynak
yaşar (`modules/gpu/src/device.cpp:28-36`, `:86-93`; test `modules/gpu/tests/msr_regression_test.cpp:32`).

**CommandList ve durum takibi (ADR-0040).** Texture durumu **kayıt anında** izlenir: `transition` mevcut durumdan
`VkImageMemoryBarrier2` üretir (`modules/gpu/src/command_list.cpp:90-125`).
Liste submit edilmeden bırakılırsa `abandon` durum değişikliklerini geri alır veya, sonradan başka bir açık liste aynı
texture'a geçmişse, bir sonraki submit'in önüne "fix-up" barrier'ı koyar (`modules/gpu/src/command_list.cpp:50-78`,
`modules/gpu/src/device.cpp:38-84`, `:367-368`). Sözleşme: listeler **kayıt sırasıyla** submit edilmeli
(`modules/gpu/include/astral/gpu/command_list.hpp:39-44`). Hatalı kullanım (geçersiz handle, pipeline'sız draw,
sınır dışı kopya, submit sonrası kullanım) loglanır ve atlanır, çökme olmaz (`modules/gpu/src/command_list.cpp:235-247`,
`modules/gpu/include/astral/gpu/command_list.hpp:42-43`). Bu loglar yalnızca `Logger` aracılığıyla görünür; assert yoktur.

**Pipeline'lar.** Grafik pipeline `VK_KHR_dynamic_rendering` ile (render pass nesnesi yok), viewport ve scissor dinamik,
vertex input boştur, çok-örnekleme 1'dir (`modules/gpu/src/pipelines.cpp:112-153`). Blend yalnızca üç mod: yok, alfa,
toplamalı (`modules/gpu/src/pipelines.cpp:52-68`). Pipeline cache kullanılmaz (`VK_NULL_HANDLE`, `modules/gpu/src/pipelines.cpp:87`, `:157`); derlenmiş
SPIR-V diske `render` modülündeki `SpirvCache` ile önbelleğe alınır (bu modülün dışında).

**Ray tracing.** Yalnızca ray query (RT pipeline/SBT yok). `create_blas` üçgen geometri (3 float konum, uint32 indeks),
`PREFER_FAST_TRACE`, opak, **bloklayan** build'dir (`modules/gpu/src/ray_tracing.cpp:70-127`). `CommandList::build_tlas` her çağrıda TLAS'ı
baştan kurar; örnekleri geçici host-visible buffer'dan alır, öncesi ve sonrasında bellek bariyeri koyar
(`modules/gpu/src/ray_tracing.cpp:177-234`). Geçersiz BLAS'lı örnekler sessizce atlanır (`modules/gpu/src/ray_tracing.cpp:197`).

**Swapchain.** `FIFO` (vsync) ya da `MAILBOX`/`IMMEDIATE` (`modules/gpu/src/swapchain.cpp:67-77`). Format olarak tercihen
`BGRA8Srgb` + sRGB doğrusal olmayan renk uzayı (`modules/gpu/src/swapchain.cpp:52-65`). Boyut değişimi ya da `OUT_OF_DATE`'te
`vkDeviceWaitIdle` ile tam bekleyip yeniden kurar (`modules/gpu/src/swapchain.cpp:93`); pencere boyutu her `acquire`'da kendisi
karşılaştırılır (`modules/gpu/src/swapchain.cpp:188-192`). `acquire` aynı kare slotunun önceki gönderimini bekleyerek CPU'yu sınırlar
(`modules/gpu/src/swapchain.cpp:196-198`).

**Doğrulama.** `DeviceDesc::validation` true ise ve `VK_LAYER_KHRONOS_validation` kuruluysa katman açılır; kurulu değilse
sessizce kapalı kalır, `Feature::Validation` ile sorgulanır (`modules/gpu/src/device_init.cpp:98-99`). Uyarı ve hata iletileri `Logger`'a
gider; `Logger` yoksa yalnızca hatalar `stderr`'e yazılır (`modules/gpu/src/device_init.cpp:16-28`). Testler doğrulama hatalarını testi
başarısız yapacak şekilde sayar (`modules/gpu/tests/gpu_test_support.hpp:14-39`).

## Güçlü yanlar

- Doğrulama katmanıyla koşan, hata sayan gerçek Vulkan testleri; "hata sayısı beklenenle eşit değilse test başarısız"
  (`modules/gpu/tests/gpu_test_support.hpp:35-39`). 35 test, 7 dosyada.
- Kaynak ömrü karmaşık durumlarda sınanmış: aynı anda birden fazla açık liste, drop edilen listeler, başka cihaza ait liste
  (MSR G01-G03, G09: `modules/gpu/tests/msr_regression_test.cpp:11-127`).
- Hatalı kullanım çökertmez: sınır dışı kopya, geçersiz handle, 128 baytı aşan push constant, submit sonrası kullanım
  reddedilir (`modules/gpu/tests/robustness_test.cpp:23-93`).
- Bindless + buffer device address: descriptor yönetimi yok; pipeline layout tektir (`modules/gpu/src/device_init.cpp:397-403`).
- Zaman çizelgeli semaphore ve `SubmitTicket` ile `on_submitted` geri çağrısı: üst katman GPU'nun bir veriyi ne zaman
  bıraktığını sorgulayabilir (`modules/gpu/include/astral/gpu/command_list.hpp:87`; test `modules/gpu/tests/command_test.cpp:108`).
- `read_timestamps` hiç beklemez, hazır değilse `false` döner (`modules/gpu/src/queries.cpp:30-45`).
- Public API Vulkan tipsizdir; `Device::Impl` opak (`modules/gpu/include/astral/gpu/device.hpp:107`).

## Zayıf yanlar ve bilinen sınırlar

- **Tek thread:** `Device` ve `CommandList` thread-safe değildir; çok thread'li kayıt yok (`modules/gpu/include/astral/gpu/device.hpp:38`,
  `modules/gpu/include/astral/gpu/command_list.hpp:44`). Tek kuyruk (`modules/gpu/src/device_init.cpp:230-233`).
- **Yüklemeler bloklar ve kare başına pahalıdır:** her `write_texture` bir staging buffer ayırır, tek seferlik komut
  tamponu gönderir ve GPU'yu bekler (`modules/gpu/src/resources.cpp:383-437`, `modules/gpu/src/device.cpp:186-225`). `read_buffer` ve `write_buffer`
  (GPU-only) de aynı yola girer; `read_buffer` ayrıca bekleyen **tüm** GPU işini bekler (`modules/gpu/src/resources.cpp:252`). Bunlar kayıt
  açıkken reddedilir (`modules/gpu/src/device.cpp:102-107`). Büyük sahnelerin yüklenmesinde sıralı texture yüklemesi bu yüzdendir
  (lead'in başlangıç ölçüm notuyla tutarlı; bu modül ölçülmedi).
- **Kaynak türleri dar:** texture yalnızca 2D, tek katman, 1 örnek (MSAA yok), optimal tiling; cube map, 3D texture, texture
  dizisi, stencil, MSAA resolve yok (`modules/gpu/src/resources.cpp:321-329`, `modules/gpu/include/astral/gpu/types.hpp:48-63`). 14 format; R8/RG8/R16 tek/çift kanallı
  formatlar ve BC1/BC4/BC6H/ASTC yok. Mip üretimi GPU'da yok (mip'ler dışarıdan `write_texture` ile yüklenir).
- **Üç sabit sampler:** karşılaştırma (shadow) sampler'ı ve özel sampler yok (`modules/gpu/include/astral/gpu/types.hpp:116`, `modules/gpu/src/device_init.cpp:370-385`).
- **Barrier ilkel:** `transition` her çağrıda tek bariyer yayınlar (gruplama yok), tüm mip'leri kapsar (alt kaynak takibi
  yok) (`modules/gpu/src/command_list.cpp:90-125`); buffer'lar için yalnızca `memory_barrier()` (her şeyden her şeye, "basit, hızlı değil",
  `modules/gpu/include/astral/gpu/command_list.hpp:55`, `modules/gpu/src/command_list.cpp:127-136`). `immediate` de tam bariyer koyar (`modules/gpu/src/device.cpp:196-207`).
- **Komut eksikleri:** indirect draw/dispatch yok (`BufferUsage::Indirect` var ama kullanan komut yok:
  `modules/gpu/include/astral/gpu/types.hpp:75`, `modules/gpu/src/resources.cpp:125`); texture-texture kopya, blit, clear/fill komutları yok; texture kopyaları yalnızca
  mip 0 (`modules/gpu/src/command_list.cpp:291-327`).
- **Pipeline kısıtları:** stencil, çokgen modu, derinlik bias'ı, özel blend, specialization constant, pipeline cache yok
  (`modules/gpu/src/pipelines.cpp:112-153`). Pipeline oluşturma eşzamanlıdır ve çağıran thread'i bekletir.
- **Mesh shader gerçek değil:** `Feature::MeshShaders` yalnızca `VK_EXT_mesh_shader` eklentisinin cihazda **bulunmasına**
  bakar; eklenti açılmaz ve mesh shader API'si yoktur. `supports(MeshShaders)` true dönse bile kullanılamaz
  (`modules/gpu/src/device.cpp:301-302`, `modules/gpu/src/device_init.cpp:260-281`). Ayrıca her çağrıda eklenti listesini yeniden sorar
  (`modules/gpu/src/device_init.cpp:408-416`).
- **VRAM bütçesi açıkta değil:** bellek kullanımı, VMA istatistikleri ve `VK_EXT_memory_budget` API'de yok (modül içinde
  `vmaGet*Budget`/istatistik çağrısı bulunamadı, grep). Bellek yetersizliğinde yalnızca genel "allocation failed" logu ve
  boş handle döner (`modules/gpu/src/resources.cpp:198`, `:334-335`).
- **Cihaz kaybı yönetimi zayıf:** `Impl::immediate` `vkAllocateCommandBuffers` ve `vkQueueSubmit2` sonucunu denetlemez,
  `last_submitted`'i artırıp o değeri sonsuza dek bekler (`modules/gpu/src/device.cpp:192`, `:210-223`, `:131`); cihaz kaybolursa bu çağrılar
  asılı kalabilir (doğrulanmadı: gerçek bir cihaz kaybıyla denenmedi). `submit_internal` hatada loglar ve ilerlemez
  (`modules/gpu/src/device.cpp:393-398`) ama kurtarma yoktur.
- **Cihaz seçimi sabit:** puanlama dışında cihaz seçme (ortam değişkeni, ayar) yok; çok GPU'lu makinede ayrık GPU
  seçilir (`modules/gpu/src/device_init.cpp:68-77`).
- **BLAS'lar statik, bloklayan:** refit, update, compaction yok; skinned mesh'ler için yeniden kurulum gerekir; BLAS yalnızca
  uint32 indeksli opak üçgen (`modules/gpu/src/ray_tracing.cpp:70-127`). TLAS her çağrıda tamamen yeniden kurulur ve her çağrıda geçici bir
  buffer ayırır (`modules/gpu/src/ray_tracing.cpp:177-234`). Örnekler `TRIANGLE_FACING_CULL_DISABLE` ile sabitlenmiştir (`modules/gpu/src/ray_tracing.cpp:203`).
- **`write_texture` kullanım bayrağı denetlemez:** `TransferDst` olmayan (ör. yalnızca `ColorTarget`) bir texture'a yazma
  Vulkan geçersizliğidir ve burada engellenmez (`modules/gpu/src/resources.cpp:383-390`, `:276-286`).
- **Swapchain:** HDR sunum için özel yol yok (tercih `BGRA8Srgb` + sRGB doğrusal olmayan; bulunamazsa tanınan ilk format, `modules/gpu/src/swapchain.cpp:52-65`). Yeniden boyutlandırma `vkDeviceWaitIdle` ile tüm GPU'yu bekletir
  (`modules/gpu/src/swapchain.cpp:93`). `Swapchain` `Device`'tan önce yok edilmelidir (`modules/gpu/include/astral/gpu/swapchain.hpp:27-28`).
- **Platform kapsamı:** CI'da Lavapipe (yazılım Vulkan) ve Windows'ta yalnızca GPU varsa çalışan testler var; macOS/MoltenVK
  doğrulanmadı.
- Kaynakta `TODO`/`FIXME` işareti yok.

## Testler

`modules/gpu/tests/` (hedef `astral_gpu_tests`, `modules/gpu/CMakeLists.txt:35-42`); `TestDevice` doğrulama katmanını açar ve
Vulkan 1.3 cihazı yoksa **test gövdesini atlar (test geçmiş sayılır)** (`modules/gpu/tests/gpu_test_support.hpp:28-34`).

| Dosya | Kapsam |
|---|---|
| `device_test.cpp` | cihaz oluşturma, doğrulama hatasız |
| `resource_test.cpp` | buffer round-trip (staging/host), sınır dışı yazma, yok edilmiş handle, adres, bindless indeks, geçersiz tanım, mip yükleme, BC bloğu, timestamp |
| `command_test.cpp` | compute buffer doldurma, offscreen üçgen, ertelenmiş silme, tekrarlı kayıt, `on_submitted`, formatsız HDR storage image, derinlik örnekleme |
| `robustness_test.cpp` | silinen kaynağın açık listede yaşaması, pipeline'sız draw/dispatch, kopya sınırları, bırakılan liste, kayıt sırasında staging reddi, push constant sınırı, submit sonrası kullanım |
| `ray_tracing_test.cpp` | TLAS ile ray query isabeti, AS kötü kullanımı |
| `msr_regression_test.cpp` | G01, G02, G03 (iki test), G05, G06, G09 |

**Kapsanmayanlar:** `Swapchain` gpu testlerinde yok (yalnızca `app`/golden testlerinde dolaylı); çok thread; bellek yetersizliği ve
cihaz kaybı yolları; `Feature::MeshShaders`; çoklu mip'li render hedefi; HDR/vsync dışı sunum modları. Donanım GPU'sunda Lavapipe'tan farklı davranış (sürücü) ancak `golden_hw_*`
testleriyle dolaylı sınanır (lead'in notuna göre bu makinede 7 `golden_hw_*` testi başarısız; neden incelenmedi).

## Performans

Ölçülmedi (bu modül için ayrı benchmark yok). Dolaylı veri: `the_architect` başlangıcında pencere + cihaz kurulumu 0.3-0.6 s
(RTX 3060; lead ölçümü, cihaz oluşturma ve pencere birlikte). RT lighting ve diğer geçiş süreleri `render` modülünün konusudur.

## Kararlar

- [ADR-0003: Kendi RHI'mız, ilk backend Vulkan 1.3](../decisions/0003-vulkan-only-own-rhi.md)
- [ADR-0008: Bindless texture'lar, buffer device address, GLSL](../decisions/0008-bindless-bda-glsl.md)
- [ADR-0011: Donanımsal ray query ile hibrit aydınlatma](../decisions/0011-hybrid-ray-traced-lighting.md)
- [ADR-0040: Bırakılan CommandList'lerin layout düzeltmesi](../decisions/0040-dropped-command-list-layout-fixups.md)
  (not: ADR kuyruğu `Device::Impl::layout_fixups` diye anar; kodda alan `recording.fixups`, `modules/gpu/src/vk_device.hpp:110`)
