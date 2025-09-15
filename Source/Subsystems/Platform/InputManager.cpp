#include "InputManager.h"
#include "Window.h"
#include "../../Core/Logger.h"

// SDL3 key mapping
#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
#endif

namespace AstralEngine {

#ifdef ASTRAL_USE_SDL3
/**
 * @brief SDL3 keycodes'ları Astral Engine KeyCode'larına çevirir
 * 
 * @param sdlKey SDL3 keycode değeri
 * @return KeyCode Karşılık gelen engine keycode'u
 */
KeyCode SDLKeyToAstralKey(SDL_Keycode sdlKey) {
    switch (sdlKey) {
        // Letters
        case SDLK_A: return KeyCode::A;
        case SDLK_B: return KeyCode::B;
        case SDLK_C: return KeyCode::C;
        case SDLK_D: return KeyCode::D;
        case SDLK_E: return KeyCode::E;
        case SDLK_F: return KeyCode::F;
        case SDLK_G: return KeyCode::G;
        case SDLK_H: return KeyCode::H;
        case SDLK_I: return KeyCode::I;
        case SDLK_J: return KeyCode::J;
        case SDLK_K: return KeyCode::K;
        case SDLK_L: return KeyCode::L;
        case SDLK_M: return KeyCode::M;
        case SDLK_N: return KeyCode::N;
        case SDLK_O: return KeyCode::O;
        case SDLK_P: return KeyCode::P;
        case SDLK_Q: return KeyCode::Q;
        case SDLK_R: return KeyCode::R;
        case SDLK_S: return KeyCode::S;
        case SDLK_T: return KeyCode::T;
        case SDLK_U: return KeyCode::U;
        case SDLK_V: return KeyCode::V;
        case SDLK_W: return KeyCode::W;
        case SDLK_X: return KeyCode::X;
        case SDLK_Y: return KeyCode::Y;
        case SDLK_Z: return KeyCode::Z;
        
        // Numbers
        case SDLK_1: return KeyCode::Number1;
        case SDLK_2: return KeyCode::Number2;
        case SDLK_3: return KeyCode::Number3;
        case SDLK_4: return KeyCode::Number4;
        case SDLK_5: return KeyCode::Number5;
        case SDLK_6: return KeyCode::Number6;
        case SDLK_7: return KeyCode::Number7;
        case SDLK_8: return KeyCode::Number8;
        case SDLK_9: return KeyCode::Number9;
        case SDLK_0: return KeyCode::Number0;
        
        // Special keys
        case SDLK_RETURN: return KeyCode::Return;
        case SDLK_ESCAPE: return KeyCode::Escape;
        case SDLK_BACKSPACE: return KeyCode::Backspace;
        case SDLK_TAB: return KeyCode::Tab;
        case SDLK_SPACE: return KeyCode::Space;
        
        // Arrow keys
        case SDLK_RIGHT: return KeyCode::Right;
        case SDLK_LEFT: return KeyCode::Left;
        case SDLK_DOWN: return KeyCode::Down;
        case SDLK_UP: return KeyCode::Up;
        
        // Function keys
        case SDLK_F1: return KeyCode::F1;
        case SDLK_F2: return KeyCode::F2;
        case SDLK_F3: return KeyCode::F3;
        case SDLK_F4: return KeyCode::F4;
        case SDLK_F5: return KeyCode::F5;
        case SDLK_F6: return KeyCode::F6;
        case SDLK_F7: return KeyCode::F7;
        case SDLK_F8: return KeyCode::F8;
        case SDLK_F9: return KeyCode::F9;
        case SDLK_F10: return KeyCode::F10;
        case SDLK_F11: return KeyCode::F11;
        case SDLK_F12: return KeyCode::F12;
        
        // Modifier keys
        case SDLK_LCTRL: return KeyCode::LeftCtrl;
        case SDLK_LSHIFT: return KeyCode::LeftShift;
        case SDLK_LALT: return KeyCode::LeftAlt;
        case SDLK_RCTRL: return KeyCode::RightCtrl;
        case SDLK_RSHIFT: return KeyCode::RightShift;
        case SDLK_RALT: return KeyCode::RightAlt;
        
        default:
            return KeyCode::Unknown;
    }
}

/**
 * @brief SDL3 mouse butonlarını Astral Engine MouseButton'larına çevirir
 * 
 * @param sdlButton SDL3 mouse button değeri
 * @return MouseButton Karşılık gelen engine mouse button'u
 */
MouseButton SDLButtonToAstralButton(uint8_t sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return MouseButton::Left; // Güvenli fallback
    }
}
#endif

