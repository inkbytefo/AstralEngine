# Dear ImGui Entegrasyon Rehberi - Astral Engine

**Tarih:** 10 Eylül 2025  
**Dear ImGui Son Sürüm:** v1.92.2b (13 Ağustos 2025)  
**Engine Hedef:** Astral Engine v0.1.0-alpha

## 📋 İçindekiler

1. [Dear ImGui Hakkında Genel Bilgiler](#dear-imgui-hakkında-genel-bilgiler)
2. [Sistem Gereksinimleri](#sistem-gereksinimleri)
3. [Dear ImGui İndirme ve Kurulum](#dear-imgui-indirme-ve-kurulum)
4. [CMake Entegrasyonu](#cmake-entegrasyonu)
5. [SDL3 + Vulkan + İmGui Entegrasyonu](#sdl3--vulkan--imgui-entegrasyonu)
6. [Kod Entegrasyonu](#kod-entegrasyonu)
7. [Subsystem İmplementasyonu](#subsystem-implementasyonu)
8. [Font ve UI Konfigürasyonu](#font-ve-ui-konfigürasyonu)
9. [Debugging ve Profiling](#debugging-ve-profiling)
10. [Test ve Doğrulama](#test-ve-doğrulama)
11. [Troubleshooting](#troubleshooting)

---

## 🎯 Dear ImGui Hakkında Genel Bilgiler

### Dear ImGui Nedir?

**Dear ImGui**, AAA oyun endüstrisinde yaygın olarak kullanılan, "immediate mode" GUI paradigmasını benimseyen hafif ve güçlü bir C++ kullanıcı arayüzü kütüphanesidir. **Horizon Forbidden West**, **CS:GO**, **World of Warcraft** ve yüzlerce diğer projede kullanılmaktadır.

### 🌟 Dear ImGui v1.92.2b'nin Temel Özellikleri

**Core Features:**
- **Immediate Mode GUI** - State management yok, minimal API
- **Renderer Agnostic** - Herhangi bir 3D API ile çalışır
- **Self-contained** - Minimal dependency, sadece C++ standard library
- **Cross-platform** - Windows, macOS, Linux, iOS, Android, Emscripten
- **MIT License** - Ticari projeler için uygun

**Advanced Features (v1.92+):**
- **Dynamic Texture Support** - `ImGuiBackendFlags_RendererHasTextures` 
- **Multi-Viewport** - Ayrılabilir pencereler (docking branch)
- **Docking System** - Tab ve panel docking
- **Tables API** - Gelişmiş tablo sistemi
- **Multi-Select** - Çoklu seçim desteği
- **Debug Tools** - Built-in metrics ve debug pencereleri

### 🔧 **Desteklenen Backend'ler**
- **Platform:** SDL3, SDL2, GLFW, Win32, Android, OSX
- **Renderer:** Vulkan, DirectX 9/10/11/12, OpenGL, Metal, WebGPU
- **Framework:** SDL_GPU, Allegro5, Emscripten

### ⚡ **Performance Characteristics**
- CPU friendly (immediate mode ≠ immediate rendering)
- GPU efficient (batched draw calls)
- Memory efficient (minimal state)
- Multi-threaded friendly

---

## 🛠️ Sistem Gereksinimleri

### Minimum Gereksinimler

- **CMake:** 3.20+
- **C++ Compiler:**
  - MSVC 2019+ (Windows)
  - Clang 12+ (macOS/Linux)
  - GCC 10+ (Linux)
- **Astral Engine Dependencies:**
  - SDL3 3.2.22+
  - Vulkan SDK 1.4.321+
  - C++20 support

### Platform Gereksinimleri

**Windows:**
- Windows 10 1903+
- Visual Studio 2019+

**Linux:**
- Ubuntu 20.04+/Debian 11+
- Mesa 21.0+ veya proprietary drivers

**macOS:**
- macOS 10.15+ (Catalina)

---

## 📦 Dear ImGui İndirme ve Kurulum

### Method 1: Git Submodule (Önerilen)

```bash
# Astral Engine kök dizininde
cd External
git submodule add https://github.com/ocornut/imgui.git imgui
cd imgui
git checkout v1.92.2b  # Son stable sürüm
```

### Method 2: Manual Download

```bash
# GitHub'dan en son sürümü indirin
cd External
wget https://github.com/ocornut/imgui/archive/refs/tags/v1.92.2b.zip
unzip v1.92.2b.zip
mv imgui-1.92.2b imgui
```

### Method 3: CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.2b
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)
```

---

## 🔧 CMake Entegrasyonu

### 1. Ana CMakeLists.txt Güncellemeleri

AstralEngine'deki `CMakeLists.txt` dosyasını güncelleyin:

```cmake
# Dear ImGui seçenekleri
option(USE_IMGUI "Enable Dear ImGui" ON)

if(USE_IMGUI)
    message(STATUS "Dear ImGui enabled")
    
    # ImGui kök dizini
    set(IMGUI_ROOT_DIR ${EXTERNAL_DIR}/imgui)
    
    # ImGui core files
    set(IMGUI_SOURCES
        ${IMGUI_ROOT_DIR}/imgui.cpp
        ${IMGUI_ROOT_DIR}/imgui_demo.cpp
        ${IMGUI_ROOT_DIR}/imgui_draw.cpp
        ${IMGUI_ROOT_DIR}/imgui_tables.cpp
        ${IMGUI_ROOT_DIR}/imgui_widgets.cpp
    )
    
    # ImGui headers
    set(IMGUI_HEADERS
        ${IMGUI_ROOT_DIR}/imgui.h
        ${IMGUI_ROOT_DIR}/imgui_internal.h
        ${IMGUI_ROOT_DIR}/imconfig.h
        ${IMGUI_ROOT_DIR}/imstb_rectpack.h
        ${IMGUI_ROOT_DIR}/imstb_textedit.h
        ${IMGUI_ROOT_DIR}/imstb_truetype.h
    )
    
    # Backend seçimi
    if(ASTRAL_USE_SDL3 AND ASTRAL_USE_VULKAN)
        message(STATUS "Using ImGui with SDL3 + Vulkan backends")
        
        # Platform backend (SDL3)
        list(APPEND IMGUI_SOURCES ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdl3.cpp)
        list(APPEND IMGUI_HEADERS ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdl3.h)
        
        # Renderer backend (Vulkan)
        list(APPEND IMGUI_SOURCES ${IMGUI_ROOT_DIR}/backends/imgui_impl_vulkan.cpp)
        list(APPEND IMGUI_HEADERS ${IMGUI_ROOT_DIR}/backends/imgui_impl_vulkan.h)
        
        # SDL_GPU backend (alternatif)
        if(USE_SDL_GPU_BACKEND)
            list(APPEND IMGUI_SOURCES ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdlgpu3.cpp)
            list(APPEND IMGUI_HEADERS ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdlgpu3.h)
        endif()
        
    else()
        message(WARNING "ImGui backends require SDL3 and Vulkan")
    endif()
    
    # ImGui target oluştur
    add_library(ImGui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})
    target_include_directories(ImGui PUBLIC ${IMGUI_ROOT_DIR})
    target_include_directories(ImGui PUBLIC ${IMGUI_ROOT_DIR}/backends)
    
    # C++20 support
    target_compile_features(ImGui PUBLIC cxx_std_20)
    
    # Platform-specific definitions
    if(WIN32)
        target_compile_definitions(ImGui PUBLIC 
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()
    
    # Backend bağımlılıkları
    if(ASTRAL_USE_SDL3)
        target_link_libraries(ImGui PUBLIC SDL3::SDL3)
        target_compile_definitions(ImGui PUBLIC IMGUI_IMPL_SDL3_BACKEND)
    endif()
    
    if(ASTRAL_USE_VULKAN)
        target_link_libraries(ImGui PUBLIC Vulkan::Vulkan)
        target_compile_definitions(ImGui PUBLIC 
            IMGUI_IMPL_VULKAN_BACKEND
            VK_NO_PROTOTYPES=0  # Vulkan function pointers
        )
    endif()
    
    # Astral Engine'e bağla
    target_link_libraries(AstralEngine PRIVATE ImGui)
    target_compile_definitions(AstralEngine PRIVATE 
        ASTRAL_USE_IMGUI
        IMGUI_USER_CONFIG="AstralImConfig.h"  # Custom config
    )
    
    message(STATUS "Dear ImGui integrated successfully")
    
else()
    message(STATUS "Dear ImGui disabled")
endif()
```

### 2. Custom ImGui Configuration

`Source/UI/AstralImConfig.h` dosyasını oluşturun:

```cpp
#pragma once

// Astral Engine için özelleştirilmiş ImGui konfigürasyonu

// Memory allocator
#define IMGUI_USE_WCHAR32                    // Unicode desteği
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS     // Deprecated API'leri devre dışı bırak
#define IMGUI_DISABLE_OBSOLETE_KEYIO         // Eski input sistem

// Performance optimizations
#define IMGUI_DISABLE_DEMO_WINDOWS           // Release'te demo pencereyi kaldır
#define IMGUI_DISABLE_DEBUG_TOOLS            // Release'te debug araçları kaldır (şartlı)

// Integration
#define IMGUI_DEFINE_MATH_OPERATORS          // Math operatörleri etkinleştir
#define IMGUI_ENABLE_FREETYPE                // FreeType font rendering

// Platform specific
#ifdef ASTRAL_DEBUG
    #define IMGUI_DEBUG_PARANOID             // Extra debug checks
    #define IM_ASSERT(expr) assert(expr)     // Custom assert
#else 
    #undef IMGUI_DISABLE_DEMO_WINDOWS        // Demo'yu sadece debug'da göster
    #undef IMGUI_DISABLE_DEBUG_TOOLS
#endif

// Astral Engine math integration
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"
#include "../Math/Vector4.h"

// Custom math types (opsiyonel)
#define IM_VEC2_CLASS_EXTRA                                         \
    constexpr ImVec2(const AstralEngine::Vector2& v) : x(v.x), y(v.y) {} \
    constexpr operator AstralEngine::Vector2() const { return AstralEngine::Vector2(x, y); }

#define IM_VEC4_CLASS_EXTRA                                             \
    constexpr ImVec4(const AstralEngine::Vector4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {} \
    constexpr operator AstralEngine::Vector4() const { return AstralEngine::Vector4(x, y, z, w); }

// Custom allocator (opsiyonel)
namespace AstralEngine {
    void* ImGuiAlloc(size_t size, void* user_data);
    void ImGuiFree(void* ptr, void* user_data);
}

#define IMGUI_USER_CONFIG_NO_WARN_UNDEFINED_FUNCTIONS
```

---

## 🔗 SDL3 + Vulkan + İmGui Entegrasyonu

### 1. UISubsystem Header

`Source/Subsystems/UI/UISubsystem.h` dosyasını oluşturun:

```cpp
#pragma once

#include "../../Core/ISubsystem.h"
#include "../../Core/Logger.h"
#include <memory>

#ifdef ASTRAL_USE_IMGUI
    #include "AstralImConfig.h"
    #include <imgui.h>
    #ifdef ASTRAL_USE_SDL3
        #include <imgui_impl_sdl3.h>
    #endif
    #ifdef ASTRAL_USE_VULKAN
        #include <imgui_impl_vulkan.h>
    #endif
#endif

namespace AstralEngine {

class Engine;
class Window;
class GraphicsDevice;

/**
 * @brief Dear ImGui tabanlı UI alt sistemi
 * 
 * SDL3 platform backend ve Vulkan renderer backend kullanır.
 * Debug tools, profilers ve editor UI için optimized.
 */
class UISubsystem : public ISubsystem {
public:
    UISubsystem();
    ~UISubsystem() override;

    // Non-copyable
    UISubsystem(const UISubsystem&) = delete;
    UISubsystem& operator=(const UISubsystem&) = delete;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "UISubsystem"; }

    // Frame management
    void BeginFrame();
    void EndFrame();
    void Render(); // Vulkan render pass'te çağrılır

    // UI State
    bool IsCapturingMouse() const;
    bool IsCapturingKeyboard() const;
    bool HasFocus() const;
    
    // Debug UI
    void ShowDebugWindow(bool* open = nullptr);
    void ShowMetricsWindow(bool* open = nullptr);
    void ShowDemoWindow(bool* open = nullptr);
    
    // Font management
    bool LoadFont(const std::string& name, const std::string& path, float size = 16.0f);
    void SetDefaultFont(const std::string& name);
    void RebuildFontAtlas();

private:
    bool InitializeImGui();
    void SetupImGuiStyle();
    void SetupVulkanBackend();
    void ShutdownImGui();

#ifdef ASTRAL_USE_IMGUI
    void ProcessSDLEvent(const void* event);
    void SetupFonts();
    void UpdateViewport();
#endif

private:
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    GraphicsDevice* m_graphicsDevice = nullptr;
    
    bool m_initialized = false;
    bool m_showDemo = false;
    bool m_showMetrics = false;
    bool m_showDebugWindow = false;
    
    // Font management
    std::unordered_map<std::string, ImFont*> m_fonts;
    std::string m_defaultFont = "default";
    
    // Vulkan resources
#ifdef ASTRAL_USE_VULKAN
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkCommandPool> m_commandPools;
#endif
};

} // namespace AstralEngine
```

### 2. UISubsystem Implementation

`Source/Subsystems/UI/UISubsystem.cpp` dosyasını oluşturun:

```cpp
#include "UISubsystem.h"
#include "../../Core/Engine.h"
#include "../Platform/PlatformSubsystem.h"
#include "../Renderer/RenderSubsystem.h"
#include "../../Events/EventManager.h"

namespace AstralEngine {

UISubsystem::UISubsystem() {
    Logger::Debug("UISubsystem", "UISubsystem created");
}

UISubsystem::~UISubsystem() {
    if (m_initialized) {
        OnShutdown();
    }
    Logger::Debug("UISubsystem", "UISubsystem destroyed");
}

void UISubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("UISubsystem", "Initializing UI subsystem...");

#ifndef ASTRAL_USE_IMGUI
    Logger::Warning("UISubsystem", "Dear ImGui not available - UI subsystem disabled");
    return;
#endif

    // Platform ve Renderer subsystem'leri al
    auto platformSubsystem = m_owner->GetSubsystem<PlatformSubsystem>();
    auto renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
    
    if (!platformSubsystem || !renderSubsystem) {
        Logger::Error("UISubsystem", "Required subsystems not found!");
        return;
    }

    m_window = platformSubsystem->GetWindow();
    m_graphicsDevice = renderSubsystem->GetGraphicsDevice();
    
    if (!m_window || !m_graphicsDevice) {
        Logger::Error("UISubsystem", "Window or GraphicsDevice not available!");
        return;
    }

    // ImGui'yi başlat
    if (!InitializeImGui()) {
        Logger::Error("UISubsystem", "Failed to initialize Dear ImGui!");
        return;
    }

    m_initialized = true;
    Logger::Info("UISubsystem", "UI subsystem initialized successfully");
    
    // Debug windows (debug modda)
    #ifdef ASTRAL_DEBUG
        m_showDemo = true;
        m_showMetrics = true;
        m_showDebugWindow = true;
    #endif
}

void UISubsystem::OnUpdate(float deltaTime) {
    if (!m_initialized) {
        return;
    }

#ifdef ASTRAL_USE_IMGUI
    BeginFrame();
    
    // Debug UI
    if (m_showDemo) {
        ImGui::ShowDemoWindow(&m_showDemo);
    }
    
    if (m_showMetrics) {
        ImGui::ShowMetricsWindow(&m_showMetrics);
    }
    
    if (m_showDebugWindow) {
        ShowDebugWindow(&m_showDebugWindow);
    }

    // Ana menü
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Demo Window", nullptr, &m_showDemo);
            ImGui::MenuItem("Metrics", nullptr, &m_showMetrics);
            ImGui::MenuItem("Debug Info", nullptr, &m_showDebugWindow);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Engine")) {
            if (ImGui::MenuItem("Shutdown")) {
                m_owner->RequestShutdown();
            }
            ImGui::EndMenu();
        }
        
        // FPS display
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        
        ImGui::EndMainMenuBar();
    }
#endif
}

void UISubsystem::OnShutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("UISubsystem", "Shutting down UI subsystem...");
    
    ShutdownImGui();
    
    m_window = nullptr;
    m_graphicsDevice = nullptr;
    m_owner = nullptr;
    m_initialized = false;
    
    Logger::Info("UISubsystem", "UI subsystem shutdown complete");
}

void UISubsystem::BeginFrame() {
#ifdef ASTRAL_USE_IMGUI
    // Backend frame start
    #ifdef ASTRAL_USE_VULKAN
        ImGui_ImplVulkan_NewFrame();
    #endif
    #ifdef ASTRAL_USE_SDL3
        ImGui_ImplSDL3_NewFrame();
    #endif
    
    // ImGui frame start
    ImGui::NewFrame();
#endif
}

void UISubsystem::EndFrame() {
#ifdef ASTRAL_USE_IMGUI
    // Render prepare
    ImGui::Render();
#endif
}

void UISubsystem::Render() {
#ifdef ASTRAL_USE_IMGUI && ASTRAL_USE_VULKAN
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        // Bu fonksiyon RenderSubsystem'den çağrılacak
        // Command buffer RenderSubsystem tarafından sağlanacak
        ImGui_ImplVulkan_RenderDrawData(drawData, /* command_buffer */);
    }
#endif
}

bool UISubsystem::IsCapturingMouse() const {
#ifdef ASTRAL_USE_IMGUI
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

bool UISubsystem::IsCapturingKeyboard() const {
#ifdef ASTRAL_USE_IMGUI
    return ImGui::GetIO().WantCaptureKeyboard;
#else
    return false;
#endif
}

bool UISubsystem::HasFocus() const {
#ifdef ASTRAL_USE_IMGUI
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
#else
    return false;
#endif
}

void UISubsystem::ShowDebugWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    if (!open || !*open) return;
    
    if (ImGui::Begin("Astral Engine Debug", open)) {
        // Engine bilgileri
        ImGui::Text("Engine: Astral Engine v0.1.0-alpha");
        ImGui::Text("Build: %s %s", __DATE__, __TIME__);
        ImGui::Separator();
        
        // Performance
        ImGui::Text("Performance:");
        ImGui::Text("  Frame Time: %.3f ms", ImGui::GetIO().DeltaTime * 1000.0f);
        ImGui::Text("  FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();
        
        // Subsystems
        ImGui::Text("Subsystems:");
        ImGui::BulletText("Platform: %s", m_owner->GetSubsystem<PlatformSubsystem>() ? "OK" : "FAILED");
        ImGui::BulletText("Renderer: %s", m_owner->GetSubsystem<RenderSubsystem>() ? "OK" : "FAILED");
        ImGui::BulletText("UI: OK");
        
        ImGui::Separator();
        
        // Window info
        if (m_window) {
            ImGui::Text("Window Info:");
            ImGui::Text("  Size: %dx%d", m_window->GetWidth(), m_window->GetHeight());
            ImGui::Text("  Title: %s", m_window->GetTitle().c_str());
        }
    }
    ImGui::End();
#endif
}

void UISubsystem::ShowMetricsWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    ImGui::ShowMetricsWindow(open);
#endif
}

void UISubsystem::ShowDemoWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    ImGui::ShowDemoWindow(open);
#endif
}

bool UISubsystem::LoadFont(const std::string& name, const std::string& path, float size) {
#ifdef ASTRAL_USE_IMGUI
    auto& io = ImGui::GetIO();
    
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
    if (!font) {
        Logger::Error("UISubsystem", "Failed to load font: {}", path);
        return false;
    }
    
    m_fonts[name] = font;
    Logger::Info("UISubsystem", "Loaded font '{}' from {}", name, path);
    return true;
#else
    return false;
#endif
}

void UISubsystem::SetDefaultFont(const std::string& name) {
    m_defaultFont = name;
}

void UISubsystem::RebuildFontAtlas() {
#ifdef ASTRAL_USE_IMGUI && ASTRAL_USE_VULKAN
    // Font atlas'ı yeniden oluştur
    ImGui_ImplVulkan_DestroyFontsTexture();
    
    // Yeni font atlas'ı upload et
    VkCommandBuffer commandBuffer = /* BeginSingleTimeCommands() */;
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    /* EndSingleTimeCommands(commandBuffer) */;
#endif
}

// Private methods

bool UISubsystem::InitializeImGui() {
#ifdef ASTRAL_USE_IMGUI
    // ImGui context oluştur
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard navigation
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Gamepad navigation
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Multi-viewports (optional)

    // Style setup
    SetupImGuiStyle();
    
    // Platform backend (SDL3)
    #ifdef ASTRAL_USE_SDL3
        if (!ImGui_ImplSDL3_InitForVulkan(m_window->GetSDLWindow())) {
            Logger::Error("UISubsystem", "Failed to initialize SDL3 backend");
            return false;
        }
    #endif
    
    // Renderer backend (Vulkan)
    #ifdef ASTRAL_USE_VULKAN
        if (!SetupVulkanBackend()) {
            Logger::Error("UISubsystem", "Failed to initialize Vulkan backend");
            return false;
        }
    #endif
    
    // Font setup
    SetupFonts();
    
    Logger::Info("UISubsystem", "Dear ImGui initialized successfully");
    Logger::Info("UISubsystem", "  Version: {}", IMGUI_VERSION);
    Logger::Info("UISubsystem", "  Backend: SDL3 + Vulkan");
    
    return true;
#else
    return false;
#endif
}

void UISubsystem::SetupImGuiStyle() {
#ifdef ASTRAL_USE_IMGUI
    // Dark theme (default)
    ImGui::StyleColorsDark();
    
    // Custom Astral Engine style
    auto& style = ImGui::GetStyle();
    
    // Spacing
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    
    // Rounding
    style.WindowRounding = 6.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 3.0f;
    
    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    
    // Colors (Astral theme)
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.2f, 0.3f, 0.5f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.4f, 0.6f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.5f, 0.7f, 1.00f);
    colors[ImGuiCol_Button] = ImVec2(0.2f, 0.3f, 0.5f, 0.60f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.4f, 0.6f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.5f, 0.7f, 1.00f);
#endif
}

bool UISubsystem::SetupVulkanBackend() {
#ifdef ASTRAL_USE_VULKAN
    // Descriptor pool oluştur (ImGui için)
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkResult result = vkCreateDescriptorPool(
        m_graphicsDevice->GetDevice(), 
        &poolInfo, 
        nullptr, 
        &m_descriptorPool
    );
    
    if (result != VK_SUCCESS) {
        Logger::Error("UISubsystem", "Failed to create descriptor pool for ImGui");
        return false;
    }

    // ImGui Vulkan init info
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_graphicsDevice->GetInstance();
    initInfo.PhysicalDevice = m_graphicsDevice->GetPhysicalDevice();
    initInfo.Device = m_graphicsDevice->GetDevice();
    initInfo.QueueFamily = m_graphicsDevice->GetGraphicsQueueFamily();
    initInfo.Queue = m_graphicsDevice->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = /* swapchain image count */;
    initInfo.CheckVkResultFn = [](VkResult result) {
        if (result != VK_SUCCESS) {
            Logger::Error("ImGui", "Vulkan error: {}", static_cast<int>(result));
        }
    };

    // Render pass oluştur (UI için)
    // Bu kısmı RenderSubsystem ile entegre etmek gerekiyor
    
    if (!ImGui_ImplVulkan_Init(&initInfo, m_renderPass)) {
        Logger::Error("UISubsystem", "Failed to initialize ImGui Vulkan backend");
        return false;
    }

    // Font texture upload
    VkCommandBuffer commandBuffer = /* BeginSingleTimeCommands() */;
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    /* EndSingleTimeCommands(commandBuffer) */;

    return true;
#else
    return false;
#endif
}

void UISubsystem::SetupFonts() {
#ifdef ASTRAL_USE_IMGUI
    auto& io = ImGui::GetIO();
    
    // Default font
    io.Fonts->AddFontDefault();
    m_fonts["default"] = io.Fonts->Fonts[0];
    
    // Additional fonts (opsiyonel)
    // TODO: Asset system entegrasyonu
    /*
    LoadFont("roboto", "Assets/Fonts/Roboto-Regular.ttf", 16.0f);
    LoadFont("roboto_bold", "Assets/Fonts/Roboto-Bold.ttf", 16.0f);
    LoadFont("mono", "Assets/Fonts/RobotoMono-Regular.ttf", 14.0f);
    */
#endif
}

void UISubsystem::ShutdownImGui() {
#ifdef ASTRAL_USE_IMGUI
    // Vulkan backend cleanup
    #ifdef ASTRAL_USE_VULKAN
        ImGui_ImplVulkan_Shutdown();
        
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_graphicsDevice->GetDevice(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        
        // Render pass ve framebuffers temizliği
        // TODO: Render pass cleanup
    #endif
    
    // Platform backend cleanup
    #ifdef ASTRAL_USE_SDL3
        ImGui_ImplSDL3_Shutdown();
    #endif
    
    // ImGui context cleanup
    ImGui::DestroyContext();
    
    m_fonts.clear();
#endif
}

} // namespace AstralEngine
```

---

## 💻 RenderSubsystem Entegrasyonu

### RenderSubsystem Header Güncellemesi

`Source/Subsystems/Renderer/RenderSubsystem.h` dosyasına eklemeler:

```cpp
#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include <memory>

#ifdef ASTRAL_USE_IMGUI
    #include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

class Window;
class UISubsystem;

class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }

    // Rendering
    void BeginFrame();
    void EndFrame();
    
    // UI Integration
    void RenderUI();
    VkRenderPass GetUIRenderPass() const { return m_uiRenderPass; }
    VkCommandBuffer GetCurrentUICommandBuffer() const;

    // Getters
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }

private:
    void CreateUIRenderPass();
    void CreateUIFramebuffers();
    void CreateUICommandBuffers();
    void DestroyUIResources();

private:
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    UISubsystem* m_uiSubsystem = nullptr;
    
    // UI rendering resources
#ifdef ASTRAL_USE_IMGUI
    VkRenderPass m_uiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_uiFramebuffers;
    std::vector<VkCommandBuffer> m_uiCommandBuffers;
    std::vector<VkCommandPool> m_uiCommandPools;
    uint32_t m_currentFrame = 0;
#endif
};

} // namespace AstralEngine
```

### RenderSubsystem Implementation Güncellemesi

```cpp
void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("RenderSubsystem", "Initializing render subsystem...");
    
    // ... existing initialization ...
    
    // UI resources oluştur
