# Astral Engine — Rendering Architecture & Contracts

This document specifies the modernized Vulkan 1.3 rendering architecture of Astral Engine after the `render-audit-refactor` overhaul, including its modular shader library, shared mathematical/color/coordinate contracts, and CPU↔GPU layout synchronization.

---

## 1. High-Level Architecture

Astral's renderer (`modules/render`) sits at Layer 2 on top of `modules/gpu` (Vulkan 1.3 + `VK_KHR_buffer_device_address` + descriptor indexing + `VK_KHR_ray_query` + `scalarBlockLayout`) and `modules/shader` (`.ashader` parser/compiler). It exposes a data-oriented `RenderScene` API (`Renderer::render`) decoupled from ECS and runs one of two automatically selected execution paths:

1. **Ray-Traced Hybrid Path** (`modules/render/src/paths/ray_traced_path.cpp`):
   `TLAS build` → `gbuffer` (Halton-jittered) → `sdf` *(if SDF shapes exist)* → `ddgi.trace` → `ddgi.update` (irradiance + distance) → `restir.temporal` → `restir.spatial` *(when `restir_di` enabled and `point_lights + rect_lights > 2`)* → `rt.lighting` → `denoise.temporal` → `denoise.atrous` (×N) → `composite` → `sky` *(optional, respects mesh + SDF depth)* → `taa` → `bloom.down` (×6) → `bloom.up` (×5) → `tonemap` → `transparent` (forward) → `post` *(optional)* → `debug.lines` → `ui`.
2. **Raster Fallback Path** (`modules/render/src/paths/raster_path.cpp`):
   `shadow` (2048² orthographic PCF) → `gbuffer` → `sdf` *(if SDF shapes exist)* → `lighting` → `sky` *(optional, respects mesh + SDF depth)* → `tonemap` → `transparent` → `post` *(optional)* → `debug.lines` → `ui`.

All transient textures, layout transitions, and synchronization barriers are managed declaratively by `RenderGraph` (`modules/render/src/render_graph.cpp`).

---

## 2. Shared Shader Module Architecture

All duplicated math, color, and BSDF formulas have been consolidated into five layered GLSL include modules under `modules/render/shaders/include/astral/` (and embedded into C++ string tables via `modules/render/CMakeLists.txt` so runtime `.ashader` compilation uses the exact same headers):

| Header | Responsibilities |
|---|---|
| `astral/bindless.glsl` *(in `modules/gpu`)* | Global descriptor set `0` arrays (`astral_textures[]`, `astral_images[]`, `astral_uimages[]`, `astral_samplers[]`, `astral_tlases[]`) and canonical sampler indices (`0=linear`, `1=nearest`, `2=shadow`). |
| `astral/color.glsl` | Rec.709 luminance (`astral_luminance`), exact piecewise IEC 61966-2-1 sRGB encode/decode (`astral_linear_to_srgb`, `astral_srgb_to_linear`), and Narkowicz ACES filmic curve (`astral_aces_tonemap`). |
| `astral/brdf.glsl` | Unified GGX/Smith microfacet contract: `ggx_d`, `smith_g1`, `smith_v_correlated`, `smith_vndf_weight`, `vndf_reflection_pdf`, Heitz 2018 `sample_vndf`, `fresnel_schlick`, Karis `env_brdf`, `specular_brdf_radiance`, `specular_brdf`, `eval_direct_pbr`, and `sky_radiance`. |
| `astral/scene.glsl` | Scalar-layout BDA structs synchronized with `modules/render/src/gpu_layouts.hpp` (`Frame`, `Instance`, `Material`, `PointLight`, `RectLight`, `SdfShape`, `Vertex`, `GpuShader`), `normal_matrix_of(mat4)` (signed cofactor), `compute_velocity(vec4, vec4)`, `reconstruct_world`, and `reconstruct_prev_world`. |
| `astral/sdf.glsl` | Analytical SDF primitives, CSG operators, guarded tetrahedral normal estimation (`sdf_normal`), sphere tracing (`sdf_march`), and soft shadow marching (`sdf_shadow`). |
| `astral/primary.glsl` | Primary-hit merger (`load_primary` combining mesh + SDF G-buffers, decoding `unshaded` flag in `emissive.a > 1.5`), depth world reconstruction, jittered `camera_ray` and unjittered `unjittered_camera_ray`. |
| `astral/rt.glsl` | Combined TLAS+SDF ray tracing (`trace`, `occluded`, optional `ASTRAL_RT_ALPHA_TEST`), Wächter & Binder (2019) `offset_ray`, `primary_ray_origin`, barycentric triangle reconstruction with `ng`, DDGI normalized-moment sampling, direct lighting (sun, point/rect with RIS + RectLight MIS), and `shade_secondary` (bypassing lighting when `s.unshaded`). |
| `astral/restir.glsl` | Compact 16-byte GPU `Reservoir` (`light_id`, `uv_packed`, `W`, `M`), stratified point + rect light candidate evaluation (`restir_eval_sample` preserving RectLight MIS parity with VNDF), target PDF, and visibility helpers shared by `restir_di_temporal.comp`, `restir_di_spatial.comp`, and `rt_lighting.comp`. |

