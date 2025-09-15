# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Window Platform Abstraction Layer
#  *
#  * Header File: Window.h
#  * Purpose: SDL3 temelli platform soyutlaması ve pencere yönetimi
#  * Author: Astral Engine Development Team
#  * Version: 1.0.0
#  * Date: 2025-09-11
#  * Dependencies: SDL3, C++20
#  **********************************************************************/
#pragma once

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <string>
#include <memory>
#include <atomic>
#include <vector>

// SDL3 conditional includes
#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_version.h>
    #include <SDL3/SDL_video.h>
    #include <SDL3/SDL_vulkan.h>
#else
    // Placeholder header for SDL3-less builds
    namespace SDL {
        enum WindowFlags { WINDOW_HIDDEN = 0 };
        enum InitFlags { INIT_VIDEO = 0 };
        struct Window { void* hwnd; };
    }
#endif

// Vulkan headers (conditionally included)
#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

// Forward declaration
class EventManager;

/**
 * @class Window
 * @brief Platform bağımsız pencere soyutlama sınıfı
 * 
 * SDL3 kütüphanesi kullanılarak modern, performans odaklı bir 
 * pencere yönetim ve olay yakalama sistemi sağlar. C++20 
 * özellikleri ve modern C++ pratikleri kullanılarak 
 * geliştirilmiştir.
 * 
 * @headerfile Window.h
 * @version 1.0.0
 * @author Astral Engine Development Team
 * @date 2025-09-11
 * @copyright © 2025 Astral Engine Team
 */
class Window {
private:
    /**********************************************************************
     * Pencere Veri Yapısı
     **********************************************************************/
    struct WindowData {
        std::string Title;
        int Width = 0;
        int Height = 0;
        bool VSync = true;
        
        // Event yönetici referansı (dependency injection)
        EventManager* EventMgr = nullptr;
        
        // Platform pencere verileri
#ifdef ASTRAL_USE_SDL3
        SDL_Window* SDLWindow = nullptr;
        SDL_WindowID WindowID = 0;
        std::atomic_bool ShouldClose{false};
#else
        void* PlatformWindow = nullptr;
        std::atomic_bool ShouldClose{false};
#endif
        
        // Performans için önbelleklenmiş değerler
        float AspectRatio = 1.0f;
        bool IsFullscreen = false;
        bool IsMaximized = false;
        bool IsMinimized = false;
        bool IsFocused = false;
        
        // Debug ve izleme
        bool IsInitialized = false;
        bool IsRenderingContextCreated = false;
        
        // Event handler ID'leri için yönetim
        std::vector<size_t> EventHandlerIDs;
        
        // Thread güvenliği için mutex
        std::atomic_flag Mutex = ATOMIC_FLAG_INIT;
    };
    
public:
    /**
     * @brief Pencere oluşturma.
     * 
     * Pencereyi başlangıç durumunda oluşturur. Initialize() için hazırlık yapar.
     * Özel bellek yönetimi ve atomik operasyonlarla optimize edilmiştir.
     */
    Window();
    
    /**
     * @brief Pencere yok etme.
     * 
     * Kaynakları düzgün bir şekilde temizler. RAII pratiklerine uygun şekilde
     * çalışır. Destructorda her durumda kaynak serbest bırakılır.
     */
    ~Window();

    /**********************************************************************
     * Kopyalama ve Taşıma Operasyonları (Kurumsal Politika)
     **********************************************************************/
    // Kopyalama hassasiyeti - Kaynak çakışmalarını önlemek için
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    
    // Taşıma operasyonları (Yüksek performans için)
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    /**********************************************************************
     * Pencere Yaşam Döngüsü Yönetimi
     **********************************************************************/
    /**
     * @brief Pencere başlatma.
     * 
     * @param title Pencere başlığı
     * @param width Genişlik (piksel)
     * @param height Yükseklik (piksel)
     * @return bool Başarılı olursa true, değilse false
     * 
     * SDL3 ile modern pencere oluşturma. Güvenli hata yönetimi ile 
     * kaynak serbest bırakmayı garanti eder.
     */
    bool Initialize(const std::string& title, int width, int height);
    
