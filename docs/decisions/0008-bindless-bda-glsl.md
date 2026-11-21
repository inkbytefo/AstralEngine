# ADR-0008: Bindless texture'lar, buffer device address, GLSL

**Durum:** Kabul · 2026-11-21

**Karar:**
- Tek global descriptor set: tüm texture'lar ve sampler'lar bir tabloda; shader'a indeks verilir.
- Buffer'lara descriptor yok: shader'lar 64-bit GPU adresiyle (buffer device address) erişir.
- Tüm pipeline'lar aynı layout'u paylaşır: set 0 + 128 byte push constant.
- Vertex input yok: vertex shader veriyi buffer'dan kendisi çeker (vertex pulling).
- Shader dili GLSL (glslc → SPIR-V, programa gömülü). Eski v2 shader'ları doğrudan taşınabilir.

**Neden:** Descriptor yönetimi ve pipeline layout karmaşası ortadan kalkar; GPU-driven çizime
(indirect, culling) doğal geçiş; modern motorların yaklaşımı.

**Sonuçlar:** Vulkan 1.3 + descriptor indexing + BDA zorunlu (2018 sonrası tüm masaüstü GPU'lar).
Slang'e geçiş ileride ayrı bir karar olabilir.
