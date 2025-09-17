# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Window Platform Abstraction Layer
#  *
#  * Implementation File: Window.cpp
#  * Purpose: SDL3 temelli platform soyutlaması implementasyonu
#  * Author: Astral Engine Development Team
#  * Version: 1.0.0
#  * Date: 2025-09-11
#  * Dependencies: SDL3, C++20, Logger, EventManager
#  **********************************************************************/

#include "Window.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"
#include "InputManager.h"

#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_audio.h>
    #include <SDL3/SDL_video.h>
    #include <SDL3/SDL_events.h>
    #include <SDL3/SDL_version.h>
    #include <SDL3/SDL_vulkan.h>
#else
    // Placeholder definitions for SDL3-less builds
    namespace std {
        namespace chrono {
            template<typename duration>
            class duration { /* ... */ };
        }
    }
#endif

// Vulkan headers (conditionally included)
#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

// **********************************************************************
// Window Sınıfı Implementasyonu
// **********************************************************************

/**
 * @brief Window constructor
 */
Window::Window() : m_data(std::make_unique<WindowData>()) {
    // Initialize data structure with default values
    m_data->AspectRatio = 1.0f;
    m_data->IsInitialized = false;
    m_data->IsRenderingContextCreated = false;
    
    // Atomic initialization settings
    m_data->ShouldClose = false;
    m_data->IsFullscreen = false;
    m_data->IsMaximized = false;
    m_data->IsMinimized = false;
    m_data->IsFocused = false;
    
    Logger::Debug("Window", "Window instance created");
}

/**
 * @brief Window destructor
 */
Window::~Window() {
    if (m_data->IsInitialized) {
        Shutdown();
    }
    Logger::Debug("Window", "Window instance destroyed");
}

// **********************************************************************
// Copy and Move Constructors
// **********************************************************************

/**
 * @brief Move constructor
 */
Window::Window(Window&& other) noexcept : m_data(std::move(other.m_data)) {
    // Move semantics ile verileri güvenli bir şekilde taşı
    Logger::Debug("Window", "Window moved");
}

/**
 * @brief Move assignment operator
 */
Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        m_data = std::move(other.m_data);
        Logger::Debug("Window", "Window assigned via move");
    }
    return *this;
}

// **********************************************************************
// Window Lifecycle Management
// **********************************************************************

/**
 * @brief Window initialization with SDL3
 */
bool Window::Initialize(const std::string& title, int width, int height) {
    if (m_data->IsInitialized) {
        Logger::Warning("Window", "Window already initialized");
        return true;
    }
    
    Logger::Info("Window", "Initializing window: '{}' ({}x{})", title, width, height);
    
    // Security validation
    if (width < 100 || height < 100) {
        Logger::Error("Window", "Invalid window dimensions: minimum 100x100 required");
        return false;
    }
    
    // Cache initial parameters
    m_data->Title = title;
    m_data->Width = width;
    m_data->Height = height;
    
#ifdef ASTRAL_USE_SDL3
    // SDL3 initialization with error handling
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        Logger::Error("Window", "SDL initialization failed: {}", SDL_GetError());
        return false;
    }
    
