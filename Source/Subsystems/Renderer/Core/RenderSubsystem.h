#pragma once

#include "Core/ISubsystem.h"
#include "../RHI/IRHIDevice.h"
#include <memory>

namespace AstralEngine {

/**
 * @brief Yüksek seviye Render Alt Sistemi
 * 
 * RHI (Render Hardware Interface) cihazını yönetir ve
 * render döngüsünü kontrol eder.
 */
class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;

    // ISubsystem interface implementation
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;

    const char* GetName() const override { return "RenderSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::Render; }

    // RHI Device Access
    IRHIDevice* GetDevice() const { return m_device.get(); }

private:
    Engine* m_engine = nullptr;
    std::shared_ptr<IRHIDevice> m_device;
};

} // namespace AstralEngine
