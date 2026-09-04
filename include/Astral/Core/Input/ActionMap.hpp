#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <cstdint>

namespace Astral {

class InputSystem;

enum class ActionType : uint8_t {
    Digital, // Acik / Kapali (Buton / Tus)
    Axis1D,  // -1.0 ile +1.0 arasi tek eksen
    Axis2D   // 2D yon vektoru (X, Y)
};

struct KeyBinding {
    int key = -1;
    float scale = 1.0f;
};

struct MouseButtonBinding {
    int button = -1;
};

struct MouseAxis2DBinding {
    float sensitivityX = 1.0f;
    float sensitivityY = 1.0f;
    bool invertY = false;
};

struct Axis2DCompositeBinding {
    int upKey = -1;
    int downKey = -1;
    int leftKey = -1;
    int rightKey = -1;
};

/// Tek bir semantik eylem (or. "Jump", "MoveForward", "Look")
struct InputAction {
    std::string name;
    ActionType type = ActionType::Digital;

    // Baglanti listeleri
    std::vector<KeyBinding> keyBindings;
    std::vector<MouseButtonBinding> mouseButtonBindings;
    std::vector<Axis2DCompositeBinding> composite2DBindings;
    std::vector<MouseAxis2DBinding> mouseAxisBindings;

    // Hesaplanan durum
    bool active = false;
    bool previousActive = false;
    bool justPressed = false;
    bool justReleased = false;

    float axis1D = 0.0f;
    glm::vec2 axis2D{0.0f};
};

/// Girdi Eylem Haritasi (Action Mapping Context)
/// Unreal Engine Enhanced Input ve Godot InputMap mantigiyla ham tuslari semantik eylemlere esler.
class ActionMap {
public:
    ActionMap();
    ~ActionMap() = default;

    ActionMap(const ActionMap&) = default;
    ActionMap& operator=(const ActionMap&) = default;

    ActionMap(ActionMap&&) noexcept = default;
    ActionMap& operator=(ActionMap&&) noexcept = default;

    // ---- Harita Tanımlama API'si ----

    /// Dijital eylem (Buton / Tus) ekler (or. "Jump" -> GLFW_KEY_SPACE)
    void MapAction(std::string_view name, int key);

    /// Fare butonu ile dijital eylem ekler (or. "Fire" -> GLFW_MOUSE_BUTTON_LEFT)
    void MapMouseButtonAction(std::string_view name, int mouseButton);

    /// 1D Eksen eylemi ekler (or. "MoveForward" -> Pozitif: W (+1.0), Negatif: S (-1.0))
    void MapAxis1D(std::string_view name, int positiveKey, int negativeKey);

    /// 2D Eksen eylemi ekler (or. "Move" -> Yukari: W, Asagi: S, Sol: A, Sag: D)
    void MapAxis2D(std::string_view name, int upKey, int downKey, int leftKey, int rightKey);

    /// Fare hareketi (Delta) ile 2D Eksen eylemi ekler (or. "Look" -> Mouse Delta)
    void MapMouseAxis2D(std::string_view name, float sensitivity = 1.0f, bool invertY = false);

    /// Eylemin belirli bir tus atamasini gunceller (Rebinding)
    bool RebindKey(std::string_view actionName, int oldKey, int newKey);

    /// Eylemin tum baglantilarini temizler
    void ClearBindings(std::string_view actionName);

    // ---- Kare Guncellemesi ----

    /// Her karenin basinda InputSystem'den alinan ham snapshot ile eylemleri hesaplar
    void Update(const InputSystem& rawInput);

    // ---- Sorgulama API'si ----

    [[nodiscard]] bool IsActionActive(std::string_view name) const;
    [[nodiscard]] bool IsActionJustPressed(std::string_view name) const;
    [[nodiscard]] bool IsActionJustReleased(std::string_view name) const;

    [[nodiscard]] float GetAxis1D(std::string_view name) const;
    [[nodiscard]] glm::vec2 GetAxis2D(std::string_view name) const;

    // ---- Katmanli Baglamlar (Contexts) ----

    void PushContext(std::string_view contextName);
    void PopContext();
    [[nodiscard]] std::string_view GetCurrentContext() const;

private:
    struct ContextData {
        std::unordered_map<std::string, InputAction> actions;
    };

    std::unordered_map<std::string, ContextData> m_Contexts;
    std::vector<std::string> m_ContextStack;

    [[nodiscard]] ContextData& GetActiveContextData();
    [[nodiscard]] const ContextData& GetActiveContextData() const;
    [[nodiscard]] const InputAction* FindAction(std::string_view name) const;
    [[nodiscard]] InputAction* FindActionMut(std::string_view name);
};

} // namespace Astral
