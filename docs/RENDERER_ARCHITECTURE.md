# Astral Engine — Rendering Architecture & Contracts

This document specifies the modernized Vulkan 1.3 rendering architecture of Astral Engine after the `render-audit-refactor` overhaul, including its modular shader library, shared mathematical/color/coordinate contracts, and CPU↔GPU layout synchronization.

---

## 1. High-Level Architecture

Astral's renderer (`modules/render`) sits at Layer 2 on top of `modules/gpu` (Vulkan 1.3 + `VK_KHR_buffer_device_address` + descriptor indexing + `VK_KHR_ray_query` + `scalarBlockLayout`) and `modules/shader` (`.ashader` parser/compiler). It exposes a data-oriented `RenderScene` API (`Renderer::render`) decoupled from ECS and runs one of two automatically selected execution paths:

1. **Ray-Traced Hybrid Path** (`modules/render/src/paths/ray_traced_path.cpp`):
   `TLAS build` → `gbuffer` (Halton-jittered) → `sdf` *(if SDF shapes exist)* → `ddgi.trace` → `ddgi.update` (irradiance + distance) → `rt.lighting` → `denoise.temporal` → `denoise.atrous` (×N) → `composite` → `sky` *(optional)* → `taa` → `bloom.down` (×6) → `bloom.up` (×5) → `tonemap` → `transparent` (forward) → `post` *(optional)* → `debug.lines` → `ui`.
2. **Raster Fallback Path** (`modules/render/src/paths/raster_path.cpp`):
   `shadow` (2048² orthographic PCF) → `gbuffer` → `sdf` *(if SDF shapes exist)* → `lighting` → `sky` *(optional)* → `tonemap` → `transparent` → `post` *(optional)* → `debug.lines` → `ui`.

All transient textures, layout transitions, and synchronization barriers are managed declaratively by `RenderGraph` (`modules/render/src/render_graph.cpp`).

---

## 2. Shared Shader Module Architecture

All duplicated math, color, and BSDF formulas have been consolidated into five layered GLSL include modules under `modules/render/shaders/include/astral/` (and embedded into C++ string tables via `modules/render/CMakeLists.txt` so runtime `.ashader` compilation uses the exact same headers):

| Header | Responsibilities |
|---|---|
| `astral/bindless.glsl` *(in `modules/gpu`)* | Global descriptor set `0` arrays (`astral_textures[]`, `astral_images[]`, `astral_uimages[]`, `astral_samplers[]`, `astral_tlases[]`) and canonical sampler indices (`0=linear`, `1=nearest`, `2=shadow`). |
| `astral/color.glsl` | Rec.709 luminance (`astral_luminance`), exact piecewise IEC 61966-2-1 sRGB encode/decode (`astral_linear_to_srgb`, `astral_srgb_to_linear`), and Narkowicz ACES filmic curve (`astral_aces_tonemap`). |
| `astral/brdf.glsl` | Unified GGX/Smith microfacet contract: `ggx_d`, `smith_g1`, `smith_v_correlated`, `smith_vndf_weight`, `vndf_reflection_pdf`, Heitz 2018 `sample_vndf`, `fresnel_schlick`, Karis `env_brdf`, `specular_brdf_radiance`, `specular_brdf`, `eval_direct_pbr`, and `sky_radiance`. |
| `astral/scene.glsl` | Scalar-layout BDA structs synchronized with `modules/render/src/gpu_layouts.hpp` (`Frame`, `Instance`, `Material`, `PointLight`, `RectLight`, `SdfShape`, `Vertex`, `GpuShader`), `normal_matrix_of(mat4)` (signed cofactor), and `compute_velocity(vec4, vec4)`. |
| `astral/sdf.glsl` | Analytical SDF primitives, CSG operators, guarded tetrahedral normal estimation (`sdf_normal`), sphere tracing (`sdf_march`), and soft shadow marching (`sdf_shadow`). |
| `astral/primary.glsl` | Primary-hit merger (`read_primary` combining mesh + SDF G-buffers), reverse-Z world reconstruction (`world_from_depth`), jittered `camera_ray` and unjittered `unjittered_camera_ray`. |
| `astral/rt.glsl` | Combined TLAS+SDF ray tracing (`trace_ray`, `trace_shadow`, optional `ASTRAL_RT_ALPHA_TEST`), Wächter & Binder (2019) `offset_ray`, `primary_ray_origin`, barycentric triangle reconstruction with `ng`, DDGI normalized-moment sampling, direct lighting (sun, point/rect with RIS + RectLight MIS), and `shade_secondary`. |

