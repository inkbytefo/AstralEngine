# AstralEngine

İstemci başlangıç sahnesi, yaşam döngüsü hook'ları ve ECS kamera kullanımı için
[A2.1 — İstemci sahneleri ve kameralar](docs/A2_1_CLIENT_SCENES_AND_CAMERAS.md)
belgesine bakın. `EmptyGameTemplate` artık kamera ve geometri içermeyen boş bir sahneyle açılır;
eski Sandbox görüntü demosu `--demo` seçeneğiyle çalıştırılır.

Modern C++20 ile yazılmış, modüler, yüksek performanslı genel amaçlı oyun motoru çekirdeği ve Vulkan 1.4 tabanlı Signed Distance Field (SDF) compute raymarching render alt-sistemi.

---

## Temel Mimari Prensipleri

- **Modüler Hedef Ayrımı**: Motor (`AstralEngine` statik kütüphane), editör (`AstralEditor`), oyunlar (`Projects/*`) ve regresyon testleri (`EngineTests`) birbirinden tamamen bağımsız CMake hedefleridir.
- **EntryPoint Deseni**: Motor ana döngüsü (`main()`) `Astral/EntryPoint.hpp` içine taşınmıştır. İstemci oyunlar `Astral::Application` sınıfından türer ve `Astral::CreateApplication()` uygulamasını sunar.
- **Şemsiye Başlık (`AstralEngine.h`)**: Tüm motor API'si (Core, Scene, ECS, Math) tek bir `#include "AstralEngine.h"` ile istemci projelerine sunulur.
- **Zero-ImGui Motor Çekirdeği**: `libAstralEngine.a` ve oyun projeleri hiçbir Dear ImGui bağımlılığı içermez. Tüm ImGui ve ImGuizmo kodları müstakil `AstralEditor.exe` hedefine izole edilmiştir.
- **Headless CI-Dostu Testler**: Regresyon testleri (`EngineTests.exe`) pencere/GPU gerektirmeden saf CPU üzerinde (<15 ms) çalışır. GPU gerektiren testler opsiyonel `--gpu` parametresiyle kategorize edilmiştir.

---

## Klasör Yapısı

```
AstralEngine/
├── CMakeLists.txt                  # Ana kök derleme yapılandırması
├── AstralEngine/                   # Motor çekirdek statik kütüphanesi (libAstralEngine.a)
│   └── CMakeLists.txt
├── include/                        # Motorun genel (public) API başlıkları
│   ├── AstralEngine.h              # Tüm motoru tek seferde dahil eden şemsiye başlık
│   └── Astral/
│       ├── EntryPoint.hpp          # Motor ana döngü / main() giriş noktası
│       ├── Core/                   # Application, ISubsystem, SystemManager, Window, Registry, vb.
│       ├── Renderer/               # VulkanContext, SDFRenderer, Buffer, BrickGrid, vb.
│       └── Scene/                  # Scene, Entity, SceneSerializer, Components, vb.
├── src/                            # Motor çekirdek kaynak kodları (Core, Renderer, Scene)
├── Tools/
│   └── AstralEditor/               # Bağımsız editör hedefi (AstralEditor.exe — ImGui & ImGuizmo)
│       ├── CMakeLists.txt
│       └── src/                    # EditorUI, paneller (Viewport, Hierarchy, Inspector, vb.)
├── Projects/
│   ├── Sandbox/                    # Referans çalışma zamanı oyunu (Sandbox.exe)
│   │   ├── CMakeLists.txt
│   │   └── src/main.cpp
│   └── EmptyGameTemplate/          # Minimal 15 satırlık şablon oyun projesi
│       ├── CMakeLists.txt
│       └── src/main.cpp
├── Tests/
│   └── EngineTests/                # Bağımsız regresyon test koşucusu (EngineTests.exe)
│       ├── CMakeLists.txt
│       └── src/                    # ECS, Physics, Identity, Scene, Serialization, GpuSmokeTest
└── assets/                         # Shaders ve sahne ikili dosyaları (.astral)
```

---

## Yeni Bir Oyun Projesi Nasıl Oluşturulur?

AstralEngine üzerinde yeni bir oyun projesi eklemek yalnızca birkaç adım sürer:

### 1. Şablonu Kopyalayın
`Projects/EmptyGameTemplate/` klasörünü `Projects/YeniOyunum/` olarak kopyalayın.

### 2. CMakeLists.txt Dosyasını Düzenleyin
`Projects/YeniOyunum/CMakeLists.txt`:
```cmake
add_executable(YeniOyunum
    src/main.cpp
)

target_link_libraries(YeniOyunum PRIVATE AstralEngine)

set_target_properties(YeniOyunum PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
```

### 3. Oyun Kodunuzu Yazın
`Projects/YeniOyunum/src/main.cpp`:
```cpp
#include "AstralEngine.h"
#include <iostream>

class YeniOyunApp : public Astral::Application {
public:
    YeniOyunApp()
        : Astral::Application() {
        std::cout << "Yeni Oyun baslatildi!\n";
        // Oyun mantığı sistemleri ISubsystem üzerinden PushSystem ile eklenir:
        // PushSystem<OyuncuKontrolSistemi>();
    }
};

namespace Astral {
Application* CreateApplication() {
    return new YeniOyunApp();
}
}

// Motor ana giriş noktasını bağlar
#include "Astral/EntryPoint.hpp"
```

