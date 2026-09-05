#pragma once

#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/Application.hpp"
#include "Astral/Renderer/RenderContext.hpp"
#include "Astral/Editor/EditorUI.hpp"
#include <memory>

namespace Astral {

/**
 * @brief EditorUISubsystem, ImGui ve tum AstralEditor arayuzunu
 *        motorun ISubsystem mimarisi uzerinden yoneten alt sistemdir.
 *        Editör nesne seçimi (authoring selection) ve panelleri bu sistem bünyesinde yönetilir.
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

    [[nodiscard]] Entity& GetSelectedEntity() noexcept { return m_SelectedEntity; }
    [[nodiscard]] const Entity& GetSelectedEntity() const noexcept { return m_SelectedEntity; }
    void SetSelectedEntity(const Entity& entity);

    enum class EditorMode {
        Edit,
        Play,
        Paused
    };

    [[nodiscard]] EditorMode GetEditorMode() const noexcept { return m_EditorMode; }
    [[nodiscard]] bool IsPlaying() const noexcept { return m_EditorMode == EditorMode::Play || m_EditorMode == EditorMode::Paused; }

    void StartPlayMode();
    void StopPlayMode();
    void TogglePausePlayMode();

private:
    Application& m_App;
    std::unique_ptr<EditorUI> m_EditorUI;
    Entity m_SelectedEntity;
    SubscriptionToken m_PickSub;
    SubscriptionToken m_SceneSub;

    EditorMode m_EditorMode{ EditorMode::Edit };
    std::shared_ptr<Scene> m_AuthoringSceneBackup;
};

} // namespace Astral