#ifdef ASTRAL_USE_IMGUI
    CreateUIRenderPass();
    CreateUIFramebuffers();
    CreateUICommandBuffers();
#endif
    
    Logger::Info("RenderSubsystem", "Render subsystem initialized successfully");
}

void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice) {
        return;
    }
    
    BeginFrame();
    
    // TODO: Scene rendering
    
    // UI rendering
    RenderUI();
    
    EndFrame();
}

void RenderSubsystem::RenderUI() {
#ifdef ASTRAL_USE_IMGUI
    if (!m_uiSubsystem) {
        m_uiSubsystem = m_owner->GetSubsystem<UISubsystem>();
        if (!m_uiSubsystem) return;
    }

    VkCommandBuffer commandBuffer = GetCurrentUICommandBuffer();
    
    // UI render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_uiRenderPass;
    renderPassInfo.framebuffer = m_uiFramebuffers[m_currentFrame];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_window->GetWidth(), m_window->GetHeight()};

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // ImGui rendering
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
    
    vkCmdEndRenderPass(commandBuffer);
#endif
}

void RenderSubsystem::CreateUIRenderPass() {
#ifdef ASTRAL_USE_IMGUI
    // Attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = /* swapchain format */;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // UI üzerine çiz
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_graphicsDevice->GetDevice(), &renderPassInfo, nullptr, &m_uiRenderPass) != VK_SUCCESS) {
        Logger::Error("RenderSubsystem", "Failed to create UI render pass");
    }