// SDL3 window creation with comprehensive flags
    m_data->SDLWindow = SDL_CreateWindow(
        title.c_str(),
        width, 
        height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    
    if (!m_data->SDLWindow) {
        Logger::Error("Window", "SDL window creation failed: {}", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    // Get window ID for event tracking
    m_data->WindowID = SDL_GetWindowID(m_data->SDLWindow);
    
    // Make window visible
    SDL_ShowWindow(m_data->SDLWindow);
    
    // Verify initialization
    int actualWidth, actualHeight;
    SDL_GetWindowSize(m_data->SDLWindow, &actualWidth, &actualHeight);
    if (actualWidth != width || actualHeight != height) {
        Logger::Warning("Window", "Requested size {}x{}, actual size {}x{}", 
                      width, height, actualWidth, actualHeight);
    }
    
    Logger::Info("Window", "SDL3 window created successfully");
    
#else
    // Placeholder implementation for SDL3-less builds
    Logger::Warning("Window", "SDL3 not available - using placeholder implementation");
    m_data->PlatformWindow = nullptr;
#endif
    
    // Update cached data and complete initialization
    UpdateCachedData();
    m_data->IsInitialized = true;
    
    Logger::Info("Window", "Window initialized successfully");
    return true;
}

/**
 * @brief Window shutdown and resource cleanup
 */
void Window::Shutdown() {
    if (!m_data->IsInitialized) {
        return;
    }
    
    Logger::Info("Window", "Shutting down window");
    
    // Ensure render context is destroyed first
    if (m_data->IsRenderingContextCreated) {
        DestroyRenderContext();
    }
    
#ifdef ASTRAL_USE_SDL3
    // Clean up SDL3 resources
    if (m_data->SDLWindow) {
        SDL_DestroyWindow(m_data->SDLWindow);
        m_data->SDLWindow = nullptr;
    }
    
    // SDL3 subsystem shutdown
    SDL_Quit();
#endif
    
    // Reset all data to safe state
    m_data->IsInitialized = false;
    m_data->IsRenderingContextCreated = false;
    m_data->EventMgr = nullptr;
    
    Logger::Info("Window", "Window shutdown complete");
}

// **********************************************************************
// Window State Management Methods
// **********************************************************************

/**
 * @brief SDL3 event loop processing
 */
void Window::PollEvents() {
    if (!m_data->IsInitialized) {
        return;
    }
    
#ifdef ASTRAL_USE_SDL3
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        HandleWindowEvent(&event);
    }
#endif
}

/**
 * @brief Swap buffers and render presentation
 */
void Window::SwapBuffers() {
    if (!m_data->IsInitialized) {
        return;
    }
    
    // Note: For Vulkan-based rendering, traditional buffer swapping is not needed
    // Vulkan manages its own presentation through the swapchain
    // This method is kept for compatibility but will log a warning
    
    Logger::Warning("Window", "SwapBuffers called - Vulkan doesn't need traditional buffer swapping");
    Logger::Warning("Window", "Vulkan manages presentation through swapchain");
}

/**
 * @brief Shutdown check
 */
bool Window::ShouldClose() const {
    if (!m_data->IsInitialized) {
        return false;
    }
    
    return m_data->ShouldClose.load();
}

/**
 * @brief Programmatic shutdown request
 */
void Window::Close() {
    if (!m_data->IsInitialized) {
        return;
    }
    
    m_data->ShouldClose = true;
    
#ifdef ASTRAL_USE_SDL3
    SDL_HideWindow(m_data->SDLWindow);
#endif
    
    Logger::Info("Window", "Close request received");
}

// **********************************************************************
// Window Property Management Methods
// **********************************************************************

/**
 * @brief Tam ekran mod kontrolü
 */
void Window::SetFullscreen(bool enabled) {
    if (!m_data->IsInitialized) {
        return;
    }
    
    if (m_data->IsFullscreen == enabled) {
        return; // Zaten bu modda
    }
    
#ifdef ASTRAL_USE_SDL3
    if (enabled) {
        // Fullscreen: Tüm ekranı kaplama
        const SDL_DisplayMode* displayMode = SDL_GetDesktopDisplayMode(0);
        if (displayMode) {
            SDL_SetWindowFullscreen(m_data->SDLWindow, SDL_WINDOW_FULLSCREEN);
            SDL_SetWindowSize(m_data->SDLWindow, displayMode->w, displayMode->h);
        }
    } else {
        // Windowed-normal: Önceki boyutlara dön
        SDL_SetWindowFullscreen(m_data->SDLWindow, 0);
        SDL_SetWindowSize(m_data->SDLWindow, m_data->Width, m_data->Height);
    }
#endif
    
    m_data->IsFullscreen = enabled;
    Logger::Info("Window", "Fullscreen mode {}", enabled ? "enabled" : "disabled");
}

/**
 * @brief En-boy oranı hesaplama
 */
float Window::GetAspectRatio() const {
    if (m_data->Height == 0) {
        return 1.0f;
    }
    return static_cast<float>(m_data->Width) / static_cast<float>(m_data->Height);
}

/**
 * @brief Pencere başlığını değiştir
 */
void Window::SetTitle(const std::string& title) {
    m_data->Title = title;
    
#ifdef ASTRAL_USE_SDL3
    if (m_data->SDLWindow) {
        SDL_SetWindowTitle(m_data->SDLWindow, title.c_str());
    }
#endif
    
    Logger::Debug("Window", "Window title set to: '{}'", title);
}

/**
 * @brief Pencere boyutunu ayarla
 */
void Window::SetSize(int width, int height) {
    m_data->Width = width;
    m_data->Height = height;
    
#ifdef ASTRAL_USE_SDL3
    if (m_data->SDLWindow) {
        SDL_SetWindowSize(m_data->SDLWindow, width, height);
    }
#endif
    
    Logger::Debug("Window", "Window size set to: {}x{}", width, height);
}

/**
 * @brief VSync durumunu ayarla
 */
void Window::SetVSync(bool enabled) {
    m_data->VSync = enabled;
    
    Logger::Debug("Window", "VSync {}", enabled ? "enabled" : "disabled");
}

/**
 * @brief Odak durum kontrolü
 */
bool Window::IsFocused() const {
    if (!m_data->IsInitialized) {
        return false;
    }
    
#ifdef ASTRAL_USE_SDL3
    return SDL_GetWindowFlags(m_data->SDLWindow) & SDL_WINDOW_INPUT_FOCUS;
#endif
    return false;
}

/**
 * @brief Pencere öne getirme
 */
void Window::BringToFront() {
    if (!m_data->IsInitialized) {
        return;
    }
    
#ifdef ASTRAL_USE_SDL3
    SDL_RestoreWindow(m_data->SDLWindow);
    SDL_RaiseWindow(m_data->SDLWindow);
    SDL_ShowWindow(m_data->SDLWindow);
#endif
    
    Logger::Debug("Window", "Window brought to front");
}

/**
 * @brief Simge durumuna küçültme
 */
void Window::Minimize() {
    if (!m_data->IsInitialized) {
        return;
    }
    
#ifdef ASTRAL_USE_SDL3
    SDL_MinimizeWindow(m_data->SDLWindow);
#endif
    
    m_data->IsMinimized = true;
    Logger::Debug("Window", "Window minimized");
}

/**
 * @brief Ekrana kaplama
 */
void Window::Maximize() {
    if (!m_data->IsInitialized) {
        return;
    }
    
#ifdef ASTRAL_USE_SDL3
    SDL_MaximizeWindow(m_data->SDLWindow);
#endif
    
    m_data->IsMaximized = true;
    Logger::Debug("Window", "Window maximized");
}

//**********************************************************************
/** Graphics Context Management
************************************************************************/
bool Window::CreateRenderContext() {
    if (!m_data->IsInitialized || m_data->IsRenderingContextCreated) {
        return false;
    }
    
    // Note: For Vulkan-based rendering, we don't need to create a traditional render context
    // Vulkan manages its own device and context through the VulkanInstance and VulkanDevice
    // This method is kept for compatibility but will log a warning
    
    Logger::Warning("Window", "CreateRenderContext called - Vulkan doesn't need traditional render context");
    Logger::Warning("Window", "Use CreateVulkanSurface for Vulkan rendering instead");
    
    // Mark as created to avoid repeated warnings, but this is not actually used for Vulkan
    m_data->IsRenderingContextCreated = true;
    return true;
}

void Window::DestroyRenderContext() {
    if (!m_data->IsRenderingContextCreated) {
        return;
    }
    
    // Note: For Vulkan-based rendering, we don't need to destroy a traditional render context
    // Vulkan manages its own device and context cleanup
    Logger::Debug("Window", "DestroyRenderContext called - no action needed for Vulkan");
    
    m_data->IsRenderingContextCreated = false;
}

std::vector<const char*> Window::GetRequiredVulkanExtensions() const {
    std::vector<const char*> extensions;
    
#ifdef ASTRAL_USE_SDL3
    if (!m_data->IsInitialized || !m_data->SDLWindow) {
        Logger::Error("Window", "Window not initialized - cannot get Vulkan extensions");
        return extensions;
    }
    
    // Get required extensions from SDL3
    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    
    if (!sdlExtensions || extensionCount == 0) {
        Logger::Error("Window", "Failed to get Vulkan extensions from SDL3: {}", SDL_GetError());
        return extensions;
    }
    
    // Copy SDL3 extensions to our vector
    extensions.reserve(extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
        extensions.push_back(sdlExtensions[i]);
        Logger::Debug("Window", "Required Vulkan extension: {}", sdlExtensions[i]);
    }
    
    Logger::Info("Window", "Found {} required Vulkan extensions", extensions.size());
    
#else
    Logger::Warning("Window", "SDL3 not available - no Vulkan extensions will be added");
#endif
    
    return extensions;
}

bool Window::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const {
    if (!m_data->IsInitialized || !instance || !surface) {
        Logger::Error("Window", "Invalid parameters for CreateVulkanSurface");
        return false;
    }
    
#ifdef ASTRAL_USE_SDL3
    if (!m_data->SDLWindow) {
        Logger::Error("Window", "SDL window not initialized");
        return false;
    }
    
    // Use SDL3 to create Vulkan surface
    if (!SDL_Vulkan_CreateSurface(m_data->SDLWindow, instance, nullptr, surface)) {
        Logger::Error("Window", "Failed to create Vulkan surface: {}", SDL_GetError());
        return false;
    }
    
    Logger::Info("Window", "Vulkan surface created successfully");
    return true;
    
#else
    Logger::Error("Window", "SDL3 not available - cannot create Vulkan surface");
    return false;
#endif
}

// **********************************************************************
/** Private Methods and Helper Functions
***********************************************************************/

/**
 * @brief Internal window data synchronization
 */
void Window::UpdateCachedData() {
    if (!m_data->IsInitialized) {
        return;
    }
    
    // En-boy oranı hesaplama (sıfır bölme önleme)
    if (m_data->Height > 0) {
        m_data->AspectRatio = static_cast<float>(m_data->Width) / static_cast<float>(m_data->Height);
    } else {
        m_data->AspectRatio = 1.0f;
    }
    
#ifdef ASTRAL_USE_SDL3
    // Gerçek zamanlı boyut güncelleme
    SDL_GetWindowSize(m_data->SDLWindow, &m_data->Width, &m_data->Height);
#endif
    
    Logger::Debug("Window", "Cached data updated: {}x{}, Aspect: {:.3f}", 
                  m_data->Width, m_data->Height, m_data->AspectRatio);
}

/**
 * @brief Event manager setup
 */
void Window::SetEventManager(EventManager* eventMgr) {
    if (m_data) {
        m_data->EventMgr = eventMgr;
        Logger::Debug("Window", "Event manager set");
    }
}

/**
 * @brief Window lifecycle events handling
 */
void Window::HandleWindowLifecycleEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    auto& eventManager = EventManager::GetInstance();
    
    switch (sdlEvent->type) {
        case SDL_EVENT_QUIT:
            m_data->ShouldClose = true;
            eventManager.PublishEvent<WindowCloseEvent>();
            Logger::Info("Window", "Application quit requested");
            break;
            
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->ShouldClose = true;
                eventManager.PublishEvent<WindowCloseEvent>();
                Logger::Info("Window", "Window close requested");
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Window size and state change events handling
 */
void Window::HandleWindowResizeEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    auto& eventManager = EventManager::GetInstance();
    
    switch (sdlEvent->type) {
        case SDL_EVENT_WINDOW_RESIZED: {
            if (sdlEvent->window.windowID == m_data->WindowID) {
                int newWidth = static_cast<int>(sdlEvent->window.data1);
                int newHeight = static_cast<int>(sdlEvent->window.data2);
                
                // Size change validation
                if (newWidth > 0 && newHeight > 0 && 
                    (newWidth != m_data->Width || newHeight != m_data->Height)) {
                    
                    m_data->Width = newWidth;
                    m_data->Height = newHeight;
                    UpdateCachedData();
                    
                    // Send resize event to event system
                    eventManager.PublishEvent<WindowResizeEvent>(newWidth, newHeight);
                    
                    Logger::Info("Window", "Window resized to {}x{} (aspect: {:.3f})", 
                               newWidth, newHeight, m_data->AspectRatio);
                }
            }
            break;
        }
        
        case SDL_EVENT_WINDOW_MINIMIZED:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->IsMinimized = true;
                m_data->IsMaximized = false;
                Logger::Debug("Window", "Window minimized");
            }
            break;
            
        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->IsMaximized = true;
                m_data->IsMinimized = false;
                Logger::Debug("Window", "Window maximized");
            }
            break;
            
        case SDL_EVENT_WINDOW_RESTORED:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->IsMinimized = false;
                m_data->IsMaximized = false;
                Logger::Debug("Window", "Window restored");
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Keyboard events handling
 */
void Window::HandleKeyboardEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    auto& eventManager = EventManager::GetInstance();
    
    switch (sdlEvent->type) {
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode keycode = sdlEvent->key.key;
            KeyCode astralKey = SDLKeyToAstralKey(keycode);
            if (astralKey != KeyCode::Unknown) {
                eventManager.PublishEvent<KeyPressedEvent>(astralKey, sdlEvent->key.repeat != 0);
            }
            
            if (!sdlEvent->key.repeat) {
                Logger::Trace("Window", "Key pressed: {} (scancode: {})", 
                            SDL_GetKeyName(keycode), static_cast<int>(sdlEvent->key.scancode));
            }
            break;
        }
        
        case SDL_EVENT_KEY_UP: {
            SDL_Keycode keycode = sdlEvent->key.key;
            KeyCode astralKey = SDLKeyToAstralKey(keycode);
            if (astralKey != KeyCode::Unknown) {
                eventManager.PublishEvent<KeyReleasedEvent>(astralKey);
            }
            
            Logger::Trace("Window", "Key released: {}", SDL_GetKeyName(keycode));
            break;
        }
        
        default:
            break;
    }
}

