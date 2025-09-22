#pragma once

namespace AstralEngine {

// Desteklenen tuş kodları (SDL3 keycodes ile eşleşecek)
enum class KeyCode {
    Unknown = 0,
    
    // Letters (SDL3 uses uppercase for letter keys)
    A = 'A',
    B = 'B',
    C = 'C',
    D = 'D',
    E = 'E',
    F = 'F',
    G = 'G',
    H = 'H',
    I = 'I',
    J = 'J',
    K = 'K',
    L = 'L',
    M = 'M',
    N = 'N',
    O = 'O',
    P = 'P',
    Q = 'Q',
    R = 'R',
    S = 'S',
    T = 'T',
    U = 'U',
    V = 'V',
    W = 'W',
    X = 'X',
    Y = 'Y',
    Z = 'Z',
    
    // Numbers
    Number1 = '1',
    Number2 = '2',
    Number3 = '3',
    Number4 = '4',
    Number5 = '5',
    Number6 = '6',
    Number7 = '7',
    Number8 = '8',
    Number9 = '9',
    Number0 = '0',
    
    // Special keys
    Return = 0x0D,
    Escape = 0x1B,
    Backspace = 0x08,
    Tab = 0x09,
    Space = 0x20,
    
    // Arrow keys (SDL3 compatible values)
    Right = 0x4000004F,
    Left = 0x40000050,
    Down = 0x40000051,
    Up = 0x40000052,
    
    // Function keys (SDL3 compatible values)
    F1 = 0x4000003A,
    F2 = 0x4000003B,
    F3 = 0x4000003C,
    F4 = 0x4000003D,
    F5 = 0x4000003E,
    F6 = 0x4000003F,
    F7 = 0x40000040,
    F8 = 0x40000041,
    F9 = 0x40000042,
    F10 = 0x40000043,
    F11 = 0x40000044,
    F12 = 0x40000045,
    
    // Modifier keys (SDL3 compatible values)
    LeftCtrl = 0x400000E0,
    LeftShift = 0x400000E1,
    LeftAlt = 0x400000E2,
    RightCtrl = 0x400000E3,
    RightShift = 0x400000E4,
    RightAlt = 0x400000E5,
    
    // Sayının maksimum değeri
    MAX_KEYS = 512
};

// Mouse butonları
enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    
    MAX_BUTTONS = 8
};

    // Equality operators for KeyCode
    inline bool operator==(KeyCode lhs, KeyCode rhs) {
        return static_cast<int>(lhs) == static_cast<int>(rhs);
    }

    inline bool operator!=(KeyCode lhs, KeyCode rhs) {
        return !(lhs == rhs);
    }

} // namespace AstralEngine