InputManager::InputManager() {
    Logger::Debug("InputManager", "InputManager created");
}

InputManager::~InputManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("InputManager", "InputManager destroyed");
}

bool InputManager::Initialize(Window* window) {
    if (m_initialized) {
        Logger::Warning("InputManager", "InputManager already initialized");
        return true;
    }
    
    if (!window) {
        Logger::Error("InputManager", "Cannot initialize without a valid window");
        return false;
    }
    
    m_window = window;
    
    // Durum bitsetlerini sıfırla
    m_keyboardState.reset();
    m_keyboardStatePrevious.reset();
    m_mouseState.reset();
    m_mouseStatePrevious.reset();
    
    m_initialized = true;
    Logger::Info("InputManager", "InputManager initialized successfully");
    
    return true;
}

void InputManager::Update() {
    if (!m_initialized) {
        return;
    }
    
    // Önceki frame'in durumunu kaydet
    m_keyboardStatePrevious = m_keyboardState;
    m_mouseStatePrevious = m_mouseState;
    
    // Frame bazlı inputları sıfırla
    ResetFrameInputs();
}

void InputManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    m_window = nullptr;
    m_initialized = false;
    
    Logger::Info("InputManager", "InputManager shutdown complete");
}

bool InputManager::IsKeyPressed(KeyCode key) const {
    if (key >= KeyCode::MAX_KEYS) {
        return false;
    }
    
    return m_keyboardState[static_cast<size_t>(key)];
}

bool InputManager::IsKeyJustPressed(KeyCode key) const {
    if (key >= KeyCode::MAX_KEYS) {
        return false;
    }
    
    size_t index = static_cast<size_t>(key);
    return m_keyboardState[index] && !m_keyboardStatePrevious[index];
}