    /**
     * @brief Pencere kapatma ve kaynakları temizleme.
     * 
     * Oluşturulan tüm SDL3 kaynaklarını güvenli bir şekilde temizler.
     * Hata durumlarında düzgün kaynak yönetimi sağlar.
     */
    void Shutdown();

    /**********************************************************************
     * Pencere Durum Yönetimi
     **********************************************************************/
    /**
     * @brief Pencere olaylarını işlemek için döngü.
     * 
     * SDL3 olay kuyruğunu okur ve uyumlu bir şekilde işler.
     * Performans için bloklama önleyici yapı.
     */
    void PollEvents();
    
    /**
     * @brief Grafik tamponları takas etme.
     * 
     * Render hedefinde çizilen içeriği ekrana yansıtır. 
     * VSync kontrolü ve performans optimizasyonu içerir.
     */
    void SwapBuffers();
    
    /**
     * @brief Pencerenin kapatılması gerekip gerekmediğini kontrol et.
     * 
     * @return bool Kapatılırsa true
     * 
     * Kullanıcı çıkış taleplerini veya sistem olaylarını yakalar.
     */
    bool ShouldClose() const;
    
    /**
     * @brief Pencereyi kapat istemiyi.
     * 
     * Bu yöntem programatik olarak pencere kapanış talebi
     * gönderir. Event system'de bu bir olay olarak yayınlanır.
     */
    void Close();

    /**********************************************************************
     * Pencere Özellik Yönetimi
     **********************************************************************/
    /**
     * @brief Pencere genişliğini al.
     * 
     * @return int Piksel cinsinden genişlik
     * 
     * Gerçek zamanlı pencere boyutunu döndürür.
     */
    inline int GetWidth() const { return m_data->Width; }
    
    /**
     * @brief Pencere yüksekliğini al.
     * 
     * @return int Piksel cinsinden yükseklik
     * 
     * Gerçek zamanlı pencere boyutunu döndürür.
     */
    inline int GetHeight() const { return m_data->Height; }
    
    /**
     * @brief En-boy oranını hesapla.
     * 
     * @return float En-boy oranı
     * 
     * Render ve matematiksel işlemler için kullanılır.
     */
    float GetAspectRatio() const;
    
    /**
     * @brief Pencere başlığını getir.
     * 
     * @return const std::string& Başlık metni
     */
    inline const std::string& GetTitle() const { return m_data->Title; }
    
    /**
     * @brief Pencere başlığını değiştir.
     * 
     * @param title Yeni başlık metni
     * 
     * Güvenli string manipülasyonu ile başlık değiştirme.
     */
    void SetTitle(const std::string& title);
    
    /**
     * @brief Pencere boyutunu ayarla.
     * 
     * @paramwidth Genişlik
     * @paramheight Yükseklik
     * 
     * Sistemin izin verdiği boyut sınırları içinde ayarlama yapar.
     */
    void SetSize(int width, int height);
    
    /**
     * @brief VSync durumunu ayarla.
     * 
     * @param enabled VSync aktifse true
     * 
     * Oynaklığı azaltmak için ekran senkronizasyonu.
     */
    void SetVSync(bool enabled);
    
    /**
     * @brief Tam ekran moduna geçiş yap.
     * 
     * @param enabled Tam ekran modu true ise
     * 
     * Performans odaklı tam ekran geçişi.
     */
    void SetFullscreen(bool enabled);
    
    /**
     * @brief Pencere odak durumunu kontrol et.
     * 
     * @return bool Odaklı ise true
     */
    bool IsFocused() const;
    
    /**
     * @brief Pencereyi öne getir.
     * 
     * Sistem için en üstte çalışma.
     */
    void BringToFront();
    
    /**
     * @brief Pencere simge durumuna küçült.
     * 
     * Sistem kaynaklarını optimize etme.
     */
    void Minimize();
    
    /**
     * @brief Pencereyi ekranı kaplat.
     * 
     * Ekrana tam kaplama.
     */
    void Maximize();

