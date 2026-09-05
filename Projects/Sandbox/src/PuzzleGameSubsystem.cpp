#define GLM_ENABLE_EXPERIMENTAL
#include "PuzzleGameSubsystem.hpp"
#include "Astral/Core/Components.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <iostream>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace Sandbox {

namespace {

bool IsUICapturingKeyboard() {
#if __has_include(<imgui.h>)
    if (ImGui::GetCurrentContext() != nullptr) {
        return ImGui::GetIO().WantCaptureKeyboard;
    }
#endif
    return false;
}

bool IsUICapturingMouse() {
#if __has_include(<imgui.h>)
    if (ImGui::GetCurrentContext() != nullptr) {
        return ImGui::GetIO().WantCaptureMouse;
    }
#endif
    return false;
}

} // namespace

PuzzleGameSubsystem::PuzzleGameSubsystem()
    : Astral::ISubsystem(Astral::SystemStage::Gameplay) {}

void PuzzleGameSubsystem::OnInit() {
    m_PlayerPos = glm::vec3(0.0f, 0.5f, 3.5f);
    m_PlayerVel = glm::vec3(0.0f);
    m_IsGrounded = false;
    m_GameWon = false;
    m_DoorCarved = false;
    m_InputBlocked = false;
}

void PuzzleGameSubsystem::OnShutdown() {}

