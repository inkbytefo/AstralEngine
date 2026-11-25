# AstralEngine M1 — Platform ve GPU Uygulama Planı

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox syntax.

**Goal:** Pencere/input (`Astral::Platform`, SDL3) ve modern Vulkan RHI (`Astral::Gpu`); headless çalışan GPU testleri; `hello_triangle`.

**Architecture:** `Gpu` ve `Platform` aynı katmanda (1) ve birbirini bilmez. Pencere yüzeyi `gpu::SurfaceSource`
(yalnız std tipleri) ile bağlanır. Vulkan/VMA/SDL tipleri sadece `src/` içinde. Kaynaklar `Handle<T>` ile,
shader'lar bindless (tek descriptor heap + push constant'ta indeksler). Sync: timeline semaphore.

**Tech Stack:** Vulkan 1.3 (dynamic rendering, sync2, descriptor indexing, timeline), VMA 3.1, SDL3 3.2, GLSL → glslc (SPIR-V gömülü).

**Spec:** AstralEngine PRD + docs/architecture.md, ADR-0003.

## Global Constraints
- Public header'larda Vulkan/VMA/SDL tipi yok (CI'da grep kontrolü).
- GPU testleri lavapipe ile headless koşar; GPU yoksa testler atlanır (fail değil).
- Bağımlılıklar FetchContent ile, sabit tag.

## Review Focus
1. Kaynak yıkımı GPU hâlâ kullanırken: gecikmeli silme (frame/timeline sonrası).
2. Swapchain out-of-date/resize: crash yok, yeniden oluşturulur.
3. Geçersiz/silinmiş handle ile komut: assert/log, UB yok.
4. Vulkan cihazı olmayan makine: anlamlı hata, `Device::create` boş döner.
5. Pencere küçültülünce (0×0): acquire atlanır.

## Tasks
1. **M0 borcu:** Threads linki + `find_dependency`, sürüm `configure_file`, `CMAKE_INSTALL_INCLUDEDIR`, CI `|| true`, Logger atomic level. Test: version == PROJECT_VERSION.
2. **core/math:** `Vec2/3/4`, `Mat4` (column-major), `Quat`; dot/cross/normalize/length, `perspective` (Vulkan clip, Y aşağı, depth 0..1), `look_at`, translate/rotate/scale, `inverse`, çarpım. Testler bilinen değerlerle.
3. **gpu: cihaz:** `gpu::Device::create(DeviceDesc) -> std::unique_ptr<Device>`; headless seçim, validation opsiyonel, `supports(Feature)`, `adapter_name()`. Test: lavapipe'ta oluşturulur.
4. **gpu: kaynaklar:** `create_buffer/texture`, `destroy`, `write_buffer`, `read_buffer`, bindless indeksler, gecikmeli silme. Test: yaz→oku round-trip; silinen handle reddedilir.
5. **gpu: pipeline + komutlar:** `create_compute_pipeline`, `create_graphics_pipeline`, `CommandList` (barrier, rendering, draw, dispatch, copy), `submit -> SubmitTicket`, `wait`. Testler: compute bir buffer'ı doldurur; offscreen üçgen render → readback merkez piksel rengi; dışı clear rengi.
6. **shader derleme:** `astral_add_shaders(<target> ...)` → glslc → gömülü SPIR-V (`gpu::ShaderBlob`), testlerde kullanılır.
7. **platform:** `Window` (oluştur, poll, size, kapanma, `vulkan_instance_extensions`, `create_vulkan_surface`), `Input` (tuş/mouse durumu, pressed/released kenarları). Test: dummy video sürücüsüyle.
8. **swapchain + hello_triangle:** `gpu::Swapchain` (acquire/present/resize, 2 frame in flight), örnek.
9. **Dokümanlar + push:** roadmap, modules/gpu.md, modules/platform.md, ADR'ler, CHANGELOG.
