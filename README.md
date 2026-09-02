# AstralEngine

Modern C++ ile yazilmis, moduler ve genel amacli bir oyun motoru cekirdegi.

## Ozellikler (Planlanan)

- C++20 standartlari
- ECS mimarisi (EnTT ile)
- Vulkan rendering backend (moduler)
- GLFW tabanli pencere yonetimi
- CMake ile platform bagimsiz derleme

## Klasor Yapisi

```
AstralEngine/
├── CMakeLists.txt           # Ana derleme yapilandirmasi
├── include/                 # Disari acik baslik dosyalari
│   └── Astral/
│       └── Core/
│           └── Application.hpp
├── src/                     # Kaynak kodu
│   ├── Core/
│   │   └── Application.cpp
│   └── main.cpp             # Sandbox uygulamasi
├── third_party/             # Harici bagimliliklar (Vulkan, GLFW, EnTT)
└── build/                   # Derleme ciktilari
```

## Derleme

### Windows - MinGW + Ninja (CMake Presets, onerilen)

```powershell
cd AstralEngine
cmake --preset mingw-debug      # build/ altında yapilandirir
cmake --build --preset mingw-debug
.\build\Sandbox.exe
```

> Not: `CMakePresets.json` icindeki MinGW derleyici yollari bu gelistirme
> makinesindeki `C:\mingw64` konumuna goredir; farkli bir kurulumda duzeltin.

### Windows - Visual Studio (MSVC)

```powershell
cd AstralEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\Sandbox.exe
```

### Linux / macOS

```bash
cd AstralEngine
cmake -S . -B build -G "Ninja"
cmake --build build
./build/Sandbox
```

## Kullanim

Sandbox uygulamasi motoru test etmek icin kullanilir. Asil oyununuza
gecerken `Sandbox` executable'i silinip kendi main.cpp dosyaniz
`AstralCore` kutuphanesine baglanir.

## Lisans

MIT (ileride kararlaştırılacak).