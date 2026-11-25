# Astral Engine — Renderer Refactoring Log

**Branch:** `render-audit-refactor`
**Scope:** Verified correctness fixes, shared shader architecture, GPU performance optimizations, C++ layout/pass updates, benchmark harness, and regression tests.

---

## 1. New Shared Shader Modules

| File | Purpose |
|---|---|
| `modules/render/shaders/include/astral/color.glsl` | Unified Rec.709 luminance, exact IEC 61966-2-1 sRGB encode/decode, and Narkowicz ACES tonemapping. |
| `modules/render/shaders/include/astral/brdf.glsl` | Unified GGX/height-correlated Smith microfacet BRDF, Heitz 2018 VNDF sampling, exact `smith_vndf_weight` ($G_2/G_1$), `vndf_reflection_pdf`, Karis `env_brdf`, `eval_direct_pbr`, and `sky_radiance`. |

Both headers were registered in `modules/render/CMakeLists.txt` (`ASTRAL_EMBEDDED_SHADER_INCLUDES`) and wired into `modules/render/src/surface_shader.cpp` so runtime-compiled `.ashader` pipelines resolve `#include <astral/color.glsl>` and `#include <astral/brdf.glsl>` identically to offline-compiled shaders.

---

## 2. Implemented Changes by Subsystem

### 2.1 Ray Tracing & Geometry (`rt.glsl`, `scene.glsl`, `sdf.glsl`, `gbuffer.*`, `sdf.comp`, `ray_tracing.cpp`, `instances.cpp`, `shader_store.cpp`)

1. **Secondary-Hit Precision & Geometric Normal (`A1`):**
   - *Root Cause:* `triangle_surface` computed `s.p = o + d*t`, dropped `ng`, and used camera-distance-scaled `ray_bias` for secondary rays.
   - *Fix:* Reconstructed `s.p` via exact barycentric vertex interpolation, preserved geometric normal `s.ng` (oriented to ray hemisphere), added Wächter & Binder (2019) integer-ULP `offset_ray(s.p, s.ng)` for secondary hits, and isolated depth-buffer bias to `primary_ray_origin(f, p, n)`.
2. **SDF Backface & Degenerate Normal Guard (`A2`):**
   - *Root Cause:* `sdf_surface` hardcoded `s.backface = false`; `sdf_normal` divided by `length(grad)` without checking zero length; `OP_UNION` / `OP_INTERSECT` had precedence/material edge cases.
   - *Fix:* Detected interior SDF hits (`s.backface = dot(n, d) > 0.0`, flipped `n`/`ng`), guarded `sdf_normal` against `len2 <= 1e-12`, parenthesized `OP_UNION`, and fixed `OP_INTERSECT` material selection.
3. **Alpha-Tested RT Candidate Intersections (`A3`):**
   - *Root Cause:* All TLAS instances forced opaque and ray queries used `gl_RayFlagsOpaqueEXT`.
   - *Fix:* Added `TlasInstance::opaque` in `modules/gpu/include/astral/gpu/types.hpp` & `modules/gpu/src/ray_tracing.cpp`, propagated `GpuShader::alpha_test` in `instances.cpp` & `shader_store.cpp`, and compiled `ASTRAL_RT_ALPHA_TEST` candidate confirmation loops into RT variants only when an active custom shader uses `alpha_test`.
4. **Behind-Camera Velocity & Shadow `tmax` Guards (`A4`):**
   - *Root Cause:* `prev.w <= 0` divided by `1e-6`, overflowing `RG16Float` velocity to `±Inf`; point/rect shadow rays could trace with negative `tmax`.
   - *Fix:* Added `compute_velocity(vec4, vec4)` in `scene.glsl` (used by `gbuffer.frag`, `sdf.comp`, `surface_shader.cpp`) and `tmax > 1e-4` / emitter-hemisphere checks in `rt.glsl`.
