# ADR-0047 — SPIR-V disk önbelleği ve paralel shader derleme

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-26
- **Kapsam:** `shader` (`SpirvCache`, `ShaderCompileServices`, `compile_glsl_batch`), `render`, `app`, `assets`
- **İlişkili:** [ADR-0022](0022-shader-and-material-language.md), [ADR-0048](0048-startup-responsiveness-and-mips.md)

## Bağlam

Özel `.ashader` yüzeyleri ve beş ray-traced compute varyantı her açılışta glslang ile ana iş parçacığında derleniyordu. `examples/the_architect` Debug açılışının 19.1 s'sinin yaklaşık 9.5 s'si glslang'di (Release'te 4.18 s'nin 0.8 s'si); pencere bu sürede donuyordu.

## Karar

1. `SpirvCache` (`modules/shader/include/astral/shader/spirv_cache.hpp`): `<sha256>.spv` dosyalarından oluşan içerik adresli bir klasör. Global örnek yok: sahibi (App, araç, test) bir tane oluşturur ve `ShaderCompileServices { cache, jobs }` ile ödünç verir; iki işaretçi de isteğe bağlıdır.
2. Anahtar, derleyicinin gördüğü her şeyin SHA-256'sı: motor önbellek etiketi, glslang sürümü, hedef seçenekleri, aşama, shader adı, tam GLSL metni ve her include (adı ve metni). Bir motor başlığı değişirse kayıt bayatlamaz, yalnızca ıskalar.
3. Kayıt: başlık (magic, sürüm, anahtar, kelime sayısı, FNV-1a-64 sağlama) + SPIR-V. Kesik, bozuk, yanlış anahtarlı her dosya ıska sayılır, yeniden derlenir ve üzerine yazılır. Derleme hataları önbelleğe alınmaz.
4. Yazma atomiktir (geçici dosya + rename). Yazma hataları yok sayılır: önbellek derlemeyi yalnız hızlandırabilir, asla bozamaz.
5. Konum: `%LOCALAPPDATA%` / `$XDG_CACHE_HOME` + `AstralEngine/shader-cache/<AppDesc::title>`. `ASTRAL_SHADER_CACHE=off|0` kapatır, `ASTRAL_SHADER_CACHE=<klasör>` taşır (`AppDesc` 25 üye sınırında olduğu için ortam değişkeni). Renderer'ı olmayan (headless) App önbellek oluşturmaz. Açılışta en eski kullanılanlardan başlayarak 512 MiB'a budanır.
6. `compile_glsl_batch` bir isteğin bağımsız aşamalarını ve RT varyantlarını `JobSystem` üzerinde paralel derler; çıktı seri derlemeyle bayt bayt aynıdır.
7. Renderer servisi taşır (`Renderer::set_shader_compile_services`); MaterialLibrary, glTF materyalleri ve RT yeniden derlemesi aynı önbelleği kullanır.

## Sonuçlar

- Sıcak açılışta glslang hiç çalışmaz: `the_architect` Debug 19.1 s → 3.95 s, Release 4.18 s → 1.76 s.
- Önbellek klasörünü silmek her zaman güvenlidir.
- Testler önbelleği build klasörüne yazar (`ASTRAL_SHADER_CACHE`, `test_environment_check`).
- `astral-cook` doğrulama derlemeleri önbelleği kullanmaz.
- Test: `modules/shader/tests/spirv_cache_test.cpp`, `modules/render/tests/surface_shader_test.cpp`.
