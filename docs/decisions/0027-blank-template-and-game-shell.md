# ADR-0027 — Boş oyun şablonu ve standart oyun kabuğu

- **Durum:** Kabul
- **Tarih:** 2026-11-22
- **Bağlam:** ADR-0023 (özellik paketleri), ADR-0016 (paketleme), ADR-0017/0026 (Luau). M11 sonunda her örnek kendi
  `main()`'ini, kendi `--capture` ayrıştırmasını ve kendi PPM yazıcısını taşıyordu; yeni bir oyunun nereden başlayacağı belli değildi.

## Karar

1. **`templates/blank`** motorun güncel mimarisindeki en küçük tam oyundur: `src/main.cpp` (standart kabuk), `src/game.cpp`
   (özellikleri kurar, ilk sahneyi açar; kural yok), `src/features/<ad>/` (C++ veri tipleri, ağır sistemler, `install_<ad>`),
   `assets/features/<ad>/` (`feature.json`, Luau, `.actions`), `assets/*.scene`, `tests/game_test.cpp` (başsız), `CMakeLists.txt`
   (oyun kütüphanesi + exe + `astral_add_game` + test + `astral_check_features`). Motor reposunda `add_subdirectory` ile, dışarıda
   `find_package(Astral)` ile derlenir (CI ikisini de dener).
2. **Standart oyun kabuğu** (`astral/app/shell.hpp`): `parse_shell_args` + `run_game`. Ortak bayraklar `--capture`, `--frames`,
   `--width`, `--height`, `--script-api`; geri kalan argümanlar oyuna kalır. Capture'da çevrimdışı görüntü ve sabit 60 Hz adım,
   pencerede Esc çıkar. Böylece görüntü testleri her oyunda aynı komutla çalışır.
3. **Oyun kodu bir kütüphanedir** (`<ad>_game_lib`): exe ve testler aynı kodu kullanır.
4. **`astral_check_features`** (AstralGame.cmake): özellik sınır denetimi her oyunun CTest'ine tek satırla eklenir.
5. **Bütün örnekler şablonun düzeninde yeniden kuruldu:** hello_headless (eski hello_core), hello_triangle (eski GPU örneği değil, `App` +
   `.ashader` ile en küçük resim), spinning_cubes, sdf_scene, physics_demo, rt_showcase, model_viewer, scene_viewer, shader_showcase, pong,
   open_world. Tekrarlanan kural özellikleri (`fly_camera`) örnek başına kopyadır (her oyun kendi paketini taşır; ortak özellik deposu
   ileride).
6. Script için `astral.quat` (eksen-açı, çarpma, döndürme, normalize) eklendi: kamera/hareket kuralları Luau'da yazılabilsin.

## Sonuçlar

- Her örnek aynı `--capture` ile görüntü testine girebilir; yeni goldens: blank, hello_triangle, spinning_cubes, sdf_scene.
- `rt_showcase` ve `model_viewer` artık kendi renderer döngüsünü değil `App`'in çevrimdışı yolunu kullanıyor (daha az kod, aynı görüntü;
  rt_showcase golden'ı değişmeden geçti).
- Alt katman (RHI) kullanımını gösteren bağımsız örnek kalmadı; `tests/integration` pencere + swapchain yolunu sınar.
- `open_world` görüntü testi kameranın float yuvarlamasına duyarlı çıktı (bkz. render.md → Bilinen sınırlar).