### 4. Kök CMakeLists.txt'ye Ekleyin
Kök `CMakeLists.txt` dosyasının alt kısmına projenizi ekleyin:
```cmake
add_subdirectory(Projects/YeniOyunum)
```

Artık `cmake --build build-release` çalıştırdığınızda projeniz bağımsız bir `.exe` olarak derlenecektir!

---

## Derleme ve Çalıştırma

### Gereksinimler
- **Vulkan SDK**: 1.3 veya 1.4 (Vulkan 1.4 önerilir)
- **C++20 Uyumlu Derleyici**: MinGW-w64 GCC 13+ veya MSVC v143+
- **CMake**: 3.20+ ve **Ninja**

### Derleme Seçenekleri (CMake Flags)
| Seçenek | Varsayılan | Açıklama |
|---|---|---|
| `ASTRAL_BUILD_TESTS` | `ON` | Regresyon test paketini (`EngineTests`) derler ve CTest'e kaydeder. |
| `ASTRAL_BUILD_EDITOR` | `ON` | Görsel geliştirme ortamını (`AstralEditor`, ImGui ve ImGuizmo) derler. |
| `ASTRAL_BUILD_PROJECTS` | `ON` | Referans oyun projelerini (`Sandbox`, `EmptyGameTemplate`) derler. |

> [!TIP]
> CI ortamlarında veya hafif test yapılarında editör gereksinimi yoksa `-DASTRAL_BUILD_EDITOR=OFF` verilerek ImGui ve ImGuizmo derleme adımları tamamen atlanabilir.

### Temiz Derleme (Release ve Debug)

#### 1. CMake Presets ile (Önerilen)
```powershell
# Release yapılandırması ve derlemesi:
cmake --preset mingw-release
cmake --build --preset mingw-release

# Debug yapılandırması ve derlemesi:
cmake --preset mingw-debug
cmake --build --preset mingw-debug
```

#### 2. Standart Komut Satırı ile
```powershell
# Release:
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release

# Debug:
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug
```

---

## Otomatik Test ve CTest Doğrulaması

AstralEngine test altyapısı, Data-Oriented Design (DOD) mimarisiyle tasarlanmıştır.

### CTest ile Test Koşusu
```powershell
# 1. CI-Dostu Headless CPU Testleri (Fiziksel GPU ve pencere gerektirmez, <1 sn):
ctest --test-dir build-release -L CPU --output-on-failure

# Veya test preset kullanarak:
ctest --preset test-cpu

# 2. Donanım GPU Compute Smoke Testi (Vulkan 1.4 destekli fiziksel GPU gerektirir):
ctest --test-dir build-release -L GPU --output-on-failure

# Veya GPU preset kullanarak:
ctest --preset test-gpu

# 3. Tüm Testler (CPU + GPU):
ctest --test-dir build-release --output-on-failure
```

### Bağımsız Test Yürütülebilir Dosyası (`EngineTests.exe`)
```powershell
# Tüm headless CPU testlerini çalıştırır:
.\build-release\EngineTests.exe

# Yalnızca belirli bir alt-sistemi test eder:
.\build-release\EngineTests.exe --ecs
.\build-release\EngineTests.exe --physics
.\build-release\EngineTests.exe --identity
.\build-release\EngineTests.exe --scene
.\build-release\EngineTests.exe --serialization
.\build-release\EngineTests.exe --brickgrid
.\build-release\EngineTests.exe --command
.\build-release\EngineTests.exe --eventbus
.\build-release\EngineTests.exe --actionmap
.\build-release\EngineTests.exe --jobs
.\build-release\EngineTests.exe --taskgraph
.\build-release\EngineTests.exe --project
.\build-release\EngineTests.exe --loop
.\build-release\EngineTests.exe --boundaries
.\build-release\EngineTests.exe --gameplay

# Vulkan 1.4 GPU & Compute donanım testi dahil tüm testler:
.\build-release\EngineTests.exe --all
```

---

## Uygulamaları Çalıştırma

1. **Görsel Editör (`AstralEditor.exe`)**:
   ```powershell
   # Dear ImGui Docking, 3D Raymarched Viewport, Sahne Hiyerarşisi, F5 Play/Stop ve F6 Pause:
   .\build-release\AstralEditor.exe
   ```

2. **Referans Çalışma Zamanı Oyunu (`Sandbox.exe`)**:
   ```powershell
   # SDF Bulmaca Referans Oyunu (WASD hareket, 'E' CSG kapı oyma, 'R' sıfırlama):
   .\build-release\Sandbox.exe

   # Klasik demo sahnesi:
   .\build-release\Sandbox.exe --demo
   ```

3. **Minimal Oyun Şablonu (`EmptyGameTemplate.exe`)**:
   ```powershell
   .\build-release\EmptyGameTemplate.exe --frames 10
   ```

---

## Lisans

MIT
