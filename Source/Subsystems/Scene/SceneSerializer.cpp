#include "SceneSerializer.h"
#include "Entity.h"
#include "../../ECS/Components.h"
#include "../../Core/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace glm {
    void to_json(json& j, const vec3& v) {
        j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
    }

    void from_json(const json& j, vec3& v) {
        v.x = j.at("x").get<float>();
        v.y = j.at("y").get<float>();
        v.z = j.at("z").get<float>();
    }

    void to_json(json& j, const vec4& v) {
        j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
    }

    void from_json(const json& j, vec4& v) {
        v.x = j.at("x").get<float>();
        v.y = j.at("y").get<float>();
        v.z = j.at("z").get<float>();
        v.w = j.at("w").get<float>();
    }
}

namespace AstralEngine {

    SceneSerializer::SceneSerializer(Scene* scene)
        : m_Scene(scene) {
    }

    void SceneSerializer::Serialize(const std::string& filepath) {
        json root;
        root["Scene"] = "Untitled"; // TODO: Scene name
        root["Entities"] = json::array();

        auto view = m_Scene->Reg().view<IDComponent>();
        for (auto entityID : view) {
            Entity entity = { entityID, m_Scene };
            if (!entity) continue;

            json entityJson;
            
            // ID Component
            if (entity.HasComponent<IDComponent>()) {
                entityJson["UUID"] = (uint64_t)entity.GetComponent<IDComponent>().ID;
            }

            // Tag Component
            if (entity.HasComponent<TagComponent>()) {
                entityJson["TagComponent"] = {
                    {"Tag", entity.GetComponent<TagComponent>().tag}
                };
            }

            // Transform Component
            if (entity.HasComponent<TransformComponent>()) {
                auto& tc = entity.GetComponent<TransformComponent>();
                entityJson["TransformComponent"] = {
                    {"Position", tc.position},
                    {"Rotation", tc.rotation},
                    {"Scale", tc.scale}
                };
            }

            // Relationship Component (Parent UUID)
            if (entity.HasComponent<RelationshipComponent>()) {
                auto& rc = entity.GetComponent<RelationshipComponent>();
                if (rc.Parent != entt::null) {
                    Entity parent = { rc.Parent, m_Scene };
                    if (parent.HasComponent<IDComponent>()) {
                         entityJson["RelationshipComponent"] = {
                            {"ParentUUID", (uint64_t)parent.GetComponent<IDComponent>().ID}
                        };
                    }
                }
            }

            // Camera Component
            if (entity.HasComponent<CameraComponent>()) {
                auto& cc = entity.GetComponent<CameraComponent>();
                entityJson["CameraComponent"] = {
                    {"ProjectionType", (int)cc.projectionType},
                    {"PerspectiveFOV", cc.fieldOfView},
                    {"PerspectiveNear", cc.nearPlane},
                    {"PerspectiveFar", cc.farPlane},
                    {"OrthoLeft", cc.orthoLeft},
                    {"OrthoRight", cc.orthoRight},
                    {"OrthoBottom", cc.orthoBottom},
                    {"OrthoTop", cc.orthoTop},
                    {"Primary", cc.isMainCamera}
                };
            }

            // Light Component
            if (entity.HasComponent<LightComponent>()) {
                auto& lc = entity.GetComponent<LightComponent>();
                entityJson["LightComponent"] = {
                    {"Type", (int)lc.type},
                    {"Color", lc.color},
                    {"Intensity", lc.intensity},
                    {"Range", lc.range},
                    {"InnerConeAngle", lc.innerConeAngle},
                    {"OuterConeAngle", lc.outerConeAngle}
                };
            }

            // Render Component
            if (entity.HasComponent<RenderComponent>()) {
                auto& rc = entity.GetComponent<RenderComponent>();
                entityJson["RenderComponent"] = {
                    {"Visible", rc.visible},
                    {"Layer", rc.renderLayer},
                    {"CastsShadows", rc.castsShadows},
                    {"ReceivesShadows", rc.receivesShadows},
                    {"MaterialHandle", {
                        {"ID", rc.materialHandle.GetID()},
                        {"Path", rc.materialHandle.GetPath()},
                        {"Type", (int)rc.materialHandle.GetType()}
                    }},
                    {"ModelHandle", {
                        {"ID", rc.modelHandle.GetID()},
                        {"Path", rc.modelHandle.GetPath()},
                        {"Type", (int)rc.modelHandle.GetType()}
                    }}
                };
            }

            root["Entities"].push_back(entityJson);
        }

        std::ofstream fout(filepath);
        fout << root.dump(4);
    }

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        std::ifstream stream(filepath);
        if (!stream.is_open()) {
            Logger::Error("SceneSerializer", "Failed to open file: " + filepath);
            return false;
        }