#endif
}
```

---

## 🎨 Font ve UI Konfigürasyonu

### Font Loading System

```cpp
// Assets/Fonts/ dizininde font'ları saklayın
bool UISubsystem::SetupFonts() {
    auto& io = ImGui::GetIO();
    
    // Default font
    io.Fonts->AddFontDefault();
    
    // Custom fonts
    const char* fontPath = "Assets/Fonts/Roboto-Regular.ttf";
    if (std::filesystem::exists(fontPath)) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
        if (font) {
            m_fonts["roboto"] = font;
            Logger::Info("UISubsystem", "Loaded Roboto font");
        }
    }
    
    // Icon font (Font Awesome)
    const char* iconPath = "Assets/Fonts/fa-solid-900.ttf";
    if (std::filesystem::exists(iconPath)) {
        static const ImWchar iconRanges[] = { 0xf000, 0xf3ff, 0 };
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphOffset = ImVec2(0, 3);
        
        ImFont* iconFont = io.Fonts->AddFontFromFileTTF(iconPath, 14.0f, &iconConfig, iconRanges);
        if (iconFont) {
            Logger::Info("UISubsystem", "Loaded Font Awesome icons");
        }
    }
    
    // Build font atlas
    io.Fonts->Build();
    return true;
}
```

### Custom UI Theme

```cpp
void UISubsystem::SetupAstralTheme() {
    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;

    // Astral Engine color scheme
    const ImVec4 astralBlue    = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
    const ImVec4 astralDark    = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    const ImVec4 astralGray    = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    const ImVec4 astralLight   = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

    // Main
    colors[ImGuiCol_Text]                   = astralLight;
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.11f, 0.11f, 0.11f, 0.95f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    
    // Borders
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Frame BG
    colors[ImGuiCol_FrameBg]                = ImVec4(0.2f, 0.2f, 0.2f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.4f);
    colors[ImGuiCol_FrameBgActive]          = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.67f);
    
    // Title
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    colors[ImGuiCol_TitleBgActive]          = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.0f, 0.0f, 0.0f, 0.51f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);
    
    // Check Mark
    colors[ImGuiCol_CheckMark]              = astralBlue;
    
    // Slider
    colors[ImGuiCol_SliderGrab]             = astralBlue;
    colors[ImGuiCol_SliderGrabActive]       = astralBlue * ImVec4(1.2f, 1.2f, 1.2f, 1.0f);
    
    // Button
    colors[ImGuiCol_Button]                 = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.4f);
    colors[ImGuiCol_ButtonHovered]          = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.6f);
    colors[ImGuiCol_ButtonActive]           = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
    
    // Header
    colors[ImGuiCol_Header]                 = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = astralBlue * ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
    colors[ImGuiCol_HeaderActive]           = astralBlue;
    
    // Style adjustments
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(8, 4);
}
```

---

## 🧪 Test ve Doğrulama

### 1. Basit Test Uygulaması

Test için `test_imgui.cpp` oluşturun:

```cpp
#include "Source/Core/Engine.h"
#include "Source/Subsystems/Platform/PlatformSubsystem.h"
#include "Source/Subsystems/Renderer/RenderSubsystem.h"
#include "Source/Subsystems/UI/UISubsystem.h"

