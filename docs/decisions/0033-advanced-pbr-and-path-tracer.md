# ADR-0033: Gelişmiş PBR Modelleri, İnce Genişletilmiş G-Buffer, Decal Pass ve Path Tracer Referans Modu (M13)

- **Durum:** Kabul (2026-11-23)
- **Kapsam:** `Astral::Shader`, `Astral::Render`, `examples/material_showcase`

## Bağlam

M10’da `.ashader` dili ve temel metallic-roughness PBR kuruldu; M13.5/M13.6’da renderer tek sorumluluklu pass’lere bölündü ve ardından ortak `brdf.glsl`, ReSTIR DI ve sanal yansıma derinliği reprojeksiyonu eklendi. M13 yol haritası enerji koruyan çoklu saçılma, clearcoat, sheen, anizotropi, transmission (kırılma), subsurface scattering, ıslaklık, detay haritaları / POM, decal’lar ve tarafsız (unbiased) birikimli path tracer referans modunu hedefler.

Mevcut kısıtlar:
- `RtLightingPush` 128 baytlık Vulkan push-constant sınırındaydı; yeni G-buffer bağlamaları için yer açılmalıydı.
- `code_size_check` bütçesi (dosya ≤ 600 satır, fonksiyon ≤ 80 satır, yapı ≤ 25 üye) katı modda zorunlu.
- Tüm BSDF modelleri raster, hibrit RT, ReSTIR DI ve path tracer arasında tek matematik kaynağına bağlı kalmalı.

## Karar

1. **Tek BSDF kaynağı (`astral/bsdf.glsl`):** Çoklu saçılma (Turquin 2019), Clearcoat, Sheen (Estevez & Kulla 2017 Charlie NDF), Anisotropic GGX (Heitz 2014/2018), Transmission (dielektrik Fresnel + Snell kırılması + Beer-Lambert hacim soğurması), Subsurface Scattering ve Wetness tek ortak GLSL başlığında toplanır ve çalışma anı `.ashader` derleyicisine gömülür.
2. **İnce genişletilmiş G-buffer (`material_ext`):**
   - `emissive.a` shading model kodunu taşır (`0` sky, `1` Standard, `2` Unshaded, `3` Clearcoat, `4` Sheen, `5` Anisotropic, `6` Transmission, `7` Subsurface).
   - 5. renk eki `gbuffer.material_ext` (`RGBA16Float`) modele özgü 4 skaler parametreyi taşır.
3. **Sahne BDA adresleri `GpuFrame` içinde:** `tlas`, `instances`, `materials`, `lights`, `rects`, `shapes`, `decals` BDA adresleri `GpuFrame` yapısına taşınır; pass push-constant blokları 128 bayt sınırının oldukça altında tutulur.
4. **`.ashader` `shading_model` bildirimi:** `shading_model standard | clearcoat | sheen | anisotropic | transmission | subsurface;` başlık ifadesi `Astral::Shader` ayrıştırıcısına eklenir. `surface()` içinde yeni büyük harfli built-in’ler (`CLEARCOAT`, `CLEARCOAT_ROUGHNESS`, `SHEEN_COLOR`, `SHEEN_ROUGHNESS`, `ANISOTROPY`, `ANISOTROPY_ROTATION`, `TRANSMISSION`, `IOR`, `THICKNESS`, `ATTENUATION_COLOR`, `ATTENUATION_DISTANCE`, `SUBSURFACE`, `SUBSURFACE_COLOR`, `SUBSURFACE_RADIUS`, `WETNESS`) ve yardımcılar (`astral_parallax_uv`, `astral_detail_normal`, `astral_apply_wetness`) tanımlıdır.
5. **Gruplu `MaterialDesc` ve 192 B `GpuMaterial`:** `MaterialDesc` 25 üye sınırını aşmamak için `ClearcoatDesc`, `SheenDesc`, `AnisotropyDesc`, `TransmissionDesc`, `SubsurfaceDesc`, `DetailDesc` alt yapılarını kullanır; `GpuMaterial` skaler düzende 192 bayt olup `static_assert` ile `scene.glsl` ile kilitlidir.
6. **Deferred `DecalPass` (`decal.comp`):** `RenderScene::decals` boş değilse `gbuffer` + `sdf` sonrasında çalışır; yönlendirilmiş kutu projeksiyonuyla mesh ve SDF G-buffer’larına albedo, roughness, metallic, normal ve ıslaklık (wetness) harmanlar. Decal yoksa sıfır maliyetlidir.
7. **Progresif Path Tracer Referans Modu (`PathTracerPass`):** `RenderSettings::path_tracer = true` olduğunda hibrit RT/denoiser/TAA yerine çok-sekmeli (NEE + MIS + Russian roulette + kırılma + SSS) `RGBA32Float` progresif birikim çalışır; kamera veya ayar değişiminde birikim otomatik sıfırlanır.

## Sonuçlar

- Raster, Hibrit RT, ReSTIR DI ve Path Tracer aynı BSDF formüllerini paylaşır; hibrit yaklaşım hataları doğrudan path tracer referansıyla nicel olarak ölçülebilir.
- `WETNESS` ve `DecalPass` doğrudan M14 (VFX izleri) ve M17 (yağmur/ıslaklık) fazlarına hazır temel sağlar.
- Mevcut `.ashader` ve `MaterialDesc` kullanıcıları kaynak uyumlu kalır (varsayılan `ShadingModel::Standard`).