bool InputManager::IsKeyJustReleased(KeyCode key) const {
    if (key >= KeyCode::MAX_KEYS) {
        return false;
    }
    
    size_t index = static_cast<size_t>(key);
    return !m_keyboardState[index] && m_keyboardStatePrevious[index];
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const {
    if (button >= MouseButton::MAX_BUTTONS) {
        return false;
    }
    
    return m_mouseState[static_cast<size_t>(button)];
}

bool InputManager::IsMouseButtonJustPressed(MouseButton button) const {
    if (button >= MouseButton::MAX_BUTTONS) {
        return false;
    }
    
    size_t index = static_cast<size_t>(button);
    return m_mouseState[index] && !m_mouseStatePrevious[index];
}

bool InputManager::IsMouseButtonJustReleased(MouseButton button) const {
    if (button >= MouseButton::MAX_BUTTONS) {
        return false;
    }
    
    size_t index = static_cast<size_t>(button);
    return !m_mouseState[index] && m_mouseStatePrevious[index];
}

void InputManager::GetMousePosition(int& x, int& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

void InputManager::GetMouseDelta(int& dx, int& dy) const {
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}

void InputManager::OnKeyEvent(KeyCode key, bool pressed) {
    if (key >= KeyCode::MAX_KEYS || key == KeyCode::Unknown) {
        return;
    }
    
    size_t keyIndex = static_cast<size_t>(key);
    bool wasPressed = m_keyboardState[keyIndex];
    m_keyboardState[keyIndex] = pressed;
    
    // Log state changes only
    if (wasPressed != pressed) {
        Logger::Trace("InputManager", "Key {} {} (code: {})", 
                    static_cast<int>(key), 
                    pressed ? "pressed" : "released",
                    keyIndex);
    }
}

/**
 * @brief SDL3 key event'ini engine key event'ine çevirir ve işler
 */
void InputManager::HandleSDLKeyEvent(int sdlKeycode, bool pressed) {
#ifdef ASTRAL_USE_SDL3
    KeyCode engineKey = SDLKeyToAstralKey(static_cast<SDL_Keycode>(sdlKeycode));
    if (engineKey != KeyCode::Unknown) {
        OnKeyEvent(engineKey, pressed);
    }
#endif
}

void InputManager::OnMouseButtonEvent(MouseButton button, bool pressed) {
    if (button >= MouseButton::MAX_BUTTONS) {
        return;
    }
    
    size_t buttonIndex = static_cast<size_t>(button);
    bool wasPressed = m_mouseState[buttonIndex];
    m_mouseState[buttonIndex] = pressed;
    
    // Log state changes only
    if (wasPressed != pressed) {
        Logger::Trace("InputManager", "Mouse button {} {} at ({}, {})", 
                    static_cast<int>(button), 
                    pressed ? "pressed" : "released",
                    m_mouseX, m_mouseY);
    }
}

/**
 * @brief SDL3 mouse button event'ini engine mouse event'ine çevirir ve işler
 */
void InputManager::HandleSDLMouseButtonEvent(uint8_t sdlButton, bool pressed, float x, float y) {
#ifdef ASTRAL_USE_SDL3
    MouseButton engineButton = SDLButtonToAstralButton(sdlButton);
    
    // Mouse pozisyonunu güncelle
    OnMouseMoveEvent(static_cast<int>(x), static_cast<int>(y));
    
    // Button event'ini işle
    OnMouseButtonEvent(engineButton, pressed);
#endif
}

/**
 * @brief Doğrudan SDL3 event işleme (Window tarafından çağrılır)
 * 
 * Bu metod, Window sınıfından gelen SDL3 event'lerini doğrudan işler.
 * Event system üzerinden dolaylı routing yerine doğrudan state güncellemesi yapar.
 */
void InputManager::ProcessSDLEvent(const void* sdlEvent) {
#ifdef ASTRAL_USE_SDL3
    if (!sdlEvent || !m_initialized) {
        return;
    }
    
    const SDL_Event* event = static_cast<const SDL_Event*>(sdlEvent);
    
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode keycode = event->key.key;
            KeyCode engineKey = SDLKeyToAstralKey(keycode);
            if (engineKey != KeyCode::Unknown) {
                OnKeyEvent(engineKey, true);
            }
            break;
        }
        
        case SDL_EVENT_KEY_UP: {
            SDL_Keycode keycode = event->key.key;
            KeyCode engineKey = SDLKeyToAstralKey(keycode);
            if (engineKey != KeyCode::Unknown) {
                OnKeyEvent(engineKey, false);
            }
            break;
        }
        
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            uint8_t button = event->button.button;
            MouseButton engineButton = SDLButtonToAstralButton(button);
            OnMouseButtonEvent(engineButton, true);
            break;
        }
        
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            uint8_t button = event->button.button;
            MouseButton engineButton = SDLButtonToAstralButton(button);
            OnMouseButtonEvent(engineButton, false);
            break;
        }
        
        case SDL_EVENT_MOUSE_MOTION: {
            int x = static_cast<int>(event->motion.x);
            int y = static_cast<int>(event->motion.y);
            OnMouseMoveEvent(x, y);
            break;
        }
        
        case SDL_EVENT_MOUSE_WHEEL: {
            int delta = static_cast<int>(event->wheel.y);
            OnMouseWheelEvent(delta);
            break;
        }
        
        default:
            // İşlenmeyen event'leri görmezden gel
            break;
    }
#endif
}

void InputManager::OnMouseMoveEvent(int x, int y) {
    m_mouseDeltaX = x - m_mouseX;
    m_mouseDeltaY = y - m_mouseY;
    m_mouseX = x;
    m_mouseY = y;
    
    Logger::Trace("InputManager", "Mouse moved to ({}, {}), delta: ({}, {})", 
                  x, y, m_mouseDeltaX, m_mouseDeltaY);
}

void InputManager::OnMouseWheelEvent(int delta) {
    m_mouseWheelDelta = delta;
    
    Logger::Trace("InputManager", "Mouse wheel delta: {}", delta);
}

void InputManager::ResetFrameInputs() {
    // Mouse delta ve wheel her frame sıfırlanmalı
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
    m_mouseWheelDelta = 0;
}

} // namespace AstralEngine
