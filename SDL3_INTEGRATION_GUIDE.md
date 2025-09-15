# SDL3 Entegrasyon Rehberi - Astral Engine

**Tarih:** 10 Eylül 2025  
**SDL3 Son Sürüm:** 3.2.22 (2 Eylül 2025)  
**Engine Hedef:** Astral Engine v0.1.0-alpha

## 📋 İçindekiler

1. [SDL3 Hakkında Genel Bilgiler](#sdl3-hakkında-genel-bilgiler)
2. [Sistem Gereksinimleri](#sistem-gereksinimleri)
3. [SDL3 İndirme ve Kurulum](#sdl3-indirme-ve-kurulum)
4. [CMake Entegrasyonu](#cmake-entegrasyonu)
5. [Kod Entegrasyonu](#kod-entegrasyonu)
6. [Migration Guide](#migration-guide)
7. [Test ve Doğrulama](#test-ve-doğrulama)
8. [Troubleshooting](#troubleshooting)

---

## 🎯 SDL3 Hakkında Genel Bilgiler

### SDL3 Yenilikler ve Önemli Değişiklikler

**SDL3 Resmi Çıkış:** 21 Ocak 2025 (v3.2.0)  
**Son Güncel Sürüm:** v3.2.22 (2 Eylül 2025)

### 🌟 SDL3'ün Temel Yenilikleri

1. **GPU API**: Modern 3D rendering ve GPU compute desteği
2. **Improved Documentation**: Kapsamlı API dokümantasyonu
3. **Main Callbacks**: main() yerine callback sistemi (opsiyonel)
4. **Properties API**: Hızlı, esnek name/value sözlükleri
5. **Better Audio**: Logical audio devices ve audio streams
6. **Dialog API**: Sistem dosya diyalogları
7. **Camera API**: Webcam desteği
8. **Process API**: Child process yönetimi
9. **Colorspace Support**: Çoklu renk uzayı desteği

### ⚠️ Breaking Changes

- **API İsimlendirme**: Tüm fonksiyonlar tutarlı isimlendirmeye geçti
- **Header Dosyaları**: Tek SDL3 header'ı (SDL.h yerine)
- **Initialization**: SDL_Init değişiklikleri
- **Event System**: Yeni event yapısı

---

## 🛠️ Sistem Gereksinimleri

### Minimum Gereksinimler

- **CMake:** 3.16+ (3.20+ önerilen)
- **C++ Compiler:** 
  - MSVC 2019+ (Windows)
  - GCC 9+ (Linux)
  - Clang 10+ (macOS/Linux)
- **Platform:**
  - Windows 10/11
  - Ubuntu 20.04+/Debian 11+
  - macOS 10.15+

### Build Dependencies (Linux)

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake git \
    libx11-dev libxext-dev libxrandr-dev \
    libxinerama-dev libxi-dev libxss-dev \
    libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdrm-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols \
    libasound2-dev libpulse-dev libjack-dev \
    libpipewire-0.3-dev libsndio-dev

# Fedora/RHEL
sudo dnf install cmake gcc-c++ git \
    libX11-devel libXext-devel libXrandr-devel \
    libXinerama-devel libXi-devel libXss-devel \
    mesa-libGL-devel mesa-libGLES-devel mesa-libEGL-devel \
    libdrm-devel libxkbcommon-devel \
    wayland-devel wayland-protocols-devel \
    alsa-lib-devel pulseaudio-libs-devel jack-audio-connection-kit-devel
```

---

## 📦 SDL3 İndirme ve Kurulum

### Seçenek 1: Precompiled Binaries (Windows için Önerilen)

```bash
# GitHub'dan en son sürümü indirin
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.22/SDL3-devel-3.2.22-mingw.tar.gz

# Windows için Visual Studio binaries
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.22/SDL3-3.2.22-win32-x64.zip
```

### Seçenek 2: Source'dan Build (Tüm Platformlar)

#### 1. Repository'yi Clone Edin

```bash
cd External
git clone https://github.com/libsdl-org/SDL.git SDL3
cd SDL3
git checkout release-3.2.22  # Son stable sürüm
```

#### 2. SDL3'ü Build Edin

**Windows (Visual Studio):**
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_INSTALL_PREFIX=C:/SDL3
cmake --build . --config Release
cmake --install . --config Release
```

**Linux/macOS:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

#### 3. AstralEngine için Optimize Build

```bash
# Performance ve shared library için
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL_SHARED=ON \
  -DSDL_STATIC=ON \
  -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=../../../External/SDL3-install
```

---

## 🔧 CMake Entegrasyonu

### 1. Mevcut CMakeLists.txt Güncellemeleri

AstralEngine'deki `CMakeLists.txt` dosyasını güncelleyin:

```cmake
# SDL3 desteği ekleyin
set(SDL3_ROOT ${EXTERNAL_DIR}/SDL3-install)

# SDL3'ü bulun
find_package(SDL3 QUIET)

if(SDL3_FOUND)
    message(STATUS "SDL3 found: ${SDL3_VERSION}")
    set(ASTRAL_HAS_SDL3 TRUE)
    
    # SDL3 target'ını link edin
    target_link_libraries(AstralEngine PRIVATE SDL3::SDL3)
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_USE_SDL3)
    
else()
    message(WARNING "SDL3 not found - using placeholder window implementation")
    set(ASTRAL_HAS_SDL3 FALSE)
    
    # Fallback: system SDL3 installation deneme
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 QUIET sdl3)
        if(SDL3_FOUND)
            target_link_libraries(AstralEngine PRIVATE ${SDL3_LIBRARIES})
            target_include_directories(AstralEngine PRIVATE ${SDL3_INCLUDE_DIRS})
            target_compile_definitions(AstralEngine PRIVATE ASTRAL_USE_SDL3)
            set(ASTRAL_HAS_SDL3 TRUE)
        endif()
    endif()
endif()

# SDL3 konfigürasyon özeti
if(ASTRAL_HAS_SDL3)
    message(STATUS "=== SDL3 Configuration ===")
    message(STATUS "SDL3 Version: ${SDL3_VERSION}")
    message(STATUS "SDL3 Found: YES")
else()
    message(STATUS "=== SDL3 Configuration ===")
    message(STATUS "SDL3 Found: NO")
    message(STATUS "Using placeholder implementation")
endif()
```

### 2. Alternative: Vendored SDL3

Eğer SDL3'ü projeye dahil etmek istiyorsanız:

```cmake
# SDL3'ü submodule veya vendored olarak kullanma
option(ASTRAL_USE_VENDORED_SDL3 "Use vendored SDL3" ON)

if(ASTRAL_USE_VENDORED_SDL3 AND EXISTS "${CMAKE_SOURCE_DIR}/External/SDL3/CMakeLists.txt")
    # SDL3'ü direct olarak build et
    message(STATUS "Using vendored SDL3")
    
    # SDL3 opsiyonlarını ayarla
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    
    # SDL3'ü projeye dahil et
    add_subdirectory(External/SDL3 EXCLUDE_FROM_ALL)
    
    # Link et
    target_link_libraries(AstralEngine PRIVATE SDL3::SDL3)
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_USE_SDL3)
    
    set(ASTRAL_HAS_SDL3 TRUE)
endif()
```

---

## 💻 Kod Entegrasyonu

### 1. Window.h Güncellemeleri

`Source/Subsystems/Platform/Window.h` dosyasını güncelleyin:

```cpp
#pragma once

#include <string>

// SDL3 conditional includes
#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
#endif

namespace AstralEngine {

class Window {
public:
    Window();
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Pencere yaşam döngüsü
    bool Initialize(const std::string& title, int width, int height);
    void Shutdown();

    // Pencere yönetimi
    void PollEvents();
    void SwapBuffers();
    bool ShouldClose() const { return m_shouldClose; }
    void Close() { m_shouldClose = true; }

    // Pencere özellikleri
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    float GetAspectRatio() const;
    
    const std::string& GetTitle() const { return m_title; }
    void SetTitle(const std::string& title);
    
    void SetSize(int width, int height);
    void SetVSync(bool enabled);

    // Platform-specific getters
#ifdef ASTRAL_USE_SDL3
    SDL_Window* GetSDLWindow() const { return m_sdlWindow; }
#endif
    void* GetNativeHandle() const;

private:
    void HandleWindowEvent(const void* event);

#ifdef ASTRAL_USE_SDL3
    SDL_Window* m_sdlWindow = nullptr;
#else
    void* m_platformWindow = nullptr; // Placeholder
#endif

    std::string m_title;
    int m_width = 0;
    int m_height = 0;
    bool m_shouldClose = false;
    bool m_vsync = true;
    bool m_initialized = false;
};

} // namespace AstralEngine
```

### 2. Window.cpp SDL3 Implementation

`Source/Subsystems/Platform/Window.cpp` dosyasını güncelleyin:

```cpp
#include "Window.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"

namespace AstralEngine {

Window::Window() {
    Logger::Debug("Window", "Window instance created");
}

Window::~Window() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("Window", "Window instance destroyed");
}

bool Window::Initialize(const std::string& title, int width, int height) {
    if (m_initialized) {
        Logger::Warning("Window", "Window already initialized");
        return true;
    }
    
    Logger::Info("Window", "Initializing window: '{}' ({}x{})", title, width, height);
    
    m_title = title;
    m_width = width;
    m_height = height;
    
#ifdef ASTRAL_USE_SDL3
    // SDL3 initialization
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logger::Error("Window", "SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    
    // SDL3'te window oluşturma
    m_sdlWindow = SDL_CreateWindow(
        title.c_str(), 
        width, 
        height, 
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    
    if (!m_sdlWindow) {
        Logger::Error("Window", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    Logger::Info("Window", "SDL3 window created successfully");
    
#else
    // Placeholder implementation
    Logger::Warning("Window", "SDL3 not available - using placeholder implementation");
    m_platformWindow = nullptr;
#endif
    
    m_initialized = true;
    Logger::Info("Window", "Window initialized successfully");
    
    return true;
}

void Window::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("Window", "Shutting down window");
    
#ifdef ASTRAL_USE_SDL3
    if (m_sdlWindow) {
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
    }
    SDL_Quit();
#endif
    
    m_initialized = false;
    Logger::Info("Window", "Window shutdown complete");
}

void Window::PollEvents() {
    if (!m_initialized) {
        return;
    }
    
#ifdef ASTRAL_USE_SDL3
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        HandleWindowEvent(&event);
    }
#endif
}

void Window::HandleWindowEvent(const void* event) {
#ifdef ASTRAL_USE_SDL3
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    
    auto& eventManager = EventManager::GetInstance();
    
    switch (sdlEvent->type) {
        case SDL_EVENT_QUIT:
            m_shouldClose = true;
            eventManager.PublishEvent<WindowCloseEvent>();
            break;
            
        case SDL_EVENT_WINDOW_RESIZED:
            m_width = sdlEvent->window.data1;
            m_height = sdlEvent->window.data2;
            eventManager.PublishEvent<WindowResizeEvent>(m_width, m_height);
            Logger::Debug("Window", "Window resized to {}x{}", m_width, m_height);
            break;
            
        case SDL_EVENT_KEY_DOWN:
            eventManager.PublishEvent<KeyPressedEvent>(
                sdlEvent->key.key, 
                sdlEvent->key.repeat
            );
            break;
            
        case SDL_EVENT_KEY_UP:
            eventManager.PublishEvent<KeyReleasedEvent>(sdlEvent->key.key);
            break;
            
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            eventManager.PublishEvent<MouseButtonPressedEvent>(sdlEvent->button.button);
            break;
            
        case SDL_EVENT_MOUSE_BUTTON_UP:
            eventManager.PublishEvent<MouseButtonReleasedEvent>(sdlEvent->button.button);
            break;
            
        case SDL_EVENT_MOUSE_MOTION:
            eventManager.PublishEvent<MouseMovedEvent>(
                (int)sdlEvent->motion.x, 
                (int)sdlEvent->motion.y
            );
            break;
    }
#endif
}

float Window::GetAspectRatio() const {
    if (m_height == 0) {
        return 1.0f;
    }
    return static_cast<float>(m_width) / static_cast<float>(m_height);
}

void Window::SetTitle(const std::string& title) {
    m_title = title;
    
#ifdef ASTRAL_USE_SDL3
    if (m_sdlWindow) {
        SDL_SetWindowTitle(m_sdlWindow, title.c_str());
    }
#endif
    
    Logger::Debug("Window", "Window title set to: '{}'", title);
}

void Window::SetSize(int width, int height) {
    m_width = width;
    m_height = height;
    
#ifdef ASTRAL_USE_SDL3
    if (m_sdlWindow) {
        SDL_SetWindowSize(m_sdlWindow, width, height);
    }
#endif
    
    Logger::Debug("Window", "Window size set to: {}x{}", width, height);
}

void* Window::GetNativeHandle() const {
#ifdef ASTRAL_USE_SDL3
    if (m_sdlWindow) {
        SDL_PropertiesID props = SDL_GetWindowProperties(m_sdlWindow);
        
        // Platform specific handle alma
        #if defined(_WIN32)
            return (void*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        #elif defined(__linux__)
            // X11 veya Wayland handle
            void* x11_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_WINDOW_POINTER, nullptr);
            if (x11_window) return x11_window;
            
            void* wayland_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            return wayland_surface;
        #elif defined(__APPLE__)
            return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        #endif
    }
#endif
    return nullptr;
}

} // namespace AstralEngine
```

### 3. InputManager SDL3 Integration

`InputManager.cpp`'yi de benzer şekilde güncelleyin:

```cpp
// SDL3 keycodes için mapping
#ifdef ASTRAL_USE_SDL3
KeyCode SDLKeyToAstralKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_A: return KeyCode::A;
        case SDLK_B: return KeyCode::B;
        // ... diğer tuşlar
        case SDLK_ESCAPE: return KeyCode::Escape;
        case SDLK_SPACE: return KeyCode::Space;
        default: return KeyCode::Unknown;
    }
}

MouseButton SDLButtonToAstralButton(uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return MouseButton::Left;
    }
}
#endif
```

---

## 🔄 Migration Guide

### SDL2'den SDL3'e Ana Değişiklikler

1. **Header Include:**
   ```cpp
   // SDL2
   #include <SDL2/SDL.h>
   
   // SDL3
   #include <SDL3/SDL.h>
   ```

2. **Initialization:**
   ```cpp
   // SDL2
   SDL_Init(SDL_INIT_VIDEO);
   
   // SDL3
   SDL_Init(SDL_INIT_VIDEO); // Aynı ama return değeri bool
   ```

3. **Window Creation:**
   ```cpp
   // SDL2
   SDL_Window* window = SDL_CreateWindow("Title", 
       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
       width, height, SDL_WINDOW_VULKAN);
   
   // SDL3
   SDL_Window* window = SDL_CreateWindow("Title", width, height, SDL_WINDOW_VULKAN);
   ```

4. **Event Handling:**
   ```cpp
   // SDL2
   case SDL_QUIT:
   case SDL_WINDOWEVENT:
   case SDL_KEYDOWN:
   
   // SDL3
   case SDL_EVENT_QUIT:
   case SDL_EVENT_WINDOW_RESIZED:
   case SDL_EVENT_KEY_DOWN:
   ```

---

## 🧪 Test ve Doğrulama

### 1. Basit Test Uygulaması

Test için `test_sdl3.cpp` oluşturun:

```cpp
#include "Source/Core/Engine.h"
#include "Source/Subsystems/Platform/PlatformSubsystem.h"

int main() {
    using namespace AstralEngine;
    
    try {
        Engine engine;
        engine.RegisterSubsystem<PlatformSubsystem>();
        
        Logger::Info("Test", "SDL3 integration test starting...");
        
        // 5 saniye çalıştır
        auto start = std::chrono::steady_clock::now();
        while (engine.IsRunning()) {
            engine.Update();
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
            if (elapsed.count() >= 5) {
                engine.RequestShutdown();
            }
        }
        
        Logger::Info("Test", "SDL3 integration test completed successfully!");
        return 0;
        
    } catch (const std::exception& e) {
        Logger::Error("Test", "SDL3 test failed: {}", e.what());
        return -1;
    }
}
```

### 2. Build ve Test

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Test çalıştır
./AstralEngine

# Log çıktısında arayın:
# [INFO] [PlatformSubsystem] SDL3 window created successfully
# [INFO] [Test] SDL3 integration test completed successfully!
```

---

## 🛠️ Troubleshooting

### Yaygın Problemler ve Çözümleri

#### 1. "SDL3 not found" hatası

**Çözüm:**
```bash
# SDL3 yolunu kontrol edin
export SDL3_ROOT=/path/to/sdl3/install
# veya
cmake .. -DSDL3_ROOT=/path/to/sdl3/install
```

#### 2. "undefined reference to SDL_*" linker hatası

**Çözüm:**
```cmake
# CMakeLists.txt'ye ekleyin
if(UNIX AND NOT APPLE)
    target_link_libraries(AstralEngine PRIVATE dl pthread)
elseif(WIN32)
    target_link_libraries(AstralEngine PRIVATE winmm imm32 version setupapi)
endif()
```

#### 3. Windows'ta DLL bulunamıyor

**Çözüm:**
```cmake
# SDL3 DLL'ini executable dizinine kopyala
if(WIN32 AND ASTRAL_HAS_SDL3)
    add_custom_command(TARGET AstralEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:AstralEngine>
    )
endif()
```

#### 4. Linux'ta missing libraries

**Çözüm:**
```bash
# Eksik development package'leri yükleyin
sudo apt-get install libwayland-dev libx11-dev libxrandr-dev
```

### Debug İpuçları

1. **Verbose CMake Output:**
   ```bash
   cmake .. --debug-find
   ```

2. **SDL3 Version Kontrolü:**
   ```cpp
   #ifdef ASTRAL_USE_SDL3
   Logger::Info("SDL3", "Version: {}.{}.{}", 
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
   #endif
   ```

3. **Platform Kontrolleri:**
   ```cpp
   Logger::Info("Platform", "OS: {}", SDL_GetPlatform());
   Logger::Info("Platform", "CPU Count: {}", SDL_GetCPUCount());
   ```

---

## ✅ Entegrasyon Kontrolü

SDL3 entegrasyonunun başarılı olduğunu doğrulamak için:

- [ ] CMake configuration geçiyor
- [ ] Build hatası yok
- [ ] Pencere açılıyor
- [ ] Event'ler çalışıyor
- [ ] Log mesajlarında "SDL3" görünüyor
- [ ] Window resize çalışıyor
- [ ] Keyboard/mouse input çalışıyor

## 📚 Yararlı Linkler

- **SDL3 Resmi Wiki:** https://wiki.libsdl.org/SDL3/
- **SDL3 GitHub:** https://github.com/libsdl-org/SDL
- **SDL3 Migration Guide:** https://wiki.libsdl.org/SDL3/README-migration
- **SDL3 CMake Guide:** https://wiki.libsdl.org/SDL3/README-cmake
- **SDL3 Examples:** https://github.com/libsdl-org/SDL/tree/main/examples

Bu rehber, Astral Engine'e SDL3 entegrasyonunu tam ve güvenli bir şekilde gerçekleştirmeniz için gerekli tüm adımları içermektedir. Herhangi bir problem yaşarsanız, troubleshooting bölümünü kontrol edin veya SDL3 community'sine danışın.