std::shared_ptr<Astral::Scene> PuzzleGameSubsystem::CreatePuzzleScene() {
    auto scene = std::make_shared<Astral::Scene>("SDFPuzzleScene");

    // 1. Zemin (Floor Box)
    auto floor = scene->CreateEntity("Floor");
    auto& floorTr = floor.AddComponent<Astral::TransformComponent>();
    floorTr.position = glm::vec3(0.0f, -0.5f, 0.0f);
    floorTr.scale = glm::vec3(8.0f, 0.5f, 8.0f);
    auto& floorSdf = floor.AddComponent<Astral::SDFComponent>();
    floorSdf.primitiveType = 1; // Box
    floorSdf.operation = 0;     // Union
    floorSdf.blendFactor = 0.05f;
    floorSdf.albedo = glm::vec3(0.2f, 0.22f, 0.25f);
    floorSdf.roughness = 0.8f;
    floorSdf.metallic = 0.1f;
    floorSdf.isVisible = 1;

    // 2. Engel Duvar (Wall Box) - Oyuncunun hedefe ulasmasini onler
    auto wall = scene->CreateEntity("ObstacleWall");
    auto& wallTr = wall.AddComponent<Astral::TransformComponent>();
    wallTr.position = glm::vec3(0.0f, 1.0f, 0.0f);
    wallTr.scale = glm::vec3(3.5f, 1.5f, 0.4f);
    auto& wallSdf = wall.AddComponent<Astral::SDFComponent>();
    wallSdf.primitiveType = 1; // Box
    wallSdf.operation = 0;     // Union
    wallSdf.blendFactor = 0.05f;
    wallSdf.albedo = glm::vec3(0.75f, 0.35f, 0.15f);
    wallSdf.roughness = 0.5f;
    wallSdf.metallic = 0.0f;
    wallSdf.isVisible = 1;

    // 3. CSG Kapi Kesici (Cutter Box/Cylinder - Subtract)
    // Baslangicta gizlidir (isVisible = 0); oyuncu etkilesime girince kapi acar
    auto cutter = scene->CreateEntity("CutterDoorway");
    auto& cutterTr = cutter.AddComponent<Astral::TransformComponent>();
    cutterTr.position = glm::vec3(0.0f, 1.0f, 0.0f);
    cutterTr.scale = glm::vec3(0.8f, 1.2f, 1.0f);
    auto& cutterSdf = cutter.AddComponent<Astral::SDFComponent>();
    cutterSdf.primitiveType = 1; // Box
    cutterSdf.operation = 1;     // Subtract (CSG Cikarma)
    cutterSdf.blendFactor = 0.05f;
    cutterSdf.albedo = glm::vec3(0.9f, 0.1f, 0.1f);
    cutterSdf.isVisible = 0;     // Baslangicta inaktif
    auto& cutterVis = cutter.AddComponent<Astral::VisibilityComponent>();
    cutterVis.isVisible = false;

    // 4. Hedef Bolgesi (Goal Torus/Sphere) - Duvarin ardinda
    auto goal = scene->CreateEntity("GoalOrb");
    auto& goalTr = goal.AddComponent<Astral::TransformComponent>();
    goalTr.position = glm::vec3(0.0f, 1.0f, -3.0f);
    goalTr.scale = glm::vec3(0.5f, 0.5f, 0.5f);
    auto& goalSdf = goal.AddComponent<Astral::SDFComponent>();
    goalSdf.primitiveType = 0; // Sphere
    goalSdf.operation = 0;     // Union
    goalSdf.blendFactor = 0.1f;
    goalSdf.albedo = glm::vec3(0.1f, 0.85f, 0.3f); // Parlak Yesil
    goalSdf.roughness = 0.2f;
    goalSdf.metallic = 0.8f;
    goalSdf.isVisible = 1;

    // 5. Oyuncu Kuresi (Player Sphere)
    auto player = scene->CreateEntity("Player");
    auto& playerTr = player.AddComponent<Astral::TransformComponent>();
    playerTr.position = glm::vec3(0.0f, 0.5f, 3.5f);
    playerTr.scale = glm::vec3(0.4f, 0.4f, 0.4f);
    auto& playerSdf = player.AddComponent<Astral::SDFComponent>();
    playerSdf.primitiveType = 0; // Sphere
    playerSdf.operation = 0;     // Union
    playerSdf.blendFactor = 0.05f;
    playerSdf.albedo = glm::vec3(0.2f, 0.6f, 0.95f); // Mavi
    playerSdf.roughness = 0.3f;
    playerSdf.metallic = 0.2f;
    playerSdf.isVisible = 1;

    // 6. Ana Kamera
    auto camera = scene->CreateEntity("MainCamera");
    auto& camTr = camera.AddComponent<Astral::TransformComponent>();
    camTr.position = glm::vec3(0.0f, 3.5f, 7.5f);
    camTr.rotation = glm::angleAxis(glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    auto& camComp = camera.AddComponent<Astral::CameraComponent>();
    camComp.primary = 1;
    camComp.verticalFovRadians = glm::radians(60.0f);

    return scene;
}

void PuzzleGameSubsystem::CarveDoorway(Astral::Registry& reg) {
    auto view = reg.GetView<Astral::TagComponent>();
    for (auto&& [entity, tag] : view) {
        if (tag.tag == "CutterDoorway") {
            if (reg.HasComponent<Astral::SDFComponent>(entity)) {
                reg.GetComponent<Astral::SDFComponent>(entity).isVisible = 1;
            }
            if (reg.HasComponent<Astral::VisibilityComponent>(entity)) {
                reg.GetComponent<Astral::VisibilityComponent>(entity).isVisible = true;
            }
            m_DoorCarved = true;
            std::cout << "[PuzzleGame] Kapi CSG ile oyuldu! Gecit acildi.\n";
            break;
        }
    }
}

void PuzzleGameSubsystem::CarveDoorway(Astral::Scene& scene) {
    CarveDoorway(scene.GetRegistry());
}

void PuzzleGameSubsystem::ResetGame(Astral::Registry& reg) {
    m_PlayerPos = glm::vec3(0.0f, 0.5f, 3.5f);
    m_PlayerVel = glm::vec3(0.0f);
    m_IsGrounded = false;
    m_GameWon = false;
    m_DoorCarved = false;

    auto view = reg.GetView<Astral::TagComponent>();
    for (auto&& [entity, tag] : view) {
        if (tag.tag == "CutterDoorway") {
            if (reg.HasComponent<Astral::SDFComponent>(entity)) {
                reg.GetComponent<Astral::SDFComponent>(entity).isVisible = 0;
            }
            if (reg.HasComponent<Astral::VisibilityComponent>(entity)) {
                reg.GetComponent<Astral::VisibilityComponent>(entity).isVisible = false;
            }
        } else if (tag.tag == "Player") {
            if (reg.HasComponent<Astral::TransformComponent>(entity)) {
                reg.GetComponent<Astral::TransformComponent>(entity).position = m_PlayerPos;
            }
        }
    }

    std::cout << "[PuzzleGame] Oyun sifirlandi.\n";
}

void PuzzleGameSubsystem::ResetGame(Astral::Scene& scene) {
    ResetGame(scene.GetRegistry());
}

void PuzzleGameSubsystem::OnUpdate(Astral::FrameContext& context) {
    auto& reg = context.registry;
    float dt = std::clamp(context.deltaTime, 0.0001f, 0.1f);

    // Entity handle'larini bul veya guncelle
    m_PlayerEntity = Astral::NullEntityHandle;
    m_GoalEntity = Astral::NullEntityHandle;
    m_WallEntity = Astral::NullEntityHandle;
    m_CutterEntity = Astral::NullEntityHandle;

    auto tagView = reg.GetView<Astral::TagComponent>();
    for (auto&& [entity, tag] : tagView) {
        if (tag.tag == "Player") m_PlayerEntity = entity;
        else if (tag.tag == "GoalOrb") m_GoalEntity = entity;
        else if (tag.tag == "ObstacleWall") m_WallEntity = entity;
        else if (tag.tag == "CutterDoorway") m_CutterEntity = entity;
    }

    if (m_PlayerEntity == Astral::NullEntityHandle || !reg.HasComponent<Astral::TransformComponent>(m_PlayerEntity)) {
        return;
    }

    // UI input capture korumasi: ImGui veya m_InputBlocked aktifse oyun kontrolleri dinlenmez
    bool keyboardCaptured = m_InputBlocked || IsUICapturingKeyboard();
    bool mouseCaptured = m_InputBlocked || IsUICapturingMouse();

    // 1. Etkilesim: CSG Kapi Kesme ('E' tusu veya Sol Tik)
    if (!keyboardCaptured && context.input.IsKeyJustPressed(GLFW_KEY_E)) {
        if (!m_DoorCarved) {
            CarveDoorway(reg);
        }
    }
    if (!mouseCaptured && context.input.IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!m_DoorCarved) {
            CarveDoorway(reg);
        }
    }

    // 2. Yeniden Baslatma ('R' tusu)
    if (!keyboardCaptured && context.input.IsKeyJustPressed(GLFW_KEY_R)) {
        ResetGame(reg);
        return;
    }

    // 3. Karakter Hareketi ve Fizik
    glm::vec3 moveDir(0.0f);
    if (!keyboardCaptured) {
        if (context.input.IsKeyPressed(GLFW_KEY_W) || context.input.IsKeyPressed(GLFW_KEY_UP)) {
            moveDir.z -= 1.0f;
        }
        if (context.input.IsKeyPressed(GLFW_KEY_S) || context.input.IsKeyPressed(GLFW_KEY_DOWN)) {
            moveDir.z += 1.0f;
        }
        if (context.input.IsKeyPressed(GLFW_KEY_A) || context.input.IsKeyPressed(GLFW_KEY_LEFT)) {
            moveDir.x -= 1.0f;
        }
        if (context.input.IsKeyPressed(GLFW_KEY_D) || context.input.IsKeyPressed(GLFW_KEY_RIGHT)) {
            moveDir.x += 1.0f;
        }
        if (context.input.IsKeyJustPressed(GLFW_KEY_SPACE) && m_IsGrounded) {
            m_PlayerVel.y = 5.0f; // Ziplama
            m_IsGrounded = false;
        }
    }

    float moveSpeed = 4.0f;
    if (glm::length(moveDir) > 0.001f) {
        moveDir = glm::normalize(moveDir);
        m_PlayerVel.x = moveDir.x * moveSpeed;
        m_PlayerVel.z = moveDir.z * moveSpeed;
    } else {
        m_PlayerVel.x = glm::mix(m_PlayerVel.x, 0.0f, dt * 10.0f);
        m_PlayerVel.z = glm::mix(m_PlayerVel.z, 0.0f, dt * 10.0f);
    }

    // Yercekimi
    m_PlayerVel.y -= 9.8f * dt;

    // Aday pozisyon
    m_PlayerPos += m_PlayerVel * dt;

    // 4. Analitik SDF Carpismasi ve Yuzey Cozumu
    // Gecici olarak player kuresinin kendisini sorgudan haric tutmak icin SDF mesafesini
    // yalnizca diger nesnelerle karsilastirmak uzere kure carpismasini calistiririz.
    // Player nesnesi kure seklinde oldugundan, yuzey cozumu yapilirken sahne bilesenleri ile test edilir.
    bool collided = Astral::SDFWorldQuery::ResolveSphereCollision(reg, m_PlayerPos, m_PlayerRadius, 0.02f, 4);

    if (collided) {
        // Yuzey normalini sorgula
        glm::vec3 normal = Astral::SDFWorldQuery::QueryNormal(reg, m_PlayerPos);
        if (normal.y > 0.6f) {
            m_IsGrounded = true;
            if (m_PlayerVel.y < 0.0f) {
                m_PlayerVel.y = 0.0f;
            }
        }
    } else {
        m_IsGrounded = false;
    }

    // Transform bilesenini guncelle
    auto& tr = reg.GetComponent<Astral::TransformComponent>(m_PlayerEntity);
    tr.position = m_PlayerPos;

    // 5. Hedefe Ulasma / Kazanma Kontrolu
    if (m_GoalEntity != Astral::NullEntityHandle && reg.HasComponent<Astral::TransformComponent>(m_GoalEntity)) {
        const auto& goalTr = reg.GetComponent<Astral::TransformComponent>(m_GoalEntity);
        float distToGoal = glm::distance(m_PlayerPos, goalTr.position);
        if (distToGoal < 1.2f) {
            if (!m_GameWon) {
                m_GameWon = true;
                std::cout << "[PuzzleGame] TEBRIKLER! Hedefe ulasildi ve bulmaca kazanildi!\n";
            }
        }
    }
}

} // namespace Sandbox
