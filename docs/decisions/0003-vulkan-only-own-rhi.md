# ADR-0003: Kendi RHI'mız, ilk backend Vulkan 1.3

**Durum:** Kabul · 2026-11-21

**Karar:** bgfx gibi hazır bir soyutlama yerine kendi RHI'mız (`Astral::Gpu`). İlk ve şimdilik
tek backend Vulkan 1.3 (dynamic rendering, descriptor indexing/bindless, timeline semaphore).
RHI modern explicit API'lere göre tasarlanır; D3D12/Metal ileride eklenebilir.

**Neden:** Üst düzey görüntü kalitesi ve render graph, bindless, GPU-driven çizim,
ray tracing/mesh shader gibi yeni özelliklere tam kontrol. SDF Playground'daki Vulkan
deneyimi doğrudan taşınır.

**Sonuçlar:** Desteklenmeyen özellikler `supports(Feature)` ile sorgulanır (Liskov).
Vulkan tipleri public header'lara sızmaz.