---

## 3. Shared Rendering Contracts

### 3.1 Coordinate System, Matrices & Depth
- **World space:** Right-handed, $+Y$ up, meters.
- **Clip / Depth:** Standard Vulkan `[0, 1]` depth (`0.0` at near plane, `1.0` at far plane; `D32Float` for meshes and `R32Float` for SDFs; `depth >= 1.0` denotes sky/miss, smaller depth is nearer). Shadow map uses standard orthographic `[0, 1]` depth.
- **Jittered vs. Unjittered Matrices (`Frame`):**
  - `view_projection` / `inv_view_projection`: current frame with Halton(2, 3) sub-pixel jitter (0 jitter on raster path).
  - `unjittered_view_projection`: current frame without jitter (used for motion vectors, object velocity delta, and debug lines).
  - `prev_view_projection` / `inv_prev_view_projection`: previous frame **unjittered** view-projection and its inverse (used for motion reprojection and world-space plane validation of virtual reflection-hit taps).
  - `camera_position` / `prev_camera_position`: current and previous frame world-space camera origins (used by `denoise_temporal.comp` and `restir_di_temporal.comp` to validate previous-frame view distances).

### 3.2 Normal & Motion-Vector Conventions
- **Normals:** World-space unit vectors stored in `RGBA16Float` (`.xyz = shading normal`, `.w = metallic`). Mesh vertex normals transform via `normal_matrix_of(mat4)` (exact signed cofactor matrix — handles non-uniform and negative/mirrored scales without `inverse()`). Triangle RT hits keep both shading normal `s.n` and geometric normal `s.ng` (oriented to the ray hemisphere; `s.backface` records original side).
- **Motion vectors (`velocity`, `RG16Float`):** Computed via `compute_velocity(curr_unjittered_clip, prev_unjittered_clip)` in UV-delta space (`curr_uv - prev_uv`), returning `vec2(0.0)` whenever `curr.w <= 1e-5` or `prev.w <= 1e-5` to prevent `RG16Float` overflow to `±Inf` when geometry crosses behind the camera plane.

### 3.3 BSDF, Roughness & Radiometric Units
- **Roughness parameterization:** Perceptual roughness $r \in [0.02, 1.0]$; microfacet slope $\alpha = r^2$ everywhere (direct lighting, VNDF sampling, `env_brdf`, and forward transparent surfaces).
- **Specular microfacet model:** Isotropic GGX NDF (`ggx_d`) paired with exact height-correlated Smith visibility (`smith_v_correlated`). VNDF sampling (`sample_vndf`, Heitz 2018) uses exact weight `F(vh) * smith_vndf_weight(nv, nl, a)` where `smith_vndf_weight` is $G_2(\omega_o, \omega_i) / G_1(\omega_o)$.
- **Diffuse / Specular energy conservation:** $k_d = (1 - \text{metallic})(1 - F(\omega_o \cdot h))$, diffuse BRDF $= \text{albedo} / \pi$.
- **Demodulation contract (RT path):**
  - `diffuse` buffer stores **irradiance-like demodulated diffuse radiance** (divided by `albedo * (1 - metallic)`), re-multiplied in `composite.comp`.
  - `specular` buffer stores **DFG-demodulated specular radiance**: each VNDF sample contributes `L_i * F(vh) * (G_2 / G_1) / max(dfg, 1e-4)` where `dfg = env_brdf(f0, r, nv)`, so `composite.comp`'s `dfg * specular` restores the exact unbiased estimator without double-attenuating $G_1$ or losing Fresnel.
