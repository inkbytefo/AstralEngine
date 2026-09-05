#pragma once

#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Scene/SDFWorldQuery.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace Sandbox {

/**
 * @brief Oynanabilir Referans SDF Bulmaca Oyunu Alt Sistemi (A3)
 *
 * Motor cekirdeginden tamamen bagimsiz olarak istemci katmaninda (Sandbox) calisir.
 * - Karakter Hareketi: WASD, ziplama, yercekimi.
 * - Analitik SDF Carpismasi: SDFWorldQuery::ResolveSphereCollision ile yuzey penetrasyonunu onler.
 * - CSG Dunya Modifikasyonu: 'E' veya sol tik ile engelde kapi/gecit oyar (CSG Subtract).
 * - Hedef ve Kazanma: Hedef torus/kuresine ulasildiginda kazanma durumunu tetikler.
 * - Yeniden Baslatma: 'R' tusu ile baslangica doner.
 */
class PuzzleGameSubsystem final : public Astral::ISubsystem {
public:
    PuzzleGameSubsystem();
    ~PuzzleGameSubsystem() override = default;

    void OnInit() override;
    void OnUpdate(Astral::FrameContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] Astral::SystemStage GetStage() const noexcept override {
        return Astral::SystemStage::Gameplay;
    }

    /// Bulmaca sahnesini hazirlar
    static std::shared_ptr<Astral::Scene> CreatePuzzleScene();

    [[nodiscard]] bool IsGameWon() const noexcept { return m_GameWon; }
    [[nodiscard]] bool IsDoorCarved() const noexcept { return m_DoorCarved; }
    [[nodiscard]] glm::vec3 GetPlayerPosition() const noexcept { return m_PlayerPos; }

    void SetInputBlocked(bool blocked) noexcept { m_InputBlocked = blocked; }
    [[nodiscard]] bool IsInputBlocked() const noexcept { return m_InputBlocked; }

    /// Testler ve script kontrolleri icin programatik eylemler
    void CarveDoorway(Astral::Registry& reg);
    void CarveDoorway(Astral::Scene& scene);
    void ResetGame(Astral::Registry& reg);
    void ResetGame(Astral::Scene& scene);

private:
    glm::vec3 m_PlayerPos{ 0.0f, 0.5f, 3.5f };
    glm::vec3 m_PlayerVel{ 0.0f };
    float m_PlayerRadius = 0.4f;
    bool m_IsGrounded = false;
    bool m_GameWon = false;
    bool m_DoorCarved = false;
    bool m_InputBlocked = false;

    Astral::EntityHandle m_PlayerEntity = Astral::NullEntityHandle;
    Astral::EntityHandle m_GoalEntity = Astral::NullEntityHandle;
    Astral::EntityHandle m_WallEntity = Astral::NullEntityHandle;
    Astral::EntityHandle m_CutterEntity = Astral::NullEntityHandle;
};

} // namespace Sandbox