---

## 3. Shared Rendering Contracts

### 3.1 Coordinate System, Matrices & Depth
- **World space:** Right-handed, $+Y$ up, meters.
- **Clip / Depth:** Reverse-Z depth (`1.0` at near plane, `0.0` at far plane; Vulkan `[0, 1]` clip Z, `D32Float` for meshes and `R32Float` for SDFs; `depth == 0.0` denotes sky/miss). Shadow map uses standard orthographic `[0, 1]` depth.
- **Jittered vs. Unjittered Matrices (`Frame`):**
  - `view_projection` / `inv_view_projection`: current frame with Halton(2, 3) sub-pixel jitter (0 jitter on raster path or when TAA is disabled).
  - `unjittered_view_proj` / `inv_unjittered_view_proj`: current frame without jitter (used for motion vectors, sky reprojection in `taa.comp`, and debug lines).
  - `prev_view_projection`: previous frame **unjittered** view-projection.
  - `camera_position` / `prev_camera_position`: current and previous frame world-space camera origins (used by `denoise_temporal.comp` to validate previous-frame view distances).

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
- **Alpha-tested geometry:** Instances referencing an `alpha_test` `.ashader` are flagged `FORCE_NO_OPAQUE` in TLAS (`TlasInstance::opaque = false`). When any active custom shader uses `alpha_test`, `ShaderStore::compile_rt_variants` compiles `rt_lighting` and `ddgi_trace` with `#define ASTRAL_RT_ALPHA_TEST 1`, evaluating the custom `surface()` alpha test inside the `rayQueryProceedEXT` loop.

### 3.5 DDGI Encoding & Hysteresis
- **Irradiance atlas:** `RGBA16Float`, `8×8` interior + `1`-texel octahedral border (`10×10` per probe), storing linear irradiance $E$ (sampled as $E/\pi$ for Lambertian surfaces).
- **Distance-moment atlas:** `RG16Float`, `16×16` interior + `1`-texel border (`18×18` per probe), storing **normalized** moments $(d / d_{\max}, (d / d_{\max})^2) \in [0, 1]^2$ where $d_{\max} = \text{ddgi\_max\_dist}(f) = 1.5 \|\text{spacing}\|$. Eliminates FP16 overflow at arbitrary world scales and halves distance atlas bandwidth.
- **Backface & initialization safety:** Backface rays store negative distance `-t * 0.2` in `ddgi_rays.a`; `ddgi_update` excludes them from irradiance and zeros irradiance for probes with $>25\%$ backface hits. On Frame 0 / after `reset_history()` (`rt_counts.w == 0u`), `ddgi_trace` sets `use_ddgi = false` so uninitialized history atlases are never sampled.

### 3.6 Color Space & Post-Processing
- **Working space:** All lighting, GI, denoising, TAA, and bloom operate in **linear HDR (`RGBA16Float`)**.
- **Bloom:** 6-level downsample pyramid (13-tap Jimenez filter + Karis average on mip 0) + 5-level tent upsample using equal-weight `mix(here, up, step / (step + 1))` for exact unit DC gain. `tonemap.frag` blends `mix(hdr, glow, bloom_strength)`.
- **Tonemap & sRGB:** Exposure multiply → Narkowicz ACES → exact IEC 61966-2-1 `astral_linear_to_srgb` when rendering to a linear/UNORM target (`apply_srgb != 0`), or raw linear output when the swapchain/target is hardware sRGB (`*Srgb`). UI and transparent passes share the exact same IEC transfer functions.

---

## 4. C++ ↔ GLSL Layout Synchronization

All BDA structs use `scalar` layout (`GL_EXT_scalar_block_layout` / Vulkan 1.2 `scalarBlockLayout`) and are verified at compile time with `static_assert(sizeof(...))` and `static_assert(offsetof(...))` in `modules/render/src/gpu_layouts.hpp`:

| Struct (C++ / GLSL) | Size (bytes) | Notable Fields Added/Updated |
|---|---|---|
| `GpuFrame` / `Frame` | `544` | Added `prev_camera_position` (`Vec4`, offset `288`). |
| `GpuShader` / `GpuShader` | `32` | Added `alpha_test` flag (`uint32_t`). |
| `BloomUpPush` | `36` | Added `blend` (`float`, offset `32`) for normalized equal-weight upsample. |
| `TlasInstance` (`modules/gpu`) | CPU-side | Added `bool opaque = true` controlling `VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR` vs `FORCE_NO_OPAQUE_BIT_KHR`. |
