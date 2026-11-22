# Blank game

Boş bir AstralEngine oyunu. Bu klasörü kopyala, `blank` adını değiştir, `src/features/` ve `assets/` ile başla.

```
CMakeLists.txt                     oyun kütüphanesi + exe + testler + özellik sınırı denetimi
src/main.cpp                       standart main (komut satırı: --capture, --script-api ...)
src/game.cpp                       özellikleri kurar, ilk sahneyi açar — kural yok
src/features/<ad>/                 C++ tarafı: component tipleri (types), ağır sistemler, install_<ad>
assets/features/<ad>/              feature.json (özelliğin API'si) + Luau kuralları + .actions
assets/main.scene                  ilk sahne (kamera, zemin, küp)
tests/game_test.cpp                başsız, deterministik testler
```

Kurallar: [AGENTS.md](AGENTS.md). Derle ve dene:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<motorun kurulum klasörü>   # motor reposunun içindeysen gerekmez
cmake --build build && ctest --test-dir build
build/blank                         # oyna
build/blank --capture out.ppm       # tek görüntü (GPU gerekir)
build/blank --script-api docs/api   # astral.d.luau + API.md
cmake --build build --target blank_package   # dağıtılabilir klasör: build/packages/blank/
```

Yeni özellik: `src/features/<ad>/` + `assets/features/<ad>/feature.json` oluştur, `game.cpp`'ye
`game.add("<ad>", install_<ad>);` ekle (saf Luau özellik için install gerekmez: `game.add("<ad>")`).
Sürümü kopyaladığın motorun sürümüyle eşle: `find_package(Astral 0.17 ...)`.
