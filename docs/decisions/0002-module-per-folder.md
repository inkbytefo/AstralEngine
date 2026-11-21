# ADR-0002: Modül başına klasör

**Durum:** Kabul · 2026-11-21

**Karar:** `modules/<ad>/{include/astral/<ad>, src, tests}`. Public API yalnızca
`include/` altında; `src/` private.

**Neden:** Modül tek klasörle taşınır/silinir; `#include <astral/physics/...>` bağımlılığı
görünür kılar ve CMake linklenmeyen modülün header'ını göstermez → katman kuralı derleyici
tarafından korunur. Alternatif (`include/` + `src/<modül>` ayrı ağaçlar) her modülü iki
yere dağıtıyordu.
