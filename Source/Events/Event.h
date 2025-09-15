#pragma once

#include <string>
#include <memory>
#include <typeindex>

namespace AstralEngine {

/**
 * @brief Tüm olaylar için temel sınıf.
 * 
 * Motor genelindeki asenkron iletişim için event system'in temelini oluşturur.
 * Her olay türü bu sınıftan türemelidir.
 */
class Event {
public:
    virtual ~Event() = default;
    
    // Olay türü bilgisi
    virtual const char* GetName() const = 0;
    virtual std::type_index GetType() const = 0;
    
    // Olay işlendi mi?
    bool IsHandled() const { return m_handled; }
    void SetHandled(bool handled = true) { m_handled = handled; }
    
    // Olay kategorileri (bitwise operations için)
    enum Category {
        None        = 0,
        Application = 1 << 0,
        Input       = 1 << 1, 
        Keyboard    = 1 << 2,
        Mouse       = 1 << 3,
        Window      = 1 << 4,
        Renderer    = 1 << 5,
        Asset       = 1 << 6
    };
    
    virtual int GetCategoryFlags() const = 0;
    bool IsInCategory(Category category) const {
        return GetCategoryFlags() & category;
    }

private:
    bool m_handled = false;
};

// Event türü için makro yardımcısı
#define EVENT_CLASS_TYPE(type) \
    static std::type_index GetStaticType() { return typeid(type); } \
    virtual std::type_index GetType() const override { return GetStaticType(); } \
    virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
    virtual int GetCategoryFlags() const override { return category; }

// Yaygın olay türleri

/**
 * @brief Pencere yeniden boyutlandırma olayı
 */
class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(int width, int height) 
        : m_width(width), m_height(height) {}
    
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    
    EVENT_CLASS_TYPE(WindowResizeEvent)
    EVENT_CLASS_CATEGORY(Window | Application)

private:
    int m_width, m_height;
};

/**
 * @brief Pencere kapatma olayı
 */
class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;
    
    EVENT_CLASS_TYPE(WindowCloseEvent)
    EVENT_CLASS_CATEGORY(Window | Application)
};

/**
 * @brief Klavye tuşuna basma olayı
 */
class KeyPressedEvent : public Event {
public:
    KeyPressedEvent(int keyCode, bool isRepeat = false)
        : m_keyCode(keyCode), m_isRepeat(isRepeat) {}
    
    int GetKeyCode() const { return m_keyCode; }
    bool IsRepeat() const { return m_isRepeat; }
    
    EVENT_CLASS_TYPE(KeyPressedEvent)
    EVENT_CLASS_CATEGORY(Keyboard | Input)

private:
    int m_keyCode;
    bool m_isRepeat;
};

/**
 * @brief Klavye tuşu bırakma olayı
 */
class KeyReleasedEvent : public Event {
public:
    KeyReleasedEvent(int keyCode) : m_keyCode(keyCode) {}
    
    int GetKeyCode() const { return m_keyCode; }
    
    EVENT_CLASS_TYPE(KeyReleasedEvent)
    EVENT_CLASS_CATEGORY(Keyboard | Input)

private:
    int m_keyCode;
};

/**
 * @brief Mouse düğmesi basma olayı
 */
class MouseButtonPressedEvent : public Event {
public:
    MouseButtonPressedEvent(int button) : m_button(button) {}
    
    int GetMouseButton() const { return m_button; }
    
    EVENT_CLASS_TYPE(MouseButtonPressedEvent)
    EVENT_CLASS_CATEGORY(Mouse | Input)

private:
    int m_button;
};

/**
 * @brief Mouse düğmesi bırakma olayı
 */
class MouseButtonReleasedEvent : public Event {
public:
    MouseButtonReleasedEvent(int button) : m_button(button) {}
    
    int GetMouseButton() const { return m_button; }
    
    EVENT_CLASS_TYPE(MouseButtonReleasedEvent)
    EVENT_CLASS_CATEGORY(Mouse | Input)

private:
    int m_button;
};

/**
 * @brief Mouse hareket olayı
 */
class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(int x, int y) : m_mouseX(x), m_mouseY(y) {}
    
    int GetX() const { return m_mouseX; }
    int GetY() const { return m_mouseY; }
    
    EVENT_CLASS_TYPE(MouseMovedEvent)
    EVENT_CLASS_CATEGORY(Mouse | Input)

private:
    int m_mouseX, m_mouseY;
};

/**
 * @brief Asset yükleme tamamlanma olayı
 */
class AssetLoadedEvent : public Event {
public:
    AssetLoadedEvent(const std::string& assetPath) : m_assetPath(assetPath) {}
    
    const std::string& GetAssetPath() const { return m_assetPath; }
    
    EVENT_CLASS_TYPE(AssetLoadedEvent)
    EVENT_CLASS_CATEGORY(Asset)

private:
    std::string m_assetPath;
};

} // namespace AstralEngine