/**
 * @brief Mouse events handling
 */
void Window::HandleMouseEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    auto& eventManager = EventManager::GetInstance();
    
    switch (sdlEvent->type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            uint8_t button = sdlEvent->button.button;
            eventManager.PublishEvent<MouseButtonPressedEvent>(static_cast<int>(button));
            
            Logger::Trace("Window", "Mouse button {} pressed at ({}, {})", 
                        button, sdlEvent->button.x, sdlEvent->button.y);
            break;
        }
        
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            uint8_t button = sdlEvent->button.button;
            eventManager.PublishEvent<MouseButtonReleasedEvent>(static_cast<int>(button));
            
            Logger::Trace("Window", "Mouse button {} released at ({}, {})", 
                        button, sdlEvent->button.x, sdlEvent->button.y);
            break;
        }
        
        case SDL_EVENT_MOUSE_MOTION: {
            float mouseX = sdlEvent->motion.x;
            float mouseY = sdlEvent->motion.y;
            eventManager.PublishEvent<MouseMovedEvent>(static_cast<int>(mouseX), static_cast<int>(mouseY));
            
            // Yüksek frekanslı olaylar için trace yerine debug
            Logger::Trace("Window", "Mouse moved to ({:.1f}, {:.1f}), delta: ({:.1f}, {:.1f})", 
                        mouseX, mouseY, sdlEvent->motion.xrel, sdlEvent->motion.yrel);
            break;
        }
        
        case SDL_EVENT_MOUSE_WHEEL: {
            float wheelY = sdlEvent->wheel.y;
            float wheelX = sdlEvent->wheel.x;
            
            // Mouse wheel için özel event (şimdilik KeyPressed olarak)
            // TODO: Gelecekte MouseWheelEvent oluşturulabilir
            
            Logger::Debug("Window", "Mouse wheel: Y={:.1f}, X={:.1f}", wheelY, wheelX);
            break;
        }
        
        default:
            break;
    }
}