int main() {
    using namespace AstralEngine;
    
    try {
        Engine engine;
        
        // Subsystem'leri kaydet
        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<RenderSubsystem>();
        engine.RegisterSubsystem<UISubsystem>();
        
        Logger::Info("Test", "Dear ImGui integration test starting...");
        
        // Engine'i başlat
        engine.Run();
        
        Logger::Info("Test", "Dear ImGui integration test completed successfully!");
        return 0;
        
    } catch (const std::exception& e) {
        Logger::Error("Test", "ImGui test failed: {}", e.what());
        return -1;
    }
}
```

### 2. Build ve Test

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_IMGUI=ON
cmake --build .

# Test çalıştır
./AstralEngine

# Log çıktısında arayın:
# [INFO] [UISubsystem] Dear ImGui initialized successfully
# [INFO] [UISubsystem] Version: 1.92.2b
# [INFO] [UISubsystem] Backend: SDL3 + Vulkan
```

### 3. Functional Tests

```cpp
// UI Test sınıfı
class UITestWindow {
public:
    void Render() {
        if (ImGui::Begin("Astral Engine Test")) {
            // Basic widgets
            ImGui::Text("Hello, Astral Engine!");
            
            static float value = 0.0f;
            ImGui::SliderFloat("Test Slider", &value, 0.0f, 1.0f);
            
            static bool checkbox = false;
            ImGui::Checkbox("Test Checkbox", &checkbox);
            
            if (ImGui::Button("Test Button")) {
                Logger::Info("UI", "Test button clicked!");
            }
            
            // Performance info
            ImGui::Separator();
            ImGui::Text("Frame Time: %.3f ms", ImGui::GetIO().DeltaTime * 1000.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            
            // Input capture test
            ImGui::Separator();
            ImGui::Text("Mouse Captured: %s", ImGui::GetIO().WantCaptureMouse ? "Yes" : "No");
            ImGui::Text("Keyboard Captured: %s", ImGui::GetIO().WantCaptureKeyboard ? "Yes" : "No");
        }
        ImGui::End();
    }
};
```