5. **Fast Normal Matrix (`A5`):**
   - *Fix:* Replaced `transpose(inverse(mat3(m)))` with signed-cofactor `normal_matrix_of(mat4)` in `scene.glsl` (`gbuffer.vert`, `surface_shader.cpp`).

### 2.2 PBR / BSDF Consistency (`brdf.glsl`, `lighting.comp`, `rt.glsl`, `rt_lighting.comp`, `surface_shader.cpp`)

1. **Unified Height-Correlated Smith-GGX (`B1`):**
   - *Fix:* Replaced Schlick-GGX hotness-remapped $k = (r+1)^2/8$ with height-correlated `smith_v_correlated` across `lighting.comp`, `rt.glsl`, and `surface_shader.cpp`.
2. **Unbiased VNDF Reflection Demodulation (`B2`):**
   - *Root Cause:* `rt_lighting.comp` weighted VNDF samples by `smith_g1(nl, a)` without `F(vh)`, while `composite.comp` multiplied by `env_brdf(f0, r, nv)` — double-attenuating rough reflections and omitting per-ray Fresnel.
   - *Fix:* Changed `rt_lighting.comp` reflection weight to `F(vh) * smith_vndf_weight(nv, nl, a) / max(dfg, 1e-4)` (cached `dfg` once per pixel), so `composite.comp`'s `dfg * spec` recovers the exact unbiased microfacet integral.
3. **RectLight Specular MIS (`D2`):**
   - *Fix:* Added power-heuristic MIS between area-light sampling in `direct_light` (reusing the existing shadow ray, zero extra rays) and VNDF reflection rays hitting a `RectLight` in `rt_lighting.comp`.
4. **Forward Transparent PBR (`B4`):**
   - *Fix:* Replaced Blinn-Phong in `surface_shader.cpp` (`astral_lit`) with shared `eval_direct_pbr` + `env_brdf`, restoring energy/unit parity with opaque surfaces.

### 2.3 DDGI Correctness & Performance (`ddgi_trace.comp`, `ddgi_update.comp`, `ddgi_pass.cpp`, `rt.glsl`)

1. **Frame-0 / Post-Reset Uninitialized Atlas Fix (`C4`):**
   - *Root Cause:* `ddgi_trace.comp` sampled uninitialized `irradiance_[prev]` / `distance_[prev]` whenever `f.rt_counts.w == 0u`.
   - *Fix:* Passed `use_ddgi = (f.rt_counts.w != 0u)` to `shade_secondary` and sanitized non-finite radiance outputs.
2. **Normalized `[0,1]` Distance Moments in `RG16Float` (`C1`):**
   - *Root Cause:* Raw `d*d` overflowed FP16 above `256 m`.
   - *Fix:* Normalized ray distances by `ddgi_max_dist(f)` in `ddgi_update.comp` and `ddgi_irradiance` (`rt.glsl`); changed distance atlas format in `ddgi_pass.cpp` from `RGBA16Float` to `RG16Float`.
3. **Signed Backface Encoding & Embedded-Probe Rejection (`C2`, `D3`):**
   - *Fix:* Encoded backface hits as `-h.t * 0.2` in `ddgi_trace.comp`; skipped backface rays in irradiance, zeroed probes with $>25\%$ backfaces, and guarded `c > 0.0` before `pow(c, 50.0)` in `ddgi_update.comp`.
4. **Shared-Memory 1-Workgroup-Per-Probe Update (`G1`):**
   - *Fix:* Rewrote `ddgi_update.comp` to dispatch `(probe_count, 1, 1)` with `local_size = (8, 8, 1)`, caching ray directions/radiance/distances in shared memory and copying 1-texel octahedral borders in the same dispatch. Reduced `ddgi.update.distance` by **6.7×** and `ddgi.update.irradiance` by **2.7×**.

### 2.4 Direct Lighting & RT Performance (`rt.glsl`, `rt_lighting.comp`)