- **Radiometric units:**
  - `DirectionalLight.intensity` and `PointLight.intensity` are $\pi$-premultiplied (consistent across `lighting.comp`, `rt.glsl`, and transparent forward `surface_shader.cpp`).
  - `RectLight.intensity`, material `emissive`, and `Environment` sky/ground colors are physical radiance ($L$).

### 3.4 Ray Origin Offsetting & Transparency
- **Primary G-buffer hits:** `primary_ray_origin(f, p, n)` applies depth-precision bias scaled by camera distance (`2e-4 * cam_dist`) along the surface normal because `p` is reconstructed from quantized `D32Float` depth.
- **Secondary RT hits:** `offset_ray(s.p, s.ng)` applies Wächter & Binder (2019) integer-ULP offsetting along the **geometric normal `s.ng`** from exact barycentric hit position `s.p` (scale-invariant precision, zero camera-distance shadow detachment).
- **Alpha-tested geometry:** Instances referencing an `alpha_test` `.ashader` are flagged `FORCE_NO_OPAQUE` in TLAS (`TlasInstance::opaque = false`). When any active custom shader uses `alpha_test`, `ShaderStore::compile_rt_variants` compiles `rt_lighting`, `ddgi_trace`, `restir_di_temporal`, and `restir_di_spatial` with `#define ASTRAL_RT_ALPHA_TEST 1`, evaluating the custom `surface()` alpha test inside the `rayQueryProceedEXT` loop.
- **Unshaded surfaces (`render_mode unshaded`):** Output `ALBEDO + EMISSION` in linear HDR with zero direct lighting, shadows, ambient, DDGI, or specular reflections:
  - *Opaque G-buffer:* Encodes `emissive = vec4(ALBEDO + EMISSION, 2.0)` (leaving `emissive.a = 1.0` for lit surfaces and `0.0` for sky) while preserving `albedo`, `normal`, and `velocity` for debug views and TAA. `lighting.comp` and `composite.comp` output `emissive.rgb` directly; `rt_lighting.comp` and ReSTIR passes skip ray tracing and mark `geometry.w = -1` so adjacent shaded pixels do not bleed black across denoiser filters.
  - *Secondary RT hits:* Custom `rt_chunk` sets `s.unshaded = true`; `shade_secondary` returns `s.albedo + s.emissive` directly.
  - *Forward blended (`blend_mix` / `blend_add` + `unshaded`):* Skips `astral_lit` and tonemaps `ALBEDO + EMISSION` directly.
- **Sky–SDF depth coherence:** `SkyPass` binds mesh `gbuffer.depth` for hardware depth testing (`LessEqual` against `z = 1.0`) and passes `gbuffer.sdf_depth` via `ScreenPush::scene` (`no_texture` when `sdf_count == 0`). Sky fragments discard early whenever `f.counts.y > 0u && sd < 1.0`, achieving zero extra passes and zero overhead when no SDFs exist.

### 3.5 Spatiotemporal ReSTIR DI & Virtual Reflection-Hit Reprojection
- **Spatiotemporal ReSTIR DI (`restir_di_temporal.comp`, `restir_di_spatial.comp`, `astral/restir.glsl`):** Activates automatically when `RenderSettings::restir_di == true` and `point_lights + rect_lights > 2` (sun remains deterministic in `rt_lighting.comp`; 0–2 local lights evaluate directly with zero ReSTIR overhead). Uses 16-byte GPU `Reservoir` buffers (`reservoirs_[2]` for pre-spatial temporal history + `reservoir_temp_` for final shaded spatial output, avoiding spatial-temporal correlation lock-in):
  1. Stratified $M_{\text{init}} = 24$ initial candidates over point + rect lights in light-surface domain (unit shift-mapping Jacobian), weighted by unshadowed PBR luminance $\hat{p}$, followed by 1 initial visibility ray.
  2. Temporal reuse with `prev_geometry` normal + previous-camera distance validation, history clamped to $M_{\max} = 20$, and automatic invalidation on `reset_history()`, resize, or light-count changes (`Frame.counts.w`).
  3. Unbiased spatial reuse (3 disk neighbors, strict normal / plane-distance / roughness similarity) using Bitterli et al. (2020) Algorithm 6 exact visibility-aware MIS denominator $Z(y^*)$, preserving exact RectLight MIS ($w_{\text{light}} + w_{\text{brdf}} = 1$) with VNDF reflection rays in `rt_lighting.comp`.
