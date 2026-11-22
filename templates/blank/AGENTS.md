# Yapay zekâ ajanları için

Bu oyunu yazarken motorun kurallarına uy: motor reposundaki [AGENTS.md](https://github.com/inkbytefo/AstralEngine/blob/v3/AGENTS.md)
(özet aşağıda). Önce `build/blank --script-api docs/api` ile `API.md` ve `astral.d.luau` üret ve oku.

1. Önce başarısız test (`tests/game_test.cpp`, başsız), sonra kural, sonra geçir: `cmake --build build && ctest --test-dir build`.
2. Oyun = özellik paketleri: `src/features/<ad>/` (C++ tipleri/ağır sistemler) + `assets/features/<ad>/` (`feature.json`, Luau, `.actions`).
3. Component = veri; sistem = tek iş; özellikler birbirini çağırmaz, olay ve component ile konuşur; durum component'te; sayılar dosyada.
4. `feature.json` özelliğin API'sidir: `install()` içinde kaydettiğin her component/olay orada listelenmeli.
5. Başka özelliğin yalnız `exports.headers` başlıklarını ve `depends_on`'daki özellikleri kullan; iç motor başlığı ve `static` global yasak (`ctest -R features`).
6. Kural `FixedUpdate`'te (deterministik); girdi eylemleri `.actions` dosyasından, tuş kodlarından değil.