        json root;
        try {
            stream >> root;
        } catch (const json::parse_error& e) {
            Logger::Error("SceneSerializer", std::string("JSON Parse Error: ") + e.what());
            return false;
        }

        auto entities = root["Entities"];
        if (entities.is_null()) return false;

        // Map to resolve parent-child relationships
        // Child Entity -> Parent UUID
        std::unordered_map<entt::entity, uint64_t> childToParentMap;
        // UUID -> Entity
        std::unordered_map<uint64_t, entt::entity> uuidToEntityMap;

        for (auto& entityJson : entities) {
            uint64_t uuid = entityJson["UUID"];
            
            std::string name;
            if (entityJson.contains("TagComponent")) {
                name = entityJson["TagComponent"]["Tag"];
            }

            Entity deserializedEntity = m_Scene->CreateEntityWithUUID(UUID(uuid), name);
            uuidToEntityMap[uuid] = (entt::entity)deserializedEntity;

            if (entityJson.contains("TransformComponent")) {
                auto& tc = deserializedEntity.GetComponent<TransformComponent>();
                auto& tcJson = entityJson["TransformComponent"];
                tc.position = tcJson["Position"];
                tc.rotation = tcJson["Rotation"];
                tc.scale = tcJson["Scale"];
            }

            if (entityJson.contains("CameraComponent")) {
                auto& cc = deserializedEntity.AddComponent<CameraComponent>();
                auto& ccJson = entityJson["CameraComponent"];
                cc.projectionType = (CameraComponent::ProjectionType)ccJson["ProjectionType"];
                cc.fieldOfView = ccJson["PerspectiveFOV"];
                cc.nearPlane = ccJson["PerspectiveNear"];
                cc.farPlane = ccJson["PerspectiveFar"];
                cc.orthoLeft = ccJson["OrthoLeft"];
                cc.orthoRight = ccJson["OrthoRight"];
                cc.orthoBottom = ccJson["OrthoBottom"];
                cc.orthoTop = ccJson["OrthoTop"];
                cc.isMainCamera = ccJson["Primary"];
            }

            if (entityJson.contains("LightComponent")) {
                auto& lc = deserializedEntity.AddComponent<LightComponent>();
                auto& lcJson = entityJson["LightComponent"];
                lc.type = (LightComponent::LightType)lcJson["Type"];
                lc.color = lcJson["Color"];
                lc.intensity = lcJson["Intensity"];
                lc.range = lcJson["Range"];
                lc.innerConeAngle = lcJson["InnerConeAngle"];
                lc.outerConeAngle = lcJson["OuterConeAngle"];
            }

            if (entityJson.contains("RenderComponent")) {
                auto& rcJson = entityJson["RenderComponent"];
                
                std::string matPath = rcJson["MaterialHandle"]["Path"];
                std::string modelPath = rcJson["ModelHandle"]["Path"];
                AssetHandle::Type matType = (AssetHandle::Type)rcJson["MaterialHandle"]["Type"];
                AssetHandle::Type modelType = (AssetHandle::Type)rcJson["ModelHandle"]["Type"];

                AssetHandle matHandle(matPath, matType);
                AssetHandle modelHandle(modelPath, modelType);
                
                auto& rc = deserializedEntity.AddComponent<RenderComponent>(matHandle, modelHandle);
                rc.visible = rcJson["Visible"];
                rc.renderLayer = rcJson["Layer"];
                rc.castsShadows = rcJson["CastsShadows"];
                rc.receivesShadows = rcJson["ReceivesShadows"];
            }

            if (entityJson.contains("RelationshipComponent")) {
                uint64_t parentUUID = entityJson["RelationshipComponent"]["ParentUUID"];
                childToParentMap[(entt::entity)deserializedEntity] = parentUUID;
            }
        }

        // Reconstruct Hierarchy
        for (auto [childEntity, parentUUID] : childToParentMap) {
            if (uuidToEntityMap.find(parentUUID) != uuidToEntityMap.end()) {
                Entity child = { childEntity, m_Scene };
                Entity parent = { uuidToEntityMap[parentUUID], m_Scene };
                m_Scene->ParentEntity(child, parent);
            }
        }

        return true;
    }

}