---

## 🛠️ Troubleshooting

### Yaygın Problemler ve Çözümleri

#### 1. "ImGui headers not found" hatası

**Çözüm:**
```bash
# Include path'leri kontrol edin
cmake .. -DIMGUI_ROOT_DIR=/path/to/imgui
```

#### 2. "Vulkan validation layer" hataları

**Çözüm:**
```cpp
// Debug modda validation layer'ları devre dışı bırakın
#ifdef ASTRAL_DEBUG
    // Vulkan debug callback'de ImGui hatalarını filtreleyin
    if (strstr(pCallbackData->pMessage, "VUID-vkCmdDrawIndexed-None-02721")) {
        return VK_FALSE; // ImGui'nin bilinen bir problemi
    }
#endif
```

#### 3. "Font atlas corruption" problemi

**Çözüm:**
```cpp
// Font atlas'ı her swapchain recreation'da yeniden yükleyin
void UISubsystem::OnSwapchainRecreated() {
    ImGui_ImplVulkan_DestroyFontsTexture();
    
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    EndSingleTimeCommands(cmd);
}
```

#### 4. "Input not working" problemi

**Çözüm:**
```cpp
// SDL event handling'de ImGui event'lerini proces edin
void PlatformSubsystem::HandleSDLEvent(SDL_Event& event) {
    #ifdef ASTRAL_USE_IMGUI
        ImGui_ImplSDL3_ProcessEvent(&event);
    #endif
    
    // Kendi event handling'iniz
    HandleEngineEvent(event);
}
```