/**
 * @brief Other important events handling
 */
void Window::HandleOtherEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    switch (sdlEvent->type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->IsFocused = true;
                Logger::Debug("Window", "Window gained focus");
            }
            break;
            
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (sdlEvent->window.windowID == m_data->WindowID) {
                m_data->IsFocused = false;
                Logger::Debug("Window", "Window lost focus");
            }
            break;
            
        case SDL_EVENT_DROP_FILE: {
            const char* droppedFile = sdlEvent->drop.data;
            if (droppedFile) {
                Logger::Info("Window", "File dropped: {}", droppedFile);
                // TODO: FileDropEvent eklenebilir
                SDL_free(const_cast<char*>(droppedFile));
            }
            break;
        }
        
        default:
            // Bilinmeyen veya işlenmeyen olaylar
            break;
    }
}

/**
 * @brief SDL3 event processing and event system integration
 * 
 * Converts SDL3 events to engine event system and routes them to appropriate components.
 * This method is called every frame and processes all window, keyboard and mouse events.
 */
void Window::HandleWindowEvent(const void* event) {
#ifdef ASTRAL_USE_SDL3
    if (!event || !m_data->IsInitialized) {
        return;
    }
    
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    
    // Event kategorilerine göre ayır
    switch (sdlEvent->type) {
        // Window lifecycle events
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            HandleWindowLifecycleEvent(sdlEvent);
            break;
            
        // Window size and state events
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            HandleWindowResizeEvent(sdlEvent);
            break;
            
        // Keyboard events
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            HandleKeyboardEvent(sdlEvent);
            break;
            
        // Mouse events
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
            HandleMouseEvent(sdlEvent);
            break;
            
        // Other events
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_DROP_FILE:
            HandleOtherEvent(sdlEvent);
            break;
            
        default:
            // Bilinmeyen veya işlenmeyen olaylar
            break;
    }
#endif
}

