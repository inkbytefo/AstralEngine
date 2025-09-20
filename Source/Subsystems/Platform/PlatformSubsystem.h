#pragma once

#include "../../Core/ISubsystem.h"
#include "Window.h"
#include "InputManager.h"
#include "../../Events/EventManager.h"
#include <memory>
#include <vector>

namespace AstralEngine {

/**
 * @brief Platform bağımlı işlemleri yöneten alt sistem.
 * 
 * Pencere oluşturma, olay döngüsü, kullanıcı girdileri gibi
 * işletim sistemi ile ilgili tüm işlemleri soyutlar.
 */
class PlatformSubsystem : public ISubsystem {
public:
    PlatformSubsystem();
    ~PlatformSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "PlatformSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::PreUpdate; }

    // Pencere ve girdi erişimi
    Window* GetWindow() const { return m_window.get(); }
    InputManager* GetInputManager() const { return m_inputManager.get(); }

    // Platform bilgileri
    bool IsWindowOpen() const;
    void CloseWindow();

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<InputManager> m_inputManager;
    Engine* m_owner = nullptr;
};

} // namespace AstralEngine
