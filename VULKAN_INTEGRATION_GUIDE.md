# Vulkan Entegrasyon Rehberi - Astral Engine

**Tarih:** 10 Eylül 2025  
**Vulkan SDK Son Sürüm:** 1.4.321.1 (18 Temmuz 2025)  
**Engine Hedef:** Astral Engine v0.1.0-alpha

## 📋 İçindekiler

1. [Vulkan SDK Hakkında Genel Bilgiler](#vulkan-sdk-hakkında-genel-bilgiler)
2. [Sistem Gereksinimleri](#sistem-gereksinimleri)
3. [Vulkan SDK İndirme ve Kurulum](#vulkan-sdk-indirme-ve-kurulum)
4. [CMake Entegrasyonu](#cmake-entegrasyonu)
5. [SDL3-Vulkan Entegrasyonu](#sdl3-vulkan-entegrasyonu)
6. [Kod Entegrasyonu](#kod-entegrasyonu)
7. [RenderSubsystem İmplementasyonu](#rendersubsystem-implementasyonu)
8. [Test ve Doğrulama](#test-ve-doğrulama)
9. [Troubleshooting](#troubleshooting)

---

## 🎯 Vulkan SDK Hakkında Genel Bilgiler

### Vulkan 1.4 Yenilikler ve Önemli Değişiklikler

**Vulkan 1.4 Çıkış:** 2025 başı  
**Son Güncel SDK:** v1.4.321.1 (18 Temmuz 2025)

### 🌟 Vulkan 1.4'ün Temel Yenilikleri

1. **Consolidated Extensions**: Önceki opsiyonel extension'lar artık core'da
2. **Enhanced Device Features**: Minimum hardware gereksinimleri artırıldı
3. **Improved Debugging**: Validation layers ve debugging araçları geliştirildi
4. **Better Memory Management**: Host memory allocation iyileştirmeleri
5. **Dynamic Rendering**: VK_KHR_dynamic_rendering core'a dahil edildi
6. **Synchronization 2**: VK_KHR_synchronization2 core'a dahil edildi

### 🔧 **SDK İçeriği**
- **Vulkan Loader**: Runtime library
- **Validation Layers**: Debug ve profiling
- **SPIR-V Tools**: Shader compilation araçları
- **glslc & DXC**: Shader compiler'ları
- **Vulkan Memory Allocator (VMA)**: Memory management helper
- **RenderDoc Integration**: Graphics debugging

---

## 🛠️ Sistem Gereksinimleri

### Minimum Gereksinimler

- **CMake:** 3.7+ (3.20+ önerilen)
- **C++ Compiler:** 
  - MSVC 2019+ (Windows)
  - GCC 10+ (Linux)
  - Clang 12+ (macOS/Linux)
- **Graphics Driver:**
  - NVIDIA: 460.89+ (Vulkan 1.4 support)
  - AMD: 21.4.1+ (Vulkan 1.4 support)
  - Intel: 27.20.100.9316+ (Vulkan 1.4 support)

### Platform Gereksinimleri

**Windows:**
- Windows 10 1903+ (build 18362)
- DirectX 12 compatible GPU

**Linux:**
- Ubuntu 20.04+/Debian 11+
- Mesa 21.0+ veya proprietary drivers
- Wayland/X11 support

**macOS:**
- macOS 10.15+ (Catalina)
- MoltenVK (Vulkan-over-Metal)

---

## 📦 Vulkan SDK İndirme ve Kurulum

### Windows İçin Kurulum

#### 1. SDK İndirme

```bash
# En son SDK'yı indirin
# https://vulkan.lunarg.com/sdk/home adresinden
wget https://sdk.lunarg.com/sdk/download/1.4.321.1/windows/vulkansdk-windows-X64-1.4.321.1.exe
```

#### 2. SDK Kurulumu

```cmd
# Administrator olarak çalıştırın
vulkansdk-windows-X64-1.4.321.1.exe

# Varsayılan lokasyon: C:\VulkanSDK\1.4.321.1\
# Environment variable otomatik ayarlanır: VULKAN_SDK
```

#### 3. Kurulum Doğrulaması

```cmd
# VULKAN_SDK environment variable kontrolü
echo %VULKAN_SDK%

# Vulkan versiyonu kontrolü
%VULKAN_SDK%\Bin\vulkaninfo.exe
```

### Linux İçin Kurulum

#### 1. SDK İndirme ve Kurulum

```bash
# SDK'yı indirin
wget https://sdk.lunarg.com/sdk/download/1.4.321.1/linux/vulkansdk-linux-x86_64-1.4.321.1.tar.xz

# Extract edin
tar -xf vulkansdk-linux-x86_64-1.4.321.1.tar.xz

# Taşıyın
sudo mv 1.4.321.1 /opt/vulkan-sdk/

# Environment setup
echo 'export VULKAN_SDK="/opt/vulkan-sdk/1.4.321.1/x86_64"' >> ~/.bashrc
echo 'export PATH="$VULKAN_SDK/bin:$PATH"' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH="$VULKAN_SDK/lib:$LD_LIBRARY_PATH"' >> ~/.bashrc
echo 'export VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layer.d"' >> ~/.bashrc

source ~/.bashrc
```

#### 2. Sistem Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install \
    vulkan-tools vulkan-validationlayers-dev \
    spirv-tools libvulkan-dev \
    libglfw3-dev libglm-dev \
    libxxf86vm-dev libxi-dev

# Fedora/RHEL
sudo dnf install \
    vulkan-tools vulkan-validation-layers-devel \
    spirv-tools-devel vulkan-loader-devel \
    glfw-devel glm-devel
```

### macOS İçin Kurulum

#### 1. SDK İndirme ve Kurulum

```bash
# SDK'yı indirin
wget https://sdk.lunarg.com/sdk/download/1.4.321.0/mac/vulkansdk-macos-1.4.321.0.zip

# Extract ve kur
unzip vulkansdk-macos-1.4.321.0.zip
sudo installer -pkg vulkansdk-macos-1.4.321.0.pkg -target /

# Environment setup
echo 'export VULKAN_SDK="/usr/local/share/vulkan/macOS"' >> ~/.zshrc
echo 'export PATH="$VULKAN_SDK/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

---

## 🔧 CMake Entegrasyonu

### 1. Mevcut CMakeLists.txt Güncellemeleri

AstralEngine'deki `CMakeLists.txt` dosyasını güncelleyin:

```cmake
# Vulkan desteği ekleyin
find_package(Vulkan REQUIRED COMPONENTS glslc)

if(Vulkan_FOUND)
    message(STATUS "Vulkan found: ${Vulkan_VERSION}")
    set(ASTRAL_HAS_VULKAN TRUE)
    
    # Vulkan library'sini link edin
    target_link_libraries(AstralEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_USE_VULKAN)
    
    # Vulkan headers
    target_link_libraries(AstralEngine PRIVATE Vulkan::Headers)
    
    # SPIR-V compiler (shader compilation için)
    if(Vulkan_glslc_FOUND)
        message(STATUS "GLSL compiler found: ${Vulkan_GLSLC_EXECUTABLE}")
        target_compile_definitions(AstralEngine PRIVATE ASTRAL_HAS_GLSLC)
    endif()
    
else()
    message(FATAL_ERROR "Vulkan not found! Please install Vulkan SDK")
endif()

# Platform-specific Vulkan libs
if(WIN32)
    # Windows'ta ek library'ler
    target_link_libraries(AstralEngine PRIVATE)
elseif(UNIX AND NOT APPLE)
    # Linux'ta ek library'ler
    target_link_libraries(AstralEngine PRIVATE dl pthread X11)
elseif(APPLE)
    # macOS'ta MoltenVK
    if(Vulkan_MoltenVK_FOUND)
        target_link_libraries(AstralEngine PRIVATE Vulkan::MoltenVK)
        message(STATUS "MoltenVK found and linked")
    endif()
endif()

# Vulkan konfigürasyon özeti
message(STATUS "=== Vulkan Configuration ===")
message(STATUS "Vulkan Version: ${Vulkan_VERSION}")
message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARIES}")
message(STATUS "Shader Compiler: ${Vulkan_GLSLC_EXECUTABLE}")
message(STATUS "=============================")
```

### 2. Shader Build System

Vulkan için shader compilation sistemi ekleyin:

```cmake
# Shader compilation function
function(compile_vulkan_shaders TARGET_NAME SHADER_DIR)
    if(NOT Vulkan_glslc_FOUND)
        message(WARNING "GLSLC not found, shaders won't be compiled")
        return()
    endif()
    
    file(GLOB_RECURSE SHADER_SOURCES 
         "${SHADER_DIR}/*.vert" 
         "${SHADER_DIR}/*.frag" 
         "${SHADER_DIR}/*.comp"
         "${SHADER_DIR}/*.geom"
         "${SHADER_DIR}/*.tesc"
         "${SHADER_DIR}/*.tese")
    
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
    
    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
        
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${SHADER_NAME}"
        )
        
        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
    endforeach()
    
    add_custom_target(${TARGET_NAME}_shaders DEPENDS ${COMPILED_SHADERS})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
endfunction()

# Shader'ları compile et
compile_vulkan_shaders(AstralEngine "${CMAKE_SOURCE_DIR}/Assets/Shaders")
```

---

## 🔗 SDL3-Vulkan Entegrasyonu

### 1. Window.h SDL3-Vulkan Güncellemeleri

`Source/Subsystems/Platform/Window.h` dosyasını güncelleyin:

```cpp
#pragma once

#include <string>
#include <vector>

// SDL3 conditional includes
#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_vulkan.h>
#endif

// Vulkan conditional includes  
#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
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

#ifdef ASTRAL_USE_SDL3
    SDL_Window* GetSDLWindow() const { return m_sdlWindow; }
#endif

    void* GetNativeHandle() const;

    // Vulkan-specific methods
#if defined(ASTRAL_USE_SDL3) && defined(ASTRAL_USE_VULKAN)
    std::vector<const char*> GetRequiredVulkanExtensions() const;
    bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const;
#endif

private:
    void HandleWindowEvent(const void* event);

#ifdef ASTRAL_USE_SDL3
    SDL_Window* m_sdlWindow = nullptr;
#else
    void* m_platformWindow = nullptr;
#endif

    std::string m_title;
    int m_width = 0;
    int m_height = 0;
    bool m_shouldClose = false;
    bool m_initialized = false;
};

} // namespace AstralEngine
```

### 2. Window.cpp SDL3-Vulkan Implementation

`Source/Subsystems/Platform/Window.cpp` dosyasını güncelleyin:

```cpp
#include "Window.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"

namespace AstralEngine {

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
    
    // SDL3'te Vulkan desteği ile window oluşturma
    Uint32 windowFlags = SDL_WINDOW_RESIZABLE;
    
#ifdef ASTRAL_USE_VULKAN
    windowFlags |= SDL_WINDOW_VULKAN;
    Logger::Info("Window", "Creating window with Vulkan support");
#endif
    
    m_sdlWindow = SDL_CreateWindow(
        title.c_str(), 
        width, 
        height, 
        windowFlags
    );
    
    if (!m_sdlWindow) {
        Logger::Error("Window", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
#ifdef ASTRAL_USE_VULKAN
    // Vulkan extension'larını kontrol et
    auto requiredExtensions = GetRequiredVulkanExtensions();
    Logger::Info("Window", "Required Vulkan extensions: {}", requiredExtensions.size());
    for (const auto& ext : requiredExtensions) {
        Logger::Debug("Window", "  - {}", ext);
    }
#endif
    
    Logger::Info("Window", "SDL3 window with Vulkan support created successfully");
    
#else
    Logger::Warning("Window", "SDL3 not available - using placeholder implementation");
    m_platformWindow = nullptr;
#endif
    
    m_initialized = true;
    Logger::Info("Window", "Window initialized successfully");
    
    return true;
}

#if defined(ASTRAL_USE_SDL3) && defined(ASTRAL_USE_VULKAN)
std::vector<const char*> Window::GetRequiredVulkanExtensions() const {
    if (!m_sdlWindow) {
        Logger::Error("Window", "Cannot get Vulkan extensions: window not initialized");
        return {};
    }
    
    uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    
    if (!extensions) {
        Logger::Error("Window", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }
    
    std::vector<const char*> result(extensions, extensions + extensionCount);
    return result;
}

bool Window::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const {
    if (!m_sdlWindow) {
        Logger::Error("Window", "Cannot create Vulkan surface: window not initialized");
        return false;
    }
    
    if (!SDL_Vulkan_CreateSurface(m_sdlWindow, instance, nullptr, surface)) {
        Logger::Error("Window", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return false;
    }
    
    Logger::Info("Window", "Vulkan surface created successfully");
    return true;
}
#endif

} // namespace AstralEngine
```

---

## 💻 Kod Entegrasyonu

### 1. GraphicsDevice Sınıfı

`Source/Subsystems/Renderer/GraphicsDevice.h` dosyasını oluşturun:

```cpp
#pragma once

#include <vector>
#include <memory>
#include <optional>

#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

class Window;

/**
 * @brief Vulkan GraphicsDevice wrapper sınıfı
 * 
 * Vulkan instance, device, queues ve temel resource'ları yönetir.
 * SDL3 window ile entegre çalışır.
 */
class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();

    // Non-copyable
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Yaşam döngüsü
    bool Initialize(Window* window);
    void Shutdown();

    // Getters
#ifdef ASTRAL_USE_VULKAN
    VkInstance GetInstance() const { return m_instance; }
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }
    
    uint32_t GetGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    uint32_t GetPresentQueueFamily() const { return m_presentQueueFamily; }
#endif

    // Device capabilities
    bool IsValidationLayersEnabled() const { return m_validationLayersEnabled; }

private:
    bool CreateInstance(const std::vector<const char*>& requiredExtensions);
    bool SetupDebugMessenger();
    bool CreateSurface(Window* window);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions() const;
    
#ifdef ASTRAL_USE_VULKAN
    // Core Vulkan objects
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    
    // Debug
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    
    // Queues
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
#endif

    Window* m_window = nullptr;
    bool m_initialized = false;
    bool m_validationLayersEnabled = false;
    
    // Validation layers
    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    // Device extensions
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

} // namespace AstralEngine
```

### 2. GraphicsDevice Implementation

`Source/Subsystems/Renderer/GraphicsDevice.cpp` dosyasını oluşturun:

```cpp
#include "GraphicsDevice.h"
#include "../Platform/Window.h"
#include "../../Core/Logger.h"

#include <set>
#include <algorithm>

namespace AstralEngine {

GraphicsDevice::GraphicsDevice() {
    Logger::Debug("GraphicsDevice", "GraphicsDevice created");
}

GraphicsDevice::~GraphicsDevice() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("GraphicsDevice", "GraphicsDevice destroyed");
}

bool GraphicsDevice::Initialize(Window* window) {
    if (m_initialized) {
        Logger::Warning("GraphicsDevice", "GraphicsDevice already initialized");
        return true;
    }
    
    if (!window) {
        Logger::Error("GraphicsDevice", "Cannot initialize without a valid window");
        return false;
    }
    
    m_window = window;
    
    Logger::Info("GraphicsDevice", "Initializing Vulkan GraphicsDevice...");
    
#ifdef ASTRAL_USE_VULKAN
    // Validation layers (debug modda)
    #ifdef ASTRAL_DEBUG
        m_validationLayersEnabled = CheckValidationLayerSupport();
        if (m_validationLayersEnabled) {
            Logger::Info("GraphicsDevice", "Validation layers enabled");
        } else {
            Logger::Warning("GraphicsDevice", "Validation layers requested but not available");
        }
    #endif
    
    // Required extensions (window'dan al + validation)
    auto requiredExtensions = m_window->GetRequiredVulkanExtensions();
    
    // Create Vulkan instance
    if (!CreateInstance(requiredExtensions)) {
        return false;
    }
    
    // Setup debug messenger
    if (m_validationLayersEnabled && !SetupDebugMessenger()) {
        Logger::Warning("GraphicsDevice", "Failed to setup debug messenger");
    }
    
    // Create window surface
    if (!CreateSurface(window)) {
        return false;
    }
    
    // Pick physical device
    if (!PickPhysicalDevice()) {
        return false;
    }
    
    // Create logical device
    if (!CreateLogicalDevice()) {
        return false;
    }
    
    Logger::Info("GraphicsDevice", "Vulkan GraphicsDevice initialized successfully");
    
#else
    Logger::Error("GraphicsDevice", "Vulkan not available - cannot initialize GraphicsDevice");
    return false;
#endif
    
    m_initialized = true;
    return true;
}

void GraphicsDevice::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("GraphicsDevice", "Shutting down GraphicsDevice...");
    
#ifdef ASTRAL_USE_VULKAN
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }
    
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
#endif
    
    m_initialized = false;
    Logger::Info("GraphicsDevice", "GraphicsDevice shutdown complete");
}

#ifdef ASTRAL_USE_VULKAN

bool GraphicsDevice::CreateInstance(const std::vector<const char*>& requiredExtensions) {
    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Astral Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Astral Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;
    
    // Instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // Extensions
    auto extensions = requiredExtensions;
    if (m_validationLayersEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    // Validation layers
    if (m_validationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        Logger::Error("GraphicsDevice", "Failed to create Vulkan instance! Error: {}", static_cast<int>(result));
        return false;
    }
    
    Logger::Info("GraphicsDevice", "Vulkan instance created successfully");
    return true;
}

bool GraphicsDevice::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : m_validationLayers) {
        bool layerFound = false;
        
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    // Message severity'ye göre log level belirle
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Logger::Error("Vulkan", "{}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Logger::Warning("Vulkan", "{}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        Logger::Info("Vulkan", "{}", pCallbackData->pMessage);
    } else {
        Logger::Debug("Vulkan", "{}", pCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

bool GraphicsDevice::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        m_instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (!func) {
        Logger::Error("GraphicsDevice", "vkCreateDebugUtilsMessengerEXT not available");
        return false;
    }
    
    VkResult result = func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS) {
        Logger::Error("GraphicsDevice", "Failed to setup debug messenger! Error: {}", static_cast<int>(result));
        return false;
    }
    
    return true;
}

bool GraphicsDevice::CreateSurface(Window* window) {
    return window->CreateVulkanSurface(m_instance, &m_surface);
}

bool GraphicsDevice::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        Logger::Error("GraphicsDevice", "Failed to find GPUs with Vulkan support!");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    
    // İlk uygun device'ı seç (basit implementasyon)
    for (const auto& device : devices) {
        // TODO: Device suitability check
        m_physicalDevice = device;
        
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        
        Logger::Info("GraphicsDevice", "Selected GPU: {}", deviceProperties.deviceName);
        return true;
    }
    
    Logger::Error("GraphicsDevice", "Failed to find a suitable GPU!");
    return false;
}

bool GraphicsDevice::CreateLogicalDevice() {
    // Queue families (basit implementasyon)
    m_graphicsQueueFamily = 0; // TODO: Proper queue family selection
    m_presentQueueFamily = 0;
    
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {m_graphicsQueueFamily, m_presentQueueFamily};
    
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    // Device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    // Device create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
    
    if (m_validationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    
    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        Logger::Error("GraphicsDevice", "Failed to create logical device! Error: {}", static_cast<int>(result));
        return false;
    }
    
    // Queue'ları al
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    
    Logger::Info("GraphicsDevice", "Vulkan logical device created successfully");
    return true;
}

#endif // ASTRAL_USE_VULKAN

} // namespace AstralEngine
```

---

## 🏗️ RenderSubsystem İmplementasyonu

### 1. RenderSubsystem Header

`Source/Subsystems/Renderer/RenderSubsystem.h` dosyasını oluşturun:

```cpp
#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include <memory>

namespace AstralEngine {

class Window;

/**
 * @brief Vulkan tabanlı render alt sistemi
 * 
 * Grafik API'sini (Vulkan) soyutlar ve sahneyi ekrana çizer.
 * SDL3 window ve GraphicsDevice ile entegre çalışır.
 */
class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }

    // Render interface
    void BeginFrame();
    void EndFrame();

    // Getters
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }

private:
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
};

} // namespace AstralEngine
```

### 2. RenderSubsystem Implementation

`Source/Subsystems/Renderer/RenderSubsystem.cpp` dosyasını oluşturun:

```cpp
#include "RenderSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Platform/PlatformSubsystem.h"

namespace AstralEngine {

RenderSubsystem::RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem created");
}

RenderSubsystem::~RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem destroyed");
}

void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("RenderSubsystem", "Initializing render subsystem...");
    
    // Platform subsystem'den window'u al
    auto platformSubsystem = m_owner->GetSubsystem<PlatformSubsystem>();
    if (!platformSubsystem) {
        Logger::Error("RenderSubsystem", "PlatformSubsystem not found!");
        return;
    }
    
    m_window = platformSubsystem->GetWindow();
    if (!m_window) {
        Logger::Error("RenderSubsystem", "Window not found!");
        return;
    }
    
    // Graphics device oluştur
    m_graphicsDevice = std::make_unique<GraphicsDevice>();
    if (!m_graphicsDevice->Initialize(m_window)) {
        Logger::Error("RenderSubsystem", "Failed to initialize GraphicsDevice!");
        m_graphicsDevice.reset();
        return;
    }
    
    Logger::Info("RenderSubsystem", "Render subsystem initialized successfully");
}

void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice) {
        return;
    }
    
    BeginFrame();
    
    // TODO: Actual rendering
    // - Scene culling
    // - Draw calls
    // - Post-processing
    
    EndFrame();
}

void RenderSubsystem::OnShutdown() {
    Logger::Info("RenderSubsystem", "Shutting down render subsystem...");
    
    if (m_graphicsDevice) {
        m_graphicsDevice->Shutdown();
        m_graphicsDevice.reset();
    }
    
    m_window = nullptr;
    Logger::Info("RenderSubsystem", "Render subsystem shutdown complete");
}

void RenderSubsystem::BeginFrame() {
    // TODO: Frame başlangıç işlemleri
    // - Command buffer begin
    // - Render pass begin
}

void RenderSubsystem::EndFrame() {
    // TODO: Frame bitirme işlemleri
    // - Render pass end
    // - Command buffer submit
    // - Present
}

} // namespace AstralEngine
```

---

## 🧪 Test ve Doğrulama

### 1. Basit Test Uygulaması

Test için `test_vulkan.cpp` oluşturun:

```cpp
#include "Source/Core/Engine.h"
#include "Source/Subsystems/Platform/PlatformSubsystem.h"
#include "Source/Subsystems/Renderer/RenderSubsystem.h"

int main() {
    using namespace AstralEngine;
    
    try {
        Engine engine;
        
        // Subsystem'leri kaydet
        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<RenderSubsystem>();
        
        Logger::Info("Test", "Vulkan integration test starting...");
        
        // Engine'i başlat
        engine.Run();
        
        Logger::Info("Test", "Vulkan integration test completed successfully!");
        return 0;
        
    } catch (const std::exception& e) {
        Logger::Error("Test", "Vulkan test failed: {}", e.what());
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
# [INFO] [GraphicsDevice] Vulkan instance created successfully
# [INFO] [GraphicsDevice] Selected GPU: [GPU Name]
# [INFO] [RenderSubsystem] Render subsystem initialized successfully
```

---

## 🛠️ Troubleshooting

### Yaygın Problemler ve Çözümleri

#### 1. "Vulkan not found" hatası

**Çözüm:**
```bash
# VULKAN_SDK environment variable kontrolü
echo $VULKAN_SDK  # Linux/macOS
echo %VULKAN_SDK%  # Windows

# Manuel path belirleme
cmake .. -DVULKAN_SDK=/path/to/vulkan/sdk
```

#### 2. Validation layers bulunamıyor

**Çözüm:**
```bash
# Linux
export VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layer.d"

# Windows
set VK_LAYER_PATH=C:\VulkanSDK\1.4.321.1\Bin
```

#### 3. "No suitable GPU found" hatası

**Çözüm:**
- Graphics driver'ları güncelleyin
- Vulkan compatible GPU olduğundan emin olun
- `vulkaninfo` komutuyla GPU desteğini kontrol edin

#### 4. Shader compilation hatası

**Çözüm:**
```bash
# GLSLC path kontrolü
which glslc  # Linux/macOS
where glslc  # Windows

# Manuel shader compilation test
glslc shader.vert -o shader.vert.spv
```

#### 5. SDL3-Vulkan integration problemi

**Çözüm:**
```cpp
// SDL3 window'un Vulkan flag'i ile oluşturulduğundan emin olun
SDL_WINDOW_VULKAN flag'ini kontrol edin

// Vulkan extension'ların doğru alındığından emin olun
auto extensions = window->GetRequiredVulkanExtensions();
```

### Debug İpuçları

1. **Validation Layers Aktifleştirme:**
   ```bash
   # Debug build'de otomatik aktif
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   ```

2. **Vulkan Info Kontrolü:**
   ```bash
   vulkaninfo --summary
   ```

3. **RenderDoc Integration:**
   ```bash
   # RenderDoc ile debug
   renderdoc ./AstralEngine
   ```

---

## ✅ Entegrasyon Kontrolü

Vulkan entegrasyonunun başarılı olduğunu doğrulamak için:

- [ ] CMake Vulkan bulması
- [ ] Build hatası olmaması
- [ ] Vulkan instance oluşması
- [ ] Physical device seçilmesi
- [ ] Logical device oluşması
- [ ] SDL3 surface oluşması
- [ ] Validation layers çalışması (debug modda)
- [ ] Log mesajlarında "Vulkan" görünmesi

## 🚀 Sonraki Adımlar

Bu entegrasyon tamamlandıktan sonra:

1. **Swapchain Implementation** - Present operation
2. **Command Buffers** - GPU komut kaydetme
3. **Buffer & Image Management** - Vertex/Index buffers, textures
4. **Render Passes** - Multi-pass rendering
5. **Shader System** - SPIR-V shader loading
6. **Descriptor Sets** - Resource binding
7. **Memory Management** - VMA entegrasyonu

## 📚 Yararlı Linkler

- **Vulkan SDK:** https://vulkan.lunarg.com/
- **Vulkan Specification:** https://registry.khronos.org/vulkan/
- **SDL3 Vulkan Guide:** https://wiki.libsdl.org/SDL3/CategoryVulkan
- **Vulkan Tutorial:** https://vulkan-tutorial.com/
- **Vulkan Memory Allocator:** https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

Bu rehber, Astral Engine'e Vulkan entegrasyonunu tam ve güvenli bir şekilde gerçekleştirmeniz için gerekli tüm adımları içermektedir. Modern Vulkan 1.4 features'lerini kullanarak performanslı bir graphics backend elde edeceksiniz!