- **Virtual Reflection-Hit Specular Reprojection (`rt_lighting.comp`, `denoise_temporal.comp`):** `rt_lighting.comp` packs reflection ray hit distance `spec_hit_t` (`>0` hit distance, `-1` sky miss, `0` none) into `rt.specular.a` (zero extra textures). `denoise_temporal.comp` computes $P_{\text{virtual}} = P_{\text{cam}} + \hat{V}_{\text{unjit}} \cdot (d_{\text{cam}} + t_{\text{hit}})$, blends surface and virtual-hit UVs smoothly by roughness (merging back into the diffuse tap loop at zero extra fetches on rough surfaces), validates specular taps with world-space plane distance via `inv_prev_view_projection`, tracks independent specular history length in `history.specular.a`, and applies roughness-scaled 3×3 neighborhood variance clamping.

### 3.6 DDGI Encoding & Hysteresis
- **Irradiance atlas:** `RGBA16Float`, `8×8` interior + `1`-texel octahedral border (`10×10` per probe), storing linear irradiance $E$ (sampled as $E/\pi$ for Lambertian surfaces).
- **Distance-moment atlas:** `RG16Float`, `16×16` interior + `1`-texel border (`18×18` per probe), storing **normalized** moments $(d / d_{\max}, (d / d_{\max})^2) \in [0, 1]^2$ where $d_{\max} = \text{ddgi\_max\_dist}(f) = 1.5 \|\text{spacing}\|$. Eliminates FP16 overflow at arbitrary world scales and halves distance atlas bandwidth.
- **Backface & initialization safety:** Backface rays store negative distance `-t * 0.2` in `ddgi_rays.a`; `ddgi_update` excludes them from irradiance and zeros irradiance for probes with $>25\%$ backface hits. On Frame 0 / after `reset_history()` (`rt_counts.w == 0u`), `ddgi_trace` sets `use_ddgi = false` so uninitialized history atlases are never sampled.

### 3.7 Color Space & Post-Processing
- **Working space:** All lighting, GI, denoising, TAA, and bloom operate in **linear HDR (`RGBA16Float`)**.
- **Bloom:** 6-level downsample pyramid (13-tap Jimenez filter + Karis average on mip 0) + 5-level tent upsample using equal-weight `mix(here, up, step / (step + 1))` for exact unit DC gain. `tonemap.frag` blends `mix(hdr, glow, bloom_strength)`.
- **Tonemap & sRGB:** Exposure multiply → Narkowicz ACES → exact IEC 61966-2-1 `astral_linear_to_srgb` when rendering to a linear/UNORM target (`apply_srgb != 0`), or raw linear output when the swapchain/target is hardware sRGB (`*Srgb`). UI and transparent passes share the exact same IEC transfer functions.

---

## 4. C++ ↔ GLSL Layout Synchronization

All BDA structs use `scalar` layout (`GL_EXT_scalar_block_layout` / Vulkan 1.2 `scalarBlockLayout`) and are verified at compile time with `static_assert(sizeof(...))` and `static_assert(offsetof(...))` in `modules/render/src/gpu_layouts.hpp`:

| Struct (C++ / GLSL) | Size (bytes) | Notable Fields Added/Updated |
|---|---|---|
| `GpuFrame` / `Frame` | `608` | `prev_camera_position` (`Vec4`, offset `528`), `inv_prev_view_projection` (`Mat4`, offset `544`), `counts[3]` ReSTIR flags (`bit 0 = active`, `bit 1 = history valid`). |
| `GpuReservoir` / `Reservoir` | `16` | Compact 128-bit ReSTIR DI reservoir (`light_id`, `uv_packed`, `W`, `M`). |
| `RtLightingPush` | `128` | Added `reservoirs` BDA (`uint64_t`, offset `56`). |
| `RestirPush` | `120` | Shared push-constant block for `restir_di_temporal.comp` and `restir_di_spatial.comp`. |
| `GpuShader` / `GpuShader` | `32` | `alpha_test` flag (`uint32_t`). |
| `BloomUpPush` | `36` | `blend` (`float`, offset `32`) for normalized equal-weight upsample. |
| `TlasInstance` (`modules/gpu`) | CPU-side | `bool opaque = true` controlling `VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR` vs `FORCE_NO_OPAQUE_BIT_KHR`. |
