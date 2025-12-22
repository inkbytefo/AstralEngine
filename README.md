# Astral Engine

Modern, modular C++20 oyun motoru. Frostbite'tan ilham alan subsystem mimarisi üzerine kurulu, veri odaklı tasarım prensipleri ile geliştirilmiştir.

## 🌟 Özellikler

### 🎯 Mevcut Özellikler (v0.1.0-alpha)
- ✅ Modüler Subsystem Mimarisi
- ✅ Merkezi Logging Sistemi
- ✅ Event-Driven İletişim
- ✅ Bellek Yönetim Sistemi
- ✅ Platform Soyutlama Katmanı (Placeholder)
- ✅ Asset Management Sistemi
- ✅ ECS (Entity Component System) Temel Yapısı

### 🚧 Geliştirme Aşamasında
- 🔄 SDL3 Entegrasyonu
- 🔄 Vulkan Renderer
- 🔄 Fizik Sistemi
- 🔄 Audio Sistemi
- 🔄 Scripting Desteği

### 🎮 Planlanan Özellikler
- 📋 Scene Management
- 📋 Animation System
- 📋 AI Framework
- 📋 Networking
- 📋 Asset Pipeline
- 📋 Editor Interface

## 🏗️ Mimari

Astral Engine, **subsystem-based modular architecture** kullanır:

```
Engine (Çekirdek Orkestratör)
├── PlatformSubsystem (Pencere, Input)
├── AssetSubsystem (Varlık Yönetimi)
├── ECSSubsystem (Oyun Mantığı)
├── RenderSubsystem (Grafik)
├── PhysicsSubsystem (Fizik)
└── AudioSubsystem (Ses)
```

### Temel Prensipler
- **Modülerlik**: Her subsystem bağımsız ve değiştirilebilir
- **Gevşek Bağlılık**: Event system ile sistemler arası iletişim
- **Veri Odaklı**: ECS pattern ile performans odaklı tasarım
- **Thread Safety**: Çoklu iş parçacığı desteği

## 🛠️ Gereksinimler

### Minimum Sistem Gereksinimleri
- **C++ Compiler**: C++20 desteği (GCC 10+, Clang 10+, MSVC 2019+)
- **CMake**: 3.20 veya üzeri
- **RAM**: 4GB (geliştirme için 8GB önerili)
- **Disk**: 500MB

### Bağımlılıklar
- **GLM**: Matematik kütüphanesi (header-only)
- **SDL3**: Platform katmanı (gelecek sürümde)
- **Vulkan**: Grafik API (gelecek sürümde)

## 📦 Kurulum

### 1. Repository'yi Klonlayın
```bash
git clone https://github.com/yourusername/AstralEngine.git
cd AstralEngine
```

### 2. Bağımlılıkları İndirin
```bash
# GLM'i External klasörüne yerleştirin
cd External
git clone https://github.com/g-truc/glm.git
cd ..
```

### 3. Build Edilin
```bash
# Build klasörü oluşturun
mkdir Build
cd Build

# CMake ile configure edin
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build edin
cmake --build . --config Debug

# Çalıştırın
./bin/AstralEngine
```

### Windows (Visual Studio)
```cmd
mkdir Build
cd Build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Debug
```

## 📁 Proje Yapısı

```
AstralEngine/
├── Assets/                 # Oyun varlıkları
│   ├── Models/
│   ├── Shaders/
│   └── Textures/
├── Build/                  # Build çıktıları
├── External/               # Dış kütüphaneler
│   ├── glm/
│   ├── SDL3/
│   └── vulkan/
├── Source/                 # Motor kaynak kodları
│   ├── Core/              # Çekirdek sistemler
│   │   ├── Engine.h/cpp
│   │   ├── ISubsystem.h
│   │   ├── Logger.h/cpp
│   │   └── MemoryManager.h/cpp
│   ├── Events/            # Event sistemi
│   │   ├── Event.h
│   │   └── EventManager.h/cpp
│   ├── ECS/               # Entity Component System
│   │   └── Components.h
│   ├── Subsystems/        # Alt sistemler
│   │   ├── Platform/      # Platform katmanı
│   │   ├── Asset/         # Asset yönetimi
│   │   ├── ECS/           # ECS subsystem
│   │   └── Renderer/      # Render sistemi
│   └── main.cpp           # Ana giriş noktası
├── CMakeLists.txt         # Build sistemi
├── README.md             # Bu dosya
└── MIMARI.md            # Detaylı mimari dokümantasyonu
```

