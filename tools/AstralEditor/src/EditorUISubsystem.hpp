#pragma once

#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/Application.hpp"
#include "Astral/Editor/EditorUI.hpp"
#include <memory>

namespace Astral {

/**
 * @brief EditorUISubsystem, ImGui ve tum AstralEditor arayuzunu
 *        motorun ISubsystem mimarisi uzerinden yoneten alt sistemdir.
 */
class EditorUISubsystem : public ISubsystem {
public:
    explicit EditorUISubsystem(Application& app);
    ~EditorUISubsystem() override = default;

    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnRender(const RenderContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] bool HasRenderPass() const override { return true; }

    [[nodiscard]] EditorUI* GetEditorUI() const noexcept { return m_EditorUI.get(); }

private:
    Application& m_App;
    std::unique_ptr<EditorUI> m_EditorUI;
};

} // namespace Astral