#### 5. "Performance issues" 

**Çözüm:**
```cpp
// ImGui render'ını optimize edin
void UISubsystem::OptimizeRendering() {
    auto& io = ImGui::GetIO();
    
    // Vertex buffer optimization
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    
    // Multi-viewport'u devre dışı bırakın (gerekmediyse)
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    
    // Docking'i sadece gerektiğinde etkinleştirin
    #ifndef ASTRAL_EDITOR_MODE
        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
    #endif
}
```

### Debug İpuçları

1. **Verbose Logging:**
   ```cpp
   #ifdef ASTRAL_DEBUG
   Logger::SetLevel(Logger::Level::Debug);
   #endif
   ```

2. **ImGui Debug Windows:**
   ```cpp
   ImGui::ShowMetricsWindow();  // Performance metrics
   ImGui::ShowDebugLogWindow(); // Debug log
   ImGui::ShowIDStackToolWindow(); // ID stack debugging
   ```

3. **Vulkan Validation:**
   ```bash
   export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d
   export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
   ```

---

## ✅ Entegrasyon Kontrolü

Dear ImGui entegrasyonunun başarılı olduğunu doğrulamak için:

### Checklist:
- [ ] CMake configuration geçiyor ve ImGui bulunuyor
- [ ] Build hatası yok (linking, missing symbols, etc.)
- [ ] SDL3 window açılıyor ve event'ler çalışıyor
- [ ] Vulkan context oluşuyor ve rendering yapılıyor
- [ ] ImGui demo window görünüyor ve interactive
- [ ] Font'lar düzgün yükleniyor
- [ ] Input events çalışıyor (mouse, keyboard)
- [ ] UI responsive ve performanslı
- [ ] Debug tools çalışıyor (metrics, demo)
- [ ] Multiple frames stable çalışıyor
- [ ] Window resize çalışıyor
- [ ] Log'larda "ImGui initialized successfully" görünüyor