## 🎯 Kullanım Örneği

### Basit Bir Sahne Oluşturma

```cpp
#include "Core/Engine.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "ECS/Components.h"

using namespace AstralEngine;

int main() {
    Engine engine;
    
    // Subsystem'leri kaydet
    engine.RegisterSubsystem<PlatformSubsystem>();
    
    // Engine'i çalıştır
    engine.Run();
    
    return 0;
}
```

### Event Sistem Kullanımı

```cpp
// Event dinleyici kaydet
EventManager& eventMgr = EventManager::GetInstance();
auto handlerID = eventMgr.Subscribe<WindowResizeEvent>(
    [](Event& event) {
        auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
        Logger::Info("Game", "Window resized to {}x{}", 
                    resizeEvent.GetWidth(), resizeEvent.GetHeight());
        return true; // Event handled
    });

// Event gönder
eventMgr.PublishEvent<WindowResizeEvent>(1920, 1080);
```

## 🔧 Geliştirme

### Debug Modu
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### Release Modu
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Logging Seviyeleri
```cpp
Logger::SetLogLevel(Logger::LogLevel::Debug); // Tüm loglar
Logger::SetLogLevel(Logger::LogLevel::Info);  // Sadece önemli bilgiler
Logger::SetLogLevel(Logger::LogLevel::Error); // Sadece hatalar
```

## 📋 Roadmap

### v0.2.0 (Completed)
- [x] SDL3 Entegrasyonu
- [x] Temel Vulkan Renderer
- [x] Window Management

### v0.3.0 (Completed)
- [x] Model Loading (OBJ/glTF)
- [x] Texture System
- [x] Basic Shader System
- [x] ImGui Editor Interface

### v0.4.0 (Completed)
- [x] ECS Subsystem (EnTT Integration)
- [x] Scene Management (Entity Creation, Registry Wrapper)
- [x] Scene Serialization (JSON) (Save/Load Scene State)
- [x] Transform Hierarchy (Parent-Child, World Transform)
- [x] Raycasting Selection System (AABB Intersection)

### v0.5.0 (Upcoming)
- [ ] Physics Integration (Jolt)
- [ ] Audio System
- [ ] Performance Profiler
- [ ] Asset Pipeline Improvements

### v1.0.0 (Target)
- [ ] Full Editor
- [ ] Scripting Support
- [ ] Networking
- [ ] Animation System

## 🤝 Katkıda Bulunma

1. Repository'yi fork edin
2. Feature branch oluşturun (`git checkout -b feature/AmazingFeature`)
3. Değişikliklerinizi commit edin (`git commit -m 'Add some AmazingFeature'`)
4. Branch'i push edin (`git push origin feature/AmazingFeature`)
5. Pull Request oluşturun

### Code Style
- C++20 standard
- CamelCase for classes/functions
- snake_case for variables
- m_ prefix for member variables
- Comprehensive commenting

## 📝 Lisans

Bu proje MIT Lisansı altında lisanslanmıştır. Detaylar için [LICENSE](LICENSE) dosyasına bakın.

## 📞 İletişim

- **Email**: your.email@example.com
- **Discord**: YourUsername#1234
- **GitHub Issues**: Bug raporları ve öneriler için

## 🙏 Teşekkürler

- **GLM Team**: Mükemmel math library için
- **SDL Team**: Cross-platform layer için
- **Khronos Group**: Vulkan API için
- **Game Engine Architecture**: Inspiring design patterns

---

**Not**: Bu engine aktif geliştirme aşamasındadır. Production kullanımı için henüz hazır değildir.
