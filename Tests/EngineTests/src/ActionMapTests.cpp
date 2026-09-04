#include "TestFramework.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/Input/ActionMap.hpp"
#include <cmath>

namespace Astral::Test {

void RunActionMapTests() {
    const std::string suite = "ActionMapEnhancedInputSuite";

    InputSystem rawInput;
    ActionMap actionMap;

    // 1. Dijital Eylem Haritalama (Digital Action Mapping: "Jump" -> Space)
    actionMap.MapAction("Jump", GLFW_KEY_SPACE);

    // Frame 1: Hicbir tusa basilmadi
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "InitialActionNotActive", !actionMap.IsActionActive("Jump"));
    TEST_CHECK(suite, "InitialActionNotJustPressed", !actionMap.IsActionJustPressed("Jump"));

    // Frame 2: Space basildi
    rawInput.SimulateKey(GLFW_KEY_SPACE, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ActionActiveWhenPressed", actionMap.IsActionActive("Jump"));
    TEST_CHECK(suite, "ActionJustPressedEdgeTriggered", actionMap.IsActionJustPressed("Jump"));
    TEST_CHECK(suite, "ActionNotJustReleased", !actionMap.IsActionJustReleased("Jump"));

    // Frame 3: Space basili tutulmaya devam ediyor
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ActionStillActiveWhenHeld", actionMap.IsActionActive("Jump"));
    TEST_CHECK(suite, "ActionNotJustPressedWhenHeld", !actionMap.IsActionJustPressed("Jump"));

    // Frame 4: Space birakildi
    rawInput.SimulateKey(GLFW_KEY_SPACE, GLFW_RELEASE);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ActionInactiveWhenReleased", !actionMap.IsActionActive("Jump"));
    TEST_CHECK(suite, "ActionJustReleasedTriggered", actionMap.IsActionJustReleased("Jump"));

    // 2. 1D Eksen Haritalama (Axis 1D: "Throttle" -> W (+1), S (-1))
    actionMap.MapAxis1D("Throttle", GLFW_KEY_W, GLFW_KEY_S);

    // W basildi -> +1.0
    rawInput.SimulateKey(GLFW_KEY_W, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ThrottlePositiveValue", std::abs(actionMap.GetAxis1D("Throttle") - 1.0f) < 0.001f);
    TEST_CHECK(suite, "ThrottleActionActive", actionMap.IsActionActive("Throttle"));

    // W birakildi, S basildi -> -1.0
    rawInput.SimulateKey(GLFW_KEY_W, GLFW_RELEASE);
    rawInput.SimulateKey(GLFW_KEY_S, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ThrottleNegativeValue", std::abs(actionMap.GetAxis1D("Throttle") - (-1.0f)) < 0.001f);

    // S birakildi -> 0.0
    rawInput.SimulateKey(GLFW_KEY_S, GLFW_RELEASE);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ThrottleZeroWhenNeutral", std::abs(actionMap.GetAxis1D("Throttle")) < 0.001f);

    // 3. 2D Bilesik Eksen Haritalama (Composite Axis 2D: "Move" -> WASD)
    actionMap.MapAxis2D("Move", GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D);

    // W ve D basildi -> x: +1.0, y: +1.0
    rawInput.SimulateKey(GLFW_KEY_W, GLFW_PRESS);
    rawInput.SimulateKey(GLFW_KEY_D, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    glm::vec2 moveVec = actionMap.GetAxis2D("Move");
    TEST_CHECK(suite, "CompositeAxisXPositive", std::abs(moveVec.x - 1.0f) < 0.001f);
    TEST_CHECK(suite, "CompositeAxisYPositive", std::abs(moveVec.y - 1.0f) < 0.001f);

    rawInput.SimulateKey(GLFW_KEY_W, GLFW_RELEASE);
    rawInput.SimulateKey(GLFW_KEY_D, GLFW_RELEASE);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);

    // 4. Fare Butonu Eylemi ("Fire" -> Sol Fare)
    actionMap.MapMouseButtonAction("Fire", GLFW_MOUSE_BUTTON_LEFT);
    rawInput.SimulateMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "MouseButtonActionTriggered", actionMap.IsActionActive("Fire") && actionMap.IsActionJustPressed("Fire"));

    rawInput.SimulateMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "MouseButtonActionReleased", !actionMap.IsActionActive("Fire") && actionMap.IsActionJustReleased("Fire"));

    // 5. Dinamik Tus Yeniden Atama (Rebinding: Space -> F tusu)
    bool rebound = actionMap.RebindKey("Jump", GLFW_KEY_SPACE, GLFW_KEY_F);
    TEST_CHECK(suite, "RebindKeyReturnedTrue", rebound);

    rawInput.SimulateKey(GLFW_KEY_SPACE, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "OldKeyNoLongerTriggersAction", !actionMap.IsActionActive("Jump"));

    rawInput.SimulateKey(GLFW_KEY_SPACE, GLFW_RELEASE);
    rawInput.SimulateKey(GLFW_KEY_F, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "NewReboundKeyTriggersAction", actionMap.IsActionActive("Jump"));

    rawInput.SimulateKey(GLFW_KEY_F, GLFW_RELEASE);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);

    // 6. Katmanli Baglamlar (Contexts: "Gameplay" vs "Menu")
    actionMap.PushContext("Menu");
    actionMap.MapAction("Confirm", GLFW_KEY_ENTER);

    rawInput.SimulateKey(GLFW_KEY_ENTER, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "ContextSpecificActionActive", actionMap.IsActionActive("Confirm"));

    // Default context'teki eyleme geri dusme (Fallback)
    rawInput.SimulateKey(GLFW_KEY_F, GLFW_PRESS);
    rawInput.BeginFrame();
    actionMap.Update(rawInput);
    TEST_CHECK(suite, "FallbackToDefaultContextAction", actionMap.IsActionActive("Jump"));

    // Context kaldirma (Pop)
    actionMap.PopContext();
    TEST_CHECK(suite, "ContextPoppedBackToDefault", actionMap.GetCurrentContext() == "Default");
}

} // namespace Astral::Test
