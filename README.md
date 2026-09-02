# AstralEngine

Modern C++20 ile yazılmış, modüler, yüksek performanslı genel amaçlı oyun motoru çekirdeği ve Vulkan 1.4 tabanlı Signed Distance Field (SDF) render alt-sistemi.

Detaylı teknik analiz ve mimari yol haritası için: **[RENDERER_ARCHITECTURE.md](RENDERER_ARCHITECTURE.md)**

---

## Temel Özellikler

- **C++20 Standartları**: Konseptler, modern RAII ve tip güvenliği.
- **Cache-Friendly ECS**: Hızlı ardışık bellek erişimi sunan sparse-set tabanlı bileşen havuzları (`Astral::Registry`).
- **Vulkan 1.4 Render Alt-Sistemi (`Astral::VulkanContext`)**:
  - `VK_KHR_dynamic_rendering` ve `VK_KHR_synchronization2` modern çekirdek özellikleri.
  - `vulkan.hpp` dinamik dispatcher (`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC`).
  - Gelişmiş doğrulama katmanı ve debug messenger (`VK_LAYER_KHRONOS_validation` + `VkDebugUtilsMessengerEXT`).
- **Pencere & Girdi Yönetimi (`Astral::Window`)**: GLFW 3.3.8 tabanlı yüksek performanslı pencereleme.
- **Otomasyon & Bağımsızlık**: `FetchContent` ile GLFW, GLM ve harici kütüphanelerin CMake seviyesinde otomatik yönetimi.

---

## Klasör Yapısı

```
AstralEngine/
├── CMakeLists.txt              # Ana derleme yapılandırması (C++20, FetchContent, Vulkan)
├── CMakePresets.json           # MinGW ve MSVC hazır derleme presetleri
├── RENDERER_ARCHITECTURE.md    # Kapsamlı SDF render mimarisi ve teknik analiz dokümanı
├── include/Astral/             # Dışarı açık başlık dosyaları
│   ├── Core/
│   │   ├── Application.hpp     # Ana motor yaşam döngüsü
│   │   ├── Registry.hpp        # Tip silinmiş (type-erased) SparseSet ECS motoru
│   │   └── Window.hpp          # GLFW pencere yöneticisi
│   └── Renderer/
│       └── VulkanContext.hpp   # Vulkan 1.4 context, validation layer ve debug messenger
├── src/                        # Kaynak kodları
│   ├── Core/
│   │   ├── Application.cpp
│   │   └── Window.cpp
│   ├── Renderer/
│   │   └── VulkanContext.cpp
│   └── main.cpp                # Sandbox test ve doğrulama uygulaması
├── renderingexample/           # [Referans Repo] inkbytefo/SDFRENDEREXAMPLE (.gitignore'da)
└── build/                      # Derleme çıktıları
```

---

## Önkoşullar

- **Vulkan SDK**: 1.3 veya 1.4 (Vulkan 1.4 ve `VK_LAYER_KHRONOS_validation` önerilir)
- **C++20 Uyumlu Derleyici**:
  - MinGW-w64 GCC 13+ (ör. GCC 15.2)
  - veya Visual Studio 2022 (MSVC v143+)
- **CMake**: 3.20 veya üzeri
- **Ninja**: (MinGW presetleri için önerilir)

---

## Derleme ve Çalıştırma

### 1. Windows - MinGW + Ninja (CMake Presets)

```powershell
# Yapılandırma ve derleme (Debug)
cmake --preset mingw-debug
cmake --build --preset mingw-debug

# Doğrulama testi çalıştırma (10 kare test modu)
.\build\Sandbox.exe --test

# İnteraktif pencere modunda çalıştırma
.\build\Sandbox.exe
```

### 2. Windows - Visual Studio 2022 (MSVC)

```powershell
# Yapılandırma ve derleme
cmake --preset msvc-debug
cmake --build --preset msvc-debug --config Debug

# Çalıştırma
.\build-msvc\Debug\Sandbox.exe --test
```

### 3. Komut Satırı Argümanları

- `--test` veya `--test-only`: 10 kare çalıştıktan sonra Vulkan kaynaklarını temizleyerek otomatik kapanır.
- `--bench`: Donanımsal GPU timestamp profilleyicisini aktif eder.
- `--bench-frames <N>`: Belirtilen $N$ kare sayısı kadar test çalıştırıp çıkar.
- `--bench-out <dosya.csv>`: Kıyaslama verilerini CSV ve JSON formatında diske kaydeder.
- `--width <W> --height <H>`: Render ve pencere çözünürlüğünü dinamik ayarlar.
- *Argümansız*: Kullanıcı pencereyi kapatana kadar ana olay döngüsünü işletir.

---

## Benchmark Otomasyonu (PR-3)

Farklı çözünürlük matrislerinde ardışık test koşturmak ve konsolide raporlar üretmek için:

```powershell
# 720p ve 1080p presetlerinde 100'er kare otomatik benchmark ve HTML raporu üretimi:
python tools/run_bench.py --frames 100 --presets 720p 1080p

# Çıktı dosyaları:
# - artifacts/bench/matrix_summary.csv
# - artifacts/bench/matrix_summary.json
# - artifacts/bench/report.html (Tarayıcıda açılabilir görsel SVG raporu)
```

---

## Lisans

MIT