### Performance Benchmarks:
- UI render time < 0.5ms (60 FPS'te)
- Font atlas upload < 10ms
- No Vulkan validation errors
- Stable memory usage (no leaks)

## 🚀 Sonraki Adımlar

Bu entegrasyon tamamlandıktan sonra:

1. **Editor UI Development** - Scene editor, asset browser
2. **Asset System Integration** - Texture loading, material editor  
3. **Profiling Tools** - Performance profiler, memory analyzer
4. **Game UI System** - HUD, menus, inventory systems
5. **Docking & Multi-Viewport** - Advanced layout system
6. **Custom Widgets** - Engine-specific UI components
7. **Theme System** - Customizable UI themes
8. **Localization** - Multi-language support

## 📚 Yararlı Linkler

- **Dear ImGui GitHub:** https://github.com/ocornut/imgui
- **Dear ImGui Wiki:** https://github.com/ocornut/imgui/wiki
- **SDL3 Integration:** https://wiki.libsdl.org/SDL3/
- **Vulkan Backend:** https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_vulkan.cpp
- **Font Awesome Icons:** https://fontawesome.com/
- **ImGui Extensions:** https://github.com/ocornut/imgui/wiki/Useful-Extensions
- **ImPlot (Charts):** https://github.com/epezent/implot
- **ImGui Test Engine:** https://github.com/ocornut/imgui_test_engine

Bu rehber, Astral Engine'e Dear ImGui entegrasyonunu modern SDL3 ve Vulkan teknolojileri ile güvenli ve performanslı bir şekilde gerçekleştirmeniz için gerekli tüm adımları içermektedir. İmGui, Astral Engine'e güçlü debug tools, editor UI ve geliştirici araçları kazandıracaktır!