    /**********************************************************************
     * Platform Özel API'ler
     **********************************************************************/
    /**
     * @brief SDL3 pencere nesnesine erişim.
     * 
     * @return SDL_Window* SDL3 pencere referansı
     * 
     * Düşük seviye SDL3 operasyonları için erişim sağlar.
     * ASTRAL_USE_SDL3 tanımlı olmalı.
     */
#ifdef ASTRAL_USE_SDL3
    inline SDL_Window* GetSDLWindow() const { return m_data->SDLWindow; }
#else
    inline void* GetSDLWindow() const { return nullptr; }
#endif
    
    /**
     * @brief Platform özel pencere handle'ı.
     * 
     * @return void* Platform bağımsız handle
     * 
     * OpenGL, Vulkan veya diğer API'ler için gereken handle.
     */
    void* GetNativeHandle() const;
    
    /**
     * @brief Windows API handle'ý alýnýr.
     * 
     * @return void* Windows pencere handle'ý
     * 
     * Windows özel fonksiyonlar için.
     */
#ifdef _WIN32
#ifdef PLATFORM_WINDOWS
    void* GetHWND() const;
#endif
#else
    void* GetNativeWindow() const;
#endif
    
    /**
     * @brief Pencere özelliklerini getir.
     * 
     * @return SDL_WindowFlags SDL3 pencere flag'lari
     * 
     * Düşük seviye özellik kontrolü.
     */
#ifdef ASTRAL_USE_SDL3
    SDL_WindowFlags GetWindowFlags() const;
#endif
    
    /**
     * @brief Pencere özelliklerini ayarla.
     * 
     * @paramFlags SDL3 pencere flaglari
     * 
     * Düşük seviye özellik ayarlama.
     */
#ifdef ASTRAL_USE_SDL3
    void SetWindowFlags(SDL_WindowFlags flags);
#endif
    
    /**
     * @brief Grafik context oluşturma.
     * 
     * @return bool Baýýrýlýrsa true
     * 
     * OpenGL/Vulkan context hazýrlama.
     */
    bool CreateRenderContext();
    
    /**
     * @brief Grafik context yok etme.
     * 
     * Kaynak serbest býrakma.
     */
    void DestroyRenderContext();
    
    /**
     * @brief Vulkan surface oluşturma.
     * 
     * @param instance Vulkan instance handle'ı
     * @param surface Oluşturulan surface'in pointer'ı
     * @return bool Başarılı olursa true
     * 
     * Vulkan render için pencere surface'i oluşturur.
     */
    bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface);

private:
    /**********************************************************************
     * Özel metodlar için bloklama
     **********************************************************************/
    friend class PlatformSubsystem;
    friend class Engine;
    
    /**
     * @brief Pencere olayları özel işleme.
     * 
     * @param event SDL3 olay referansý
     * 
     * Olay sistemine değerli olaylarýnýný gönderþi.
     */
    void HandleWindowEvent(const void* event);
    
    /**
     * @brief EventManager ayarlama.
     * 
     * @paramEventManager Olay yönetici referansý
     * 
     * Olay sistemine baðlama için.
     */
    void SetEventManager(EventManager* eventMgr);
    
    /**
     * @brief Pencere veri yapýsýný güncelleme.
     * 
     * Değiþen özellikler için iç durum güncelleþme.
     */
    void UpdateCachedData();
    
    /**
     * @brief Window lifecycle events handling
     */
    void HandleWindowLifecycleEvent(const void* event);
    
    /**
     * @brief Window size and state change events handling
     */
    void HandleWindowResizeEvent(const void* event);
    
    /**
     * @brief Keyboard events handling
     */
    void HandleKeyboardEvent(const void* event);
    
    /**
     * @brief Mouse events handling
     */
    void HandleMouseEvent(const void* event);
    
    /**
     * @brief Other important events handling
     */
    void HandleOtherEvent(const void* event);
    
    /**********************************************************************
     * Üye değişkenler
     **********************************************************************/
    // Window verileriniSmart pointer ile yönetim
    std::unique_ptr<WindowData> m_data;
};

// Windows API için typedef kullanýlýnýr
#ifdef _WIN32
using HWND = void*;
#endif

} // namespace AstralEngine
