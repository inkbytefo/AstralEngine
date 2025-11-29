# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Window Platform Abstraction Layer
#  *
#  * Implementation File: Window.cpp
#  * Purpose: Refactored SDL3/Vulkan window management implementation.
#  * Author: Jules (AI Assistant)
#  * Version: 2.0.0
#  * Date: 2025-09-15
#  * Dependencies: SDL3, Vulkan, C++20, Logger, EventManager
#  **********************************************************************/
#include "Window.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"

#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_vulkan.h>
#endif

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Logger::Error("Window", "SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    
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
    
#else
    Logger::Warning("Window", "SDL3 not available - using placeholder implementation");
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
            break;
        case SDL_EVENT_KEY_DOWN:
            eventManager.PublishEvent<KeyPressedEvent>(sdlEvent->key.key, sdlEvent->key.repeat);
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
            eventManager.PublishEvent<MouseMovedEvent>(static_cast<int>(sdlEvent->motion.x), static_cast<int>(sdlEvent->motion.y));
            break;
    }
#endif
}

void Window::SwapBuffers() {
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
}

void Window::SetSize(int width, int height) {
    m_width = width;
    m_height = height;
#ifdef ASTRAL_USE_SDL3
    if (m_sdlWindow) {
        SDL_SetWindowSize(m_sdlWindow, width, height);
    }
#endif
}

void* Window::GetNativeHandle() const {
    Logger::Warning("Window", "GetNativeHandle is not implemented due to missing SDL_syswm.h in the provided SDL3 library.");
    return nullptr;
}

#if defined(ASTRAL_USE_SDL3) && defined(ASTRAL_USE_VULKAN)

std::vector<const char*> Window::GetRequiredVulkanExtensions() const {
    if (!m_sdlWindow) {
        Logger::Error("Window", "Cannot get Vulkan extensions: window not initialized");
        return {};
    }
    
    unsigned int count;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    
    if (!extensions) {
        Logger::Error("Window", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }
    
    std::vector<const char*> result(extensions, extensions + count);

    #ifdef ASTRAL_DEBUG
        result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

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

}
