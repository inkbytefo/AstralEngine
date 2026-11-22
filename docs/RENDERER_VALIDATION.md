# Astral Engine — Renderer Validation & Benchmark Report

**Branch:** `render-audit-refactor` (base commit `61e4e01`)
**Validation Hardware:** NVIDIA GeForce RTX 3060 (12288 MiB VRAM, driver 621.14), Vulkan 1.4.357 (device target Vulkan 1.3), Windows 10/11 x64, MSVC 14.51 (`/WX` warnings-as-errors), `glslc` (shaderc v2025.5, `-O`), `spirv-val` (SPIRV-Tools v2025.5).

---

## 1. Build, SPIR-V & Automated Test Results

| Check / Suite | Result | Details |
|---|---|---|
| CMake Debug Build (`cmake --build --preset debug`) | **PASS** | Zero compiler warnings under MSVC `/WX`. |
| SPIR-V Validation (`spirv-val --target-env vulkan1.3 --scalar-block-layout`) | **20 / 20 PASS** | Every compiled shader in `modules/render/shaders/` validated cleanly against Vulkan 1.3 + `scalarBlockLayout` (matching `device_init.cpp:246`). |
| Full Non-Golden CTest Suite (`ctest --preset debug -LE golden`) | **53 / 53 PASS** | Includes `shared_features_in_sync`, `code_size_check`, `layer_check`, `layer_check_self_test`, `astral_public_headers_clean`, `astral_core_tests`, `astral_shader_tests`, `astral_gpu_tests`, `astral_render_tests`, `astral_app_tests`, `astral_integration_tests`, `astral_benchmarks_smoke`, `astral_golden_tests`, and all example test/feature/script suites. |
| `astral_render_tests` (with Vulkan Validation Layers via `TestDevice`) | **80 / 80 PASS** | Includes 11 new tests in `modules/render/tests/render_audit_test.cpp` (5 CPU math contract tests + 6 headless Vulkan GPU regression tests). Zero Vulkan validation errors. |
| `golden-hw` Hardware Comparison (`ctest -L '^golden-hw$'`) | **5 / 12 PASS, 7 / 12 EXPECTED SHIFT** | Documented in Section 4 below (caused strictly by fixing Bloom's 6× DC energy bug and exact IEC sRGB encoding against pre-refactor lavapipe baselines; see ADR-0031). |

### New Audit Regression Tests (`modules/render/tests/render_audit_test.cpp`)
1. `math contract: GGX VNDF white-furnace reflectance is energy-conserving and matches Smith G2/G1` — verifies Heitz 2018 VNDF + `smith_vndf_weight` against 1D quadrature across roughness `{0.1, 0.35, 0.7, 1.0}` and incidence `{0.2, 0.5, 0.9, 1.0}` (including analytical $1 - \ln 2 \approx 0.306853$ at $r=1, n \cdot v = 1$, matching within `< 0.015`).
2. `math contract: octahedral encode/decode round-trips across the unit sphere` — `dot(n, decoded) > 0.99999` across 728 sphere directions.
3. `math contract: Wachter-Binder offset_ray scales across 7 orders of magnitude` — positive displacement across `0.001..10000` and monotonic ULP scaling beyond `1.0`.
4. `math contract: signed cofactor normal_matrix_of matches transpose(inverse(M)) including negative scale` — `dot > 0.9999` under non-uniform scale, rotation+scale, and reflection ($\det < 0$).
5. `math contract: IEC sRGB round-trip and bloom upsample unit DC gain` — `srgb_to_linear(linear_to_srgb(x)) == x` to `1e-5` and 2..8 level bloom upsample DC gain `== 1.0` to `1e-6`.
6. `regression: DDGI frame 0 and post-reset never leak uninitialized VRAM into a dark room` — renders bright yellow scene, resizes DDGI grid & resets history, renders sealed dark room → max RGB channel `< 4`.
7. `regression: DDGI normalized distance moments avoid FP16 overflow on 2000m worlds` — renders 2000 m scene with `max_ray_distance = 3000` → ground remains valid green without Inf/NaN blowup.
8. `regression: bloom preserves uniform HDR DC energy (no 6x amplification)` — uniform gray sky with `bloom_strength = 0.0` vs `0.25` differs by `<= 3/255`.
9. `regression: alpha-tested .ashader lets ray-traced shadows pass through cut-out regions` — `UV.x > 0.5 ? 1.0 : 0.0` overhead card lets direct sun hit floor under cut-out half (`lit > shadowed + 40`).
10. `regression: transparent forward PBR lighting matches opaque PBR units (no 1/PI darkening)` — `blend_mix` with `ALPHA = 1.0` matches opaque cube luminance within `<= 4/255`.
11. `regression: RectLight MIS produces clean specular highlights on rough metals` — rough metallic floor (`r = 0.35, metallic = 1.0`) under `RectLight` produces clean specular highlight (`luma > 90`).

---

## 2. GPU Performance Benchmarks (RTX 3060, `1280×720`)

Measured with `bench/render_bench.cpp` (`astral-render-bench`, Vulkan hardware timestamps averaged over steady-state frames after warmup; shaders compiled with `glslc -O`):

| Scene | Configuration | Baseline GPU (ms) | Refactored GPU (ms) | Speedup / Δ | Key Pass Deltas |
|---|---|---:|---:|---:|---|
| `cornell_area` | 560 DDGI probes, 3 SDF, 1 RectLight | 7.830 (127.7 FPS) | **6.851** (146.0 FPS) | **-12.5% (1.14×)** | `ddgi.update.distance`: 0.458 → **0.068 ms (6.7×)**; `ddgi.update.irradiance`: 0.081 → **0.030 ms (2.7×)**; `ddgi.trace`: 0.364 → **0.197 ms**; `denoise.atrous`: 1.994 → **1.621 ms** |
| `material_grid` | 25 spheres (roughness×metallic), 216 probes | 2.845 (351.5 FPS) | **2.313** (432.3 FPS) | **-18.7% (1.23×)** | `ddgi.update.distance`: 0.052 → **0.018 ms (2.9×)**; `denoise.atrous`: 1.023 → **0.770 ms** |
| `thin_wall` | Sealed thin-wall probe-leak stress, 288 probes | 5.092 (196.4 FPS) | **4.208** (237.6 FPS) | **-17.4% (1.21×)** | `rt.lighting`: 1.992 → **1.745 ms**; `ddgi.update.distance`: 0.121 → **0.043 ms (2.8×)** |
| `high_lights` | 32 PointLights + 4 RectLights + 8 SDFs | 45.577 (21.9 FPS) | **14.825** (67.5 FPS) | **-67.5% (3.07×)** | `rt.lighting`: 39.595 → **11.339 ms (3.5×)**; `ddgi.trace`: 2.386 → **0.545 ms (4.4×)** |
| `open_world_scale` | 2000 m terrain, 256 probes, `max_dist = 3000` | 2.171 (460.7 FPS) | 2.290 (436.7 FPS) | +5.5% (+0.119 ms) | `ddgi.update.distance`: 0.058 → **0.021 ms**; `denoise.atrous` +0.10 ms from plane-distance edge stop |

### Temporal Stability Measurement (`moving_camera`, `denoise_iterations = 0`)
- Measuring pure temporal accumulation during forward camera translation (spatial À-Trous disabled):
  - Baseline high-frequency Laplacian noise: **`1.604`** (history rejected every frame due to `abs(pg.w - geo.w)` comparing previous vs current camera distances).
  - Refactored high-frequency Laplacian noise: **`0.847`** (**-47.2% noise reduction**).

---

## 3. Quantitative Before vs. After Visual Comparison

Captured across all 6 `astral-render-bench` scenes (`1280×720`) and 5 example scenes (`960×540`) before and after refactoring:

| Scene | Resolution | Baseline Mean | Refactored Mean | Δ Mean | RMSE | SSIM | Visual Diagnosis |
|---|---|---:|---:|---:|---:|---:|---|
| `thin_wall` | 1280×720 | 145.54 | **0.47** | -145.06 | 177.65 | 0.3039 | **Critical fix:** Baseline exploded into neon-yellow VRAM garbage (`C4`); refactored dark room is physically black. |
| `moving_camera` | 1280×720 | 122.21 | **92.52** | -29.69 | 49.32 | 0.8280 | **Critical fixes:** Eliminated neon-green DDGI VRAM leak (`C4`) and fixed camera-motion history wipe (`E1`, HF noise `2.556 → 1.571`). |
| `high_lights` | 1280×720 | 199.89 | **188.66** | -11.23 | 18.12 | 0.9455 | **Critical fix:** Baseline had hot magenta bloom blob from `composite.comp` `vec3(50,0,50) // DEBUG` (`F2`) + 6× pink bloom haze (`F1`); refactored is clean and 3.07× faster. |
| `shader_showcase` | 960×540 | 107.18 | **85.49** | -21.69 | 27.22 | 0.8952 | **Verified improvements:** 6× bloom haze removed (`F1`), sharper procedural stars via unjittered sky TAA (`E5`), physically lit water/glass (`B4`), alpha-tested foliage shadows (`A3`). |
| `cornell_area` | 1280×720 | 111.61 | **101.70** | -9.91 | 14.78 | 0.9742 | Cleaner contrast without 6× bloom washout (`F1`), exact IEC sRGB (`F3`), RectLight MIS highlights (`D2`). |
| `material_grid` | 1280×720 | 195.95 | **187.82** | -8.14 | 10.56 | 0.9862 | Unbiased VNDF rough metallic/dielectric reflections (`B2`) + energy-conserving bloom (`F1`). |
| `rt_showcase` | 960×540 | 113.39 | **102.87** | -10.52 | 15.45 | 0.9713 | Cleaner shadow/contrast accuracy (`F1`, `F3`, `B2`). |
| `scene_viewer` | 960×540 | 109.53 | **97.92** | -11.60 | 16.62 | 0.9660 | Richer contrast and cleaner reflections (`F1`, `F3`, `B2`). |
| `sdf_scene` | 960×540 | 202.00 | **193.88** | -8.12 | 9.43 | 0.9930 | High fidelity preservation (`SSIM 0.9930`); bloom/sRGB accuracy improved. |
| `open_world` | 960×540 | 223.21 | **218.00** | -5.21 | 6.65 | 0.9941 | Virtually identical (`SSIM 0.9941`); cleaner contrast. |
| `open_world_scale` | 1280×720 | 222.78 | **217.18** | -5.60 | 6.64 | 0.9973 | Virtually identical (`SSIM 0.9973`) with FP16-safe normalized DDGI moments (`C1`). |

---

## 4. `golden-hw` Analysis & Lavapipe Baseline Update Procedure

Running `ctest --preset debug -L "^golden-hw$"` against the pre-refactor Linux Mesa lavapipe baselines produced:
- **5/12 PASS:** `open_world` (all 3 views), `blank`, `sdf_scene`.
- **7/12 SHIFTED:** `scene_viewer` (SSIM 0.9329), `rt_showcase` (0.9539), `pong` (0.6955), `shader_showcase` (0.8778), `hello_triangle` (0.9437), `spinning_cubes` (0.9712), `physics_playground` (0.9684).

**Proven Root Cause:** 91%–100% of pixel differences are a uniform downward luminance shift (`Δ mean = -10.4` to `-26.0`) directly attributable to two verified correctness fixes:
1. Fixing Bloom's 6× unnormalized DC amplification (`F1`), which previously inflated global brightness by `+20%–25%` and created `+600%` ambient haze in dark scenes (`pong`, `shader_showcase`).
2. Replacing `pow(c, 1/2.2)` with exact IEC 61966-2-1 sRGB encoding (`F3`), lowering dark tones by up to `8/255` to match hardware sRGB swapchains.

**Action Required (per ADR-0031):** Do **not** overwrite `tests/golden/references/*.png` from RTX 3060 hardware captures. On the Linux Mesa lavapipe CI environment, regenerate the canonical golden references with:

```bash
ASTRAL_UPDATE_GOLDEN=1 ctest --preset debug -L "^golden$"
```