1. **Deferred DDGI Lookup (`G2`):**
   - *Fix:* Moved `ddgi_irradiance` inside the miss / beyond-`near_gi_radius` branch in `rt_lighting.comp` and eliminated the duplicate `env_brdf` call.
2. **Single-Frame RIS for High Light Counts (`G3`):**
   - *Fix:* Added unbiased Resampled Importance Sampling (2 point-light samples when `point_count > 4`; 1 rect-light sample when `rect_count > 2`, using unshadowed diffuse+specular target PDF) in `rt.glsl`. Reduced `high_lights` GPU time by **3.07×** (45.58 → 14.83 ms).

### 2.5 Temporal Denoising, À-Trous & TAA (`denoise_temporal.comp`, `denoise_atrous.comp`, `taa.comp`, `ray_traced_path.*`, `raster_path.cpp`, `gpu_layouts.hpp`)

1. **Previous-Camera View-Distance Comparison (`E1`):**
   - *Root Cause:* `denoise_temporal.comp` compared `pg.w` (distance from previous camera) against `geo.w` (distance from current camera), rejecting temporal history on camera translation.
   - *Fix:* Tracked `prev_camera_position` in `RayTracedPath` / `RasterPath`, uploaded it in `GpuFrame` (544 B), and compared `pg.w` against `length(f.prev_camera_position.xyz - world)`. Reduced `moving_camera` high-frequency noise by **47.2%**.
2. **World-Space Plane-Distance Edge Stopping (`E4`):**
   - *Fix:* Replaced raw `abs(qg.w - geo.w)` with `abs(dot(qp - center_p, geo.xyz))` in `denoise_atrous.comp`.
3. **Unjittered Sky Reprojection (`E5`):**
   - *Fix:* Switched `taa.comp` sky reprojection to `unjittered_camera_ray(f, uv)` from `primary.glsl`.

### 2.6 Bloom, Composite, Tonemap & UI (`bloom_up.comp`, `bloom_pass.cpp`, `composite.comp`, `tonemap.frag`, `ui.frag`)

1. **Energy-Conserving Bloom Upsample (`F1`):**
   - *Root Cause:* Unnormalized `here + up` across 5 upsample levels created 6× DC energy.
   - *Fix:* Passed `blend = float(step) / float(step + 1)` via `BloomUpPush` (`bloom_pass.cpp`) and blended `mix(here, up, pc.blend)` in `bloom_up.comp` (exact unit DC gain).
2. **Removed Debug Magenta Override & Protected Debug Views (`F2`):**
   - *Fix:* Removed leftover `vec3(50, 0, 50) // DEBUG` line from `composite.comp` and gated visible `RectLight` overlay on `debug_view == 0u`.
3. **Exact IEC 61966-2-1 sRGB Everywhere (`F3`):**
   - *Fix:* Switched `tonemap.frag`, `ui.frag`, and `surface_shader.cpp` to shared `astral_linear_to_srgb` / `astral_srgb_to_linear` in `color.glsl`.

---

## 3. Benchmarks & Regression Tests Added

- `bench/render_bench.cpp` (`astral-render-bench`, wired in `bench/CMakeLists.txt`): Deterministic 6-scene GPU benchmark (`cornell_area`, `material_grid`, `thin_wall`, `high_lights`, `moving_camera`, `open_world_scale`) reporting per-pass Vulkan timestamp averages and exporting PPM frames.
- `modules/render/tests/render_audit_test.cpp` (wired in `modules/render/CMakeLists.txt`): 5 CPU mathematical verification tests (IEC sRGB roundtrip/continuity, normal-matrix cofactor under non-uniform & negative scale, GGX VNDF white-furnace energy conservation, normalized DDGI Chebyshev bounds, bloom 6-level unit DC gain) + 6 headless Vulkan GPU regression tests (Frame-0 DDGI explosion prevention, `moving_camera` temporal convergence, large-scale 2000 m DDGI stability, `high_lights` RIS finite output, transparent forward PBR luminance parity, alpha-tested RT shadow pass-through).