// Windows handle için özel implementasyon
#if defined(_WIN32) && defined(PLATFORM_WINDOWS)
void* Window::GetHWND() const {
#ifdef ASTRAL_USE_SDL3
    if (m_data->SDLWindow) {
        SDL_PropertiesID props = SDL_GetWindowProperties(m_data->SDLWindow);
        return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    }
#endif
    return nullptr;
}
#endif

void* Window::GetNativeHandle() const {
#ifdef ASTRAL_USE_SDL3
    if (m_data->SDLWindow) {
        SDL_PropertiesID props = SDL_GetWindowProperties(m_data->SDLWindow);
        
        // Platform specific handle alma
        #if defined(_WIN32)
            return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
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

#ifdef ASTRAL_USE_SDL3
SDL_WindowFlags Window::GetWindowFlags() const {
    if (m_data->SDLWindow) {
        SDL_WindowFlags flags = SDL_GetWindowFlags(m_data->SDLWindow);
        return flags;
    }
    return SDL_WINDOW_HIDDEN;
}

void Window::SetWindowFlags([[maybe_unused]] SDL_WindowFlags flags) {
    if (m_data->SDLWindow) {
        // SDL3 doesn't have SDL_SetWindowFlags, use individual property setters
        Logger::Warning("Window", "SetWindowFlags not implemented for SDL3");
    }
}
#endif

} // namespace AstralEngine
