# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Window Platform Abstraction Layer
#  *
#  * Header File: Window.h
#  * Purpose: Refactored SDL3/Vulkan window management.
#  * Author: Jules (AI Assistant)
#  * Version: 2.0.0
#  * Date: 2025-09-15
#  * Dependencies: SDL3, Vulkan, C++20
#  **********************************************************************/
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
    void SwapBuffers(); // Note: Behavior is different for Vulkan
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

    // Vulkan-specific methods required by the integration guides
#if defined(ASTRAL_USE_SDL3) && defined(ASTRAL_USE_VULKAN)
    std::vector<const char*> GetRequiredVulkanExtensions() const;
    bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const;
#endif

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
    bool m_initialized = false;
};

} // namespace AstralEngine
