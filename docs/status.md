# Durum Raporu

**Taban:** master `a113b55` · **Tarih:** 2026-11-26 · **Ortam:** Windows 11, MSVC 19.51, Ninja, CMake 4.2, RTX 3060 12 GB

Bu sayfa motorun bugün ne kadar hazır olduğunu dürüstçe özetler. Ayrıntılar modül dokümanlarındadır; planlar [roadmap.md](roadmap.md)'dedir. Olgunluk etiketleri: **Stabil** (geniş test, örneklerde kullanılıyor, bilinen açık hata yok) · **Kullanılabilir** (çalışıyor ve test ediliyor, belirgin boşluklar var) · **Deneysel** (kısmi, zayıf test veya bilinen kırık) · **Yok**.

## 1. Modüller

| Modül | Olgunluk | Güçlü yanlar | En önemli zayıf yanlar |
|---|---|---|---|
| [core](modules/core.md) | Stabil | Log, handle, saat, iş sistemi, kayıpsız 64-bit JSON, reflection, profiler | İş sistemi tek kuyruk/tek mutex (work-stealing yok); JSON nesne araması doğrusal; matematik minimal |
| [platform](modules/platform.md) | Kullanılabilir | SDL3 pencere, klavye/fare, gamepad, metin girişi, başsız mod, `keep_responsive` | Tam ekran, pencere konumu ve boyut olayı yok; yalnız ilk gamepad; IME yok; `wasd()` bilinçli olarak normalize edilmez (çapraz uzunluk √2) |
| [gpu](modules/gpu.md) | Kullanılabilir | Vulkan 1.3, bindless + BDA, ray query (BLAS/TLAS), timestamp | Tek iş parçacığı, tek kuyruk; doku/buffer yüklemeleri bloklayıcı; yalnız 2D doku; VRAM bütçesi ve pipeline önbelleği yok |
| [shader](modules/shader.md) | Kullanılabilir | `.ashader` ayrıştırıcı, glslang, SPIR-V disk önbelleği, paralel derleme | SPIR-V optimizer kapalı; önbellek etiketi elle artırılır; `ui` shader türü ayrıştırılıyor ama kullanılmıyor |
| [world](modules/world.md) | Stabil (ECS, sahne/prefab, serileştirme) · Kullanılabilir (harita durumu, Level Instance) | Kendi ECS'i, değişiklik takibi, olaylar, paralel zamanlayıcı, prefab override/nesting, hücre streaming, floating origin | Transform güncellemesinde değişiklik bayrağı yok; çok sayıda doğrusal arama; harita değişiklikleri yalnız bellekte |
| [physics](modules/physics.md) | Kullanılabilir | Jolt v5.7.0; SixDOF/motor/kırılma eklemleri; toplu sorgular; rüzgâr/su alanları; karakter; deterministik komut sırası | Platformlar arası determinizm yok; SixDOF/SwingTwist'te tek aralıklı `set_joint_limits` yalnız uyarı verir (`JointLimitsDesc` gerekir); alanlar yalnız eksen hizalı kutu |
| [audio](modules/audio.md) | Kullanılabilir | miniaudio, bus'lar, 3D ses, sessiz cihaz | Filtre, reverb, oklüzyon, Luau API'si yok; yalnız 4 test |
| [script](modules/script.md) | Kullanılabilir | Luau sandbox, hot reload, staged yükleme, üretilen `astral.d.luau`, `astral-luau-check` | Bütçe modül ve faz başına ama duvar saatiyle ölçülüyor (yük altında sahte aşım; [ADR-0052](decisions/0052-script-budget-per-module.md)); bellek sınırı yok; tip denetimi callback parametrelerini yakalamıyor |
| [render](modules/render.md) | Kullanılabilir | Raster + hibrit RT (DDGI, ReSTIR DI, RT gölge/yansıma, denoiser, TAA, bloom), SDF, decal, gelişmiş PBR, path tracer, özel shader'lar her yolda | RT maliyeti orta sınıf GPU'da yüksek (render scale yok); SpotLight ve DoF yok; path tracer kesin tarafsız değil |
| [assets](modules/assets.md) | Kullanılabilir | glTF/GLB, cook (`astral-cook 7`), `.apak`, hot reload, arazi tile'ları | glTF skin/animasyon/morph atlanıyor; GPU yüklemesi ana iş parçacığında, bütçesiz; boşaltma (unload) yok |
| [app](modules/app.md) | Stabil | Oyun kabuğu, servisler, sabit adımlı fizik köprüsü, FeatureSet, Atmosphere, VFX, HUD, debug UI, başsız/offscreen | `AppDesc` ve `App::Impl` 25/25 üye; `read_frame` yalnız offscreen; VFX loop'suz |
| [editor](modules/editor.md) | `editor_core` Kullanılabilir · `editor` Deneysel | Belge/komut/transaction modeli, undo/redo, ajan önerisi akışı | Yalnız sabit demo projesini açar; Play yalnız fizik önizlemesi; UI hiç otomatik test edilmiyor |
| [araçlar](tools.md) | Kullanılabilir | Cook, paketleme, özellik/katman/boyut/doküman denetimleri, kurulu SDK tüketici testi | Bazı denetimler sezgisel (satır regex'i, yalnız doğrudan kenar); bazıları eski CMake'te (< 3.28) sessizce atlanıyor |

**Hiç olmayanlar:** iskelet animasyonu, araç fiziği, navigasyon, ağ/multiplayer, SpotLight, alan derinliği, pencereli ekran görüntüsü, GPU bellek bütçesi API'si, entity başına materyal override.

## 2. Test durumu

Debug ve Release, `-DASTRAL_WARNINGS_AS_ERRORS=ON`: build temiz, 69 ctest testinden yalnız aşağıdaki 3 `golden_hw` düşüyor (2026-11-27, `b13528b`, examples yenileme sonrası; Windows, RTX 3060). Eski örnekler silinince test sayısı azaldı.

| Durum | Test | Not |
|---|---|---|
| ❌ Her koşumda düşüyor | `golden_hw_pong`, `golden_hw_hello_triangle`, `golden_hw_spinning_cubes` | Bu PC'de ton farkı (ör. `hello_triangle` köşe RGB −11, SSIM 0.9445 < 0.95). Kök neden incelenmedi |
| — Golden testi yok | `physics_showcase`, `lighting_showcase`, `materials_showcase`, `world_showcase`, `vfx_showcase`, `asset_viewer` | Referanslar yalnız lavapipe'ta (CI) üretilir; henüz üretilmedi. Silinen örneklerin 7 golden bloğu ve 9 referansı kaldırıldı |
| ⚠️ Paralel yükte ara sıra | `pong_tests`, `spinning_cubes_tests` | Luau 100 ms bütçe aşımı; muhtemel neden: bütçe duvar saati ve tüm faz için ortak (`modules/script/src/host.cpp:21`) |
| ⚠️ Paralel yükte ara sıra | `astral_render_tests` | DDGI regresyon testinde SIGSEGV (`modules/render/tests/render_audit_test.cpp`); seri koşumda geçiyor |
| — Bu GPU'da atlanıyor | `golden_*` (lavapipe) | Donanımdan bağımsız görüntü testleri yalnız CI'da |
| — Bu makinede çalışmıyor | tsan, asan | Windows/MSVC; tsan CI'da (`linux-tsan`) |

**Test kalitesiyle ilgili uyarılar:**
- Birçok GPU testi cihaz yoksa sessizce geçiyor (`if (!f.device) return;`); CI'da GPU olmayan işlerde bu testler bir şey kanıtlamaz.
- `the roughness cutoff saves ray-traced lighting time` testi hiçbir şey doğrulamıyor (`modules/render/tests/m13_pbr_test.cpp:753`: `CHECK(full >= -1.0f)`).
- Editör arayüzü hiçbir otomatik testte çalışmıyor.
- Projeksiyon yönü düzeltmesi yalnız raster yolunda test edildi.

## 3. Ölçümler

Hepsi RTX 3060 12 GB, `examples/the_architect`. Başka bir ortamda ölçülmedi.

| Ölçüm | Değer | Kaynak |
|---|---|---|
| Açılış (`--capture --frames 4`), Debug | önce 19.1 s → önbellek boş 11.2 s, dolu 3.95 s | merge `31e2c48` |
| Açılış, Release | önce 4.18 s → önbellek boş 2.53 s, dolu 1.76 s | merge `31e2c48` |
| `rt.lighting`, 1080p, rt_high, Release | 10.43 ms; `max_reflection_roughness = 0.35` ile 7.54 ms | merge `31e2c48` |
| ReSTIR DI temporal + spatial, rt_ultra | ≈ 10.9 ms | Dış rapor (`examples/the_architect/ENGINE_GAPS.md`); yeniden ölçülmedi |
| À-trous denoise | 2.6 ms (2 geçiş) / 4.4 ms (3 geçiş) | Dış rapor; yeniden ölçülmedi |
| Raster yol, 1080p | 2.85 ms | Dış rapor; yeniden ölçülmedi |

## 4. Bilinen hatalar ve tutarsızlıklar

Doküman çalışması sırasında kodda bulundu, düzeltilmedi. "Doğrulandı" = kaynakta ayrıca kontrol edildi.
Dalga A'da (2026-11-26) düzeltilip listeden çıkarılanlar: başsız ses mikseri ([ADR-0054](decisions/0054-headless-silent-mixer-clock.md)), script bütçesi
([ADR-0052](decisions/0052-script-budget-per-module.md)), katman gizlemede diff kaybı, editör rollback/ebeveyn döngüsü/silme ([ADR-0053](decisions/0053-editor-rollback-and-hierarchy-rules.md)).
Dalga B'de düzeltilip çıkarılanlar: RT/PT güneş gölgesi bayrağı, `MeshShaders` raporu, `write_texture` kullanım denetimi, `JobSystem::submit` sayacı,
`set_joint_limits` uyarısı, `update_material` busy çağrısı, `astral_check_scripts` uyarısı, wasd test adı, ADR-0040 alan adı notu.

| Alan | Sorun | Kaynak | Durum |
|---|---|---|---|
| app/audio | Ses önbelleği hiç boşaltılmıyor; başarısız yükleme kalıcı olarak önbellekte kalıyor | `modules/app/src/services/audio_bridge.cpp:34-48` | — |
| gpu | `immediate` komut ayırma/gönderme sonuçlarını yok sayıp bekliyor (cihaz kaybında takılabilir) | `modules/gpu/src/device.cpp:210-223` | Tekrarlanmadı |
| physics | `step()` sonuç döndürmüyor; Jolt taşması yalnız log'lanıyor | `modules/app/src/services/physics_bridge.cpp:105` | — |
| shader | SPIR-V optimizer kapalı (çalışma zamanı etkisi ölçülmedi) | `modules/shader/src/compile.cpp:141` | Doğrulandı |
| render | Path tracer radyansı 64'e kırpılıyor; ikincil isabetlerde normal/detay haritası ve POM yok | `modules/render/shaders/path_tracer.comp:372` | Doğrulandı |
| render | `blend_*` yüzeyler TLAS'ta değil (RT/PT'de görünmez); partiküller tonemap sonrası çiziliyor | [render.md](modules/render.md) | — |
| render | Path tracer SDF yüzeylerinde koyu halka + nokta çiziyor (hibrit RT'de yok) | [render.md](modules/render.md) | Gözlendi (capture), nedeni incelenmedi |
| render | Bazı `render_mode`'lar (`rt_simple`, `after_tonemap`, `unshaded` particle/volume) kabul ediliyor ama kullanılmıyor | `modules/shader/src/ashader.cpp:372-382` | — |
| script | `astral-luau-check` sistem callback parametrelerindeki alan hatalarını, bilinmeyen query component'lerini ve olay alanı hatalarını yakalamıyor | `modules/script/src/api.cpp:223` | Ajan tarafından denendi |
| editor_core | `AgentTaskManager::cancel` worker'ı join ediyor; codex süreci zaman aşımı olmadan çalıştığı için UI kilitlenebilir; komut satırı kaçışsız | `modules/editor_core/src/agent_task.cpp:313-315` | — |
| world | `EventReader::read` ile ilk `World::send` arasında olası veri yarışı | `modules/world/include/astral/world/world.hpp:334` | Doğrulanmadı |
| assets | Yalnız occlusion için kullanılan doku hiç cook edilmiyor (sıkıştırmasız RGBA8 kalıyor) | `modules/assets/src/texture_codec.cpp:140` | — |

## 5. Denetimler

| Denetim | ctest | Durum |
|---|---|---|
| Katman bağımlılıkları | `layer_check` | ✅ |
| Editörün oyunlardan yalıtımı | `editor_isolation_check` | ✅ (yalnız doğrudan kenarlar) |
| Public header'larda üçüncü parti tip | `astral_public_headers_clean` | ✅ |
| Boyut bütçesi | `code_size_check` | ✅ (`Context` istisnası; `AppDesc`, `App::Impl` sınırda) |
| Ortak `features/` eşliği | `shared_features_in_sync` | ✅ |
| Doküman linkleri ve kaynak referansları | `docs_check` | ✅ (bu refaktörle eklendi) |
| Testlerin önbellek yalıtımı | `test_environment_check` | ✅ (CMake ≥ 3.28) |
