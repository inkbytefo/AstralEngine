#pragma once

#include "AstralEngine.h"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <cmath>

namespace Sandbox {
/// Client-owned fixture shared by Sandbox and its regression tests, never by the engine.
class DemoScene {
public:
    std::shared_ptr<Astral::Scene> Create(bool stress) {
        using namespace Astral;
        m_Box = {};
        m_Torus = {};
        // 1. Authoring (Editor) Sahnesi olustur
        auto editorScene = std::make_shared<Scene>("Sandbox Editor Level");

        // Obje 0: Zemin Entity
        Entity ground = editorScene->CreateEntity();
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, -1.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        ground.AddComponent<SDFComponent>(
            static_cast<uint32_t>(PrimitiveType::Plane),
            static_cast<uint32_t>(CSGOperation::Union),
            0.0f, 0u, glm::vec3(0.3f, 0.32f, 0.35f), 0.8f, 0.05f
        );

        if (stress) {
            for (int i = 0; i < 31; ++i) {
                Entity obj = editorScene->CreateEntity();
                float angle = static_cast<float>(i) * (2.0f * 3.14159f / 31.0f);
                float radius = 3.0f + static_cast<float>(i % 3) * 2.5f;
                float heightY = 0.5f + static_cast<float>(i % 4) * 1.0f;
                glm::vec3 pos = glm::vec3(std::cos(angle) * radius, heightY, std::sin(angle) * radius);

                uint32_t type = i % 3; // 0=Sphere, 1=Box, 2=Torus
                glm::vec3 scale{1.0f};
                glm::vec3 albedo{1.0f};
                if (type == 0) {
                    scale = glm::vec3(0.5f + (i % 2) * 0.2f);
                    albedo = glm::vec3(0.85f, 0.2f + (i % 5) * 0.15f, 0.25f);
                } else if (type == 1) {
                    scale = glm::vec3(0.45f + (i % 3) * 0.1f);
                    albedo = glm::vec3(0.2f, 0.5f + (i % 4) * 0.1f, 0.9f);
                } else {
                    scale = glm::vec3(0.6f, 0.2f, 1.0f);
                    albedo = glm::vec3(0.9f, 0.8f, 0.2f);
                }

                obj.AddComponent<TransformComponent>(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale);
                obj.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.0f, 0.2f, 0.0f));
                obj.AddComponent<SDFComponent>(
                    type,
                    static_cast<uint32_t>(CSGOperation::SmoothUnion),
                    0.2f, 1u, albedo, 0.3f, 0.5f
                );
            }
        } else {
            // Obje 1: Kutu
            m_Box = editorScene->CreateEntity();
            m_Box.AddComponent<TransformComponent>(glm::vec3(-1.8f, 0.2f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.6f));
            m_Box.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.0f, 0.5f, 0.0f));
            m_Box.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Box),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.3f, 1u, glm::vec3(0.2f, 0.5f, 0.9f), 0.4f, 0.3f
            );

            // Obje 2: Merkez Kure
            Entity sphereEntity = editorScene->CreateEntity();
            sphereEntity.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.3f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.85f));
            sphereEntity.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Sphere),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.3f, 1u, glm::vec3(0.9f, 0.25f, 0.2f), 0.2f, 0.8f
            );

            // Obje 3: Torus
            m_Torus = editorScene->CreateEntity();
            m_Torus.AddComponent<TransformComponent>(glm::vec3(1.8f, 0.2f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.7f, 0.25f, 1.0f));
            m_Torus.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.5f, 0.0f, 0.0f));
            m_Torus.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Torus),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.25f, 1u, glm::vec3(0.9f, 0.75f, 0.15f), 0.3f, 0.9f
            );
        }

        auto camera = editorScene->CreateEntity();
        const glm::vec3 direction = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
        camera.AddComponent<TransformComponent>(glm::vec3(0.0f, 1.5f, 4.0f),
            glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
        camera.AddComponent<CameraComponent>(2.0f * std::atan(0.5f / 1.5f), 0.01f, 50.0f, 1u);
        camera.AddComponent<TagComponent>("Sandbox Camera");
        return editorScene;
    }

    void Update(Astral::Scene& scene, bool stress, uint32_t frameIndex) {
        using namespace Astral;
        const float timeSec = static_cast<float>(frameIndex) * 0.016f;
            if (stress) {
                auto& transforms = scene.GetRegistry().GetView<TransformComponent>();
                size_t idx = 0;
                for (auto&& [entity, transform] : transforms) {
                    if (idx > 0 && scene.GetRegistry().HasComponent<SDFComponent>(entity)) { // Zemin haric
                        float phase = timeSec * 1.2f + static_cast<float>(idx) * 0.5f;
                        transform.position.y += std::sin(phase) * 0.005f;
                    }
                    idx++;
                }
            } else {
                if (m_Box.GetScene() != &scene || m_Torus.GetScene() != &scene) return;
                Entity rBox(m_Box.GetHandle(), &scene);
                if (rBox.HasComponent<TransformComponent>()) {
                    auto& boxTr = rBox.GetComponent<TransformComponent>();
                    boxTr.position.y = 0.2f + std::sin(timeSec * 1.5f) * 0.2f;
                    float angleY = timeSec * 0.5f;
                    boxTr.rotation = glm::angleAxis(angleY, glm::vec3(0.0f, 1.0f, 0.0f));
                }
                Entity rTorus(m_Torus.GetHandle(), &scene);
                if (rTorus.HasComponent<TransformComponent>()) {
                    auto& torusTr = rTorus.GetComponent<TransformComponent>();
                    torusTr.position.y = 0.2f + std::cos(timeSec * 1.5f) * 0.2f;
                    float angleX = timeSec * 0.5f;
                    torusTr.rotation = glm::angleAxis(angleX, glm::vec3(1.0f, 0.0f, 0.0f));
                }
            }

    }
private:
    Astral::Entity m_Box;
    Astral::Entity m_Torus;
};
} // namespace Sandbox