---

## 4. Migration Notes & Follow-Up Milestone Completed

- **Golden Image Baselines (ADR-0031):** Fixing Bloom's 6× DC amplification (`F1`), exact IEC sRGB encoding (`F3`), and `render_mode unshaded` (`hello_triangle`) legitimately shifts affected scenes relative to pre-refactor references. Golden reference PNGs in `tests/golden/references/` must be regenerated on Linux Mesa lavapipe (`ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$'`) — never overwritten with hardware GPU captures.

### Milestone 2 — Deferred Audit Features Implemented & Validated

1. **End-to-End `render_mode unshaded` (`surface_shader.cpp`, `primary.glsl`, `rt.glsl`, `lighting.comp`, `rt_lighting.comp`, `composite.comp`):**
   - Opaque G-buffer encodes `emissive = vec4(ALBEDO + EMISSION, 2.0)` (preserving `albedo`, `normal`, and `velocity` for debug views & TAA, zero extra G-buffer textures); `lighting.comp` and `composite.comp` bypass PBR lighting directly; `rt_lighting.comp` and ReSTIR passes skip ray tracing and output `geometry.w = -1` so denoiser neighbors do not bleed black.
   - Secondary RT hits (`rt_chunk` + `shade_secondary`) return `s.albedo + s.emissive` directly for mirrors and GI.
   - Forward transparent (`blend_mix` / `blend_add` + `unshaded`) skips `astral_lit` and tonemaps `ALBEDO + EMISSION` directly.
2. **Sky–SDF Depth Coherence (`sky_pass.{hpp,cpp}`, `surface_shader.cpp`, `raster_path.cpp`, `ray_traced_path.cpp`):**
   - `SkyPass` passes `gbuffer.sdf_depth` via unused `ScreenPush::scene` (`no_texture` when `sdf_count == 0`); sky fragment shaders discard early whenever `f.counts.y > 0u && sd < 1.0`. Zero extra render passes and zero overhead when `sdf_count == 0`.
3. **Virtual Reflection-Hit Specular Reprojection (`rt_lighting.comp`, `denoise_temporal.comp`, `denoise_atrous.comp`, `gpu_layouts.hpp`, `scene.glsl`):**
   - `rt_lighting.comp` packs reflection hit distance `spec_hit_t` into unused `rt.specular.a`.
   - Added `inv_prev_view_projection` to `GpuFrame`/`Frame` (608 B).
   - `denoise_temporal.comp` computes virtual reflection world point along `unjittered_camera_ray`, blends surface and virtual-hit motion smoothly by roughness (fast-path merged with diffuse taps on rough surfaces), validates specular taps with world-space plane distance, tracks independent specular history in `history.specular.a`, and applies roughness-scaled 3×3 neighborhood variance clamping.
4. **Spatiotemporal ReSTIR DI (`astral/restir.glsl`, `restir_di_temporal.comp`, `restir_di_spatial.comp`, `rt_lighting_pass.{hpp,cpp}`, `shader_store.{hpp,cpp}`, `ray_traced_path.{hpp,cpp}`, `renderer.{hpp,cpp}`):**
   - Added `RenderSettings::restir_di` (default `true`, reflected), activating automatically when `point_lights + rect_lights > 2` (zero overhead for 0–2 local lights).
   - 16-byte GPU `Reservoir` (`GpuReservoir`) ping-ponged via `reservoirs_[2]` (pre-spatial temporal history, eliminating spatial-temporal correlation lock-in) + `reservoir_temp_` (final shaded spatial output).
   - Stratified $M_{\text{init}} = 24$ initial candidates + 1 initial visibility ray, temporal reuse ($M_{\max} = 20$, auto-invalidated on reset/resize/light-count change), and unbiased spatial reuse (3 disk neighbors, Algorithm 6 exact visibility-aware MIS $Z(y^*)$), preserving exact RectLight MIS with VNDF reflection rays and alpha-test RT variant support.
