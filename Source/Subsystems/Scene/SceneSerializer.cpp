#include "SceneSerializer.h"
#include "../../Core/Logger.h"
#include "../../ECS/Components.h"
#include "../../Subsystems/Scene/Entity.h"
#include <fstream>
#include <glm/glm.hpp>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace AstralEngine {

using json = nlohmann::json;

// Helper to serialize glm types
static json SerializeVec3(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

static glm::vec3 DeserializeVec3(const json &j) {
  return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

SceneSerializer::SceneSerializer(const std::shared_ptr<Scene> &scene)
    : m_scene(scene) {}

void SceneSerializer::Serialize(const std::string &filepath) {
  json root = json::object();
  root["Scene"] = "Untitled";

  auto entities = json::array();

  m_scene->Reg().each([&](auto entityID) {
    Entity entity(entityID, m_scene.get());
    if (!entity)
      return;

    json entityJson = json::object();

    // 1. ID Component
    if (entity.HasComponent<IDComponent>()) {
      entityJson["ID"] = (uint64_t)entity.GetComponent<IDComponent>().ID;
    }

    // 2. Name Component
    if (entity.HasComponent<NameComponent>()) {
      entityJson["Name"] = entity.GetComponent<NameComponent>().name;
    }

    // 3. Transform Component
    if (entity.HasComponent<TransformComponent>()) {
      auto &tc = entity.GetComponent<TransformComponent>();
      entityJson["Transform"] = {{"Position", SerializeVec3(tc.position)},
                                 {"Rotation", SerializeVec3(tc.rotation)},
                                 {"Scale", SerializeVec3(tc.scale)}};
    }

    // 4. Render Component
    if (entity.HasComponent<RenderComponent>()) {
      auto &rc = entity.GetComponent<RenderComponent>();
      entityJson["Render"] = {{"MaterialID", rc.materialHandle.GetID()},
                              {"ModelID", rc.modelHandle.GetID()},
                              {"Visible", rc.visible},
                              {"CastsShadows", rc.castsShadows}};
    }

    // 5. Light Component
    if (entity.HasComponent<LightComponent>()) {
      auto &lc = entity.GetComponent<LightComponent>();
      entityJson["Light"] = {{"Type", (int)lc.type},
                             {"Color", SerializeVec3(lc.color)},
                             {"Intensity", lc.intensity},
                             {"Range", lc.range}};
    }

    // 6. Camera Component
    if (entity.HasComponent<CameraComponent>()) {
      auto &cc = entity.GetComponent<CameraComponent>();
      entityJson["Camera"] = {{"Type", (int)cc.projectionType},
                              {"FOV", cc.fieldOfView},
                              {"Near", cc.nearPlane},
                              {"Far", cc.farPlane},
                              {"Main", cc.isMainCamera}};
    }

    entities.push_back(entityJson);
  });

  root["Entities"] = entities;

  std::ofstream fout(filepath);
  if (fout.is_open()) {
    fout << root.dump(4);
    Logger::Info("SceneSerializer", "Scene serialized to: {}", filepath);
  } else {
    Logger::Error("SceneSerializer",
                  "Failed to open file for serialization: {}", filepath);
  }
}

bool SceneSerializer::Deserialize(const std::string &filepath) {
  std::ifstream fin(filepath);
  if (!fin.is_open()) {
    Logger::Error("SceneSerializer", "Failed to open scene file: {}", filepath);
    return false;
  }

  json root;
  try {
    fin >> root;
  } catch (const json::parse_error &e) {
    Logger::Error("SceneSerializer", "JSON Parse error: {}", e.what());
    return false;
  }

  if (!root.contains("Entities"))
    return false;

  for (auto &entityJson : root["Entities"]) {
    uint64_t uuid = entityJson["ID"].get<uint64_t>();
    std::string name = entityJson.value("Name", "Entity");

    Entity entity = m_scene->CreateEntityWithUUID(UUID(uuid), name);

    if (entityJson.contains("Transform")) {
      auto &tc = entity.GetComponent<TransformComponent>();
      auto &trans = entityJson["Transform"];
      tc.position = DeserializeVec3(trans["Position"]);
      tc.rotation = DeserializeVec3(trans["Rotation"]);
      tc.scale = DeserializeVec3(trans["Scale"]);
    }

    if (entityJson.contains("Render")) {
      auto &rc = entity.AddComponent<RenderComponent>();
      auto &r = entityJson["Render"];
      rc.materialHandle = AssetHandle(r["MaterialID"].get<uint64_t>(),
                                      AssetHandle::Type::Material);
      rc.modelHandle =
          AssetHandle(r["ModelID"].get<uint64_t>(), AssetHandle::Type::Model);
      rc.visible = r.value("Visible", true);
      rc.castsShadows = r.value("CastsShadows", true);
    }

    if (entityJson.contains("Light")) {
      auto &lc = entity.AddComponent<LightComponent>();
      auto &l = entityJson["Light"];
      lc.type = (LightComponent::LightType)l["Type"].get<int>();
      lc.color = DeserializeVec3(l["Color"]);
      lc.intensity = l["Intensity"].get<float>();
      lc.range = l["Range"].get<float>();
    }

    if (entityJson.contains("Camera")) {
      auto &cc = entity.AddComponent<CameraComponent>();
      auto &c = entityJson["Camera"];
      cc.projectionType = (CameraComponent::ProjectionType)c["Type"].get<int>();
      cc.fieldOfView = c["FOV"].get<float>();
      cc.nearPlane = c["Near"].get<float>();
      cc.farPlane = c["Far"].get<float>();
      cc.isMainCamera = c.value("Main", false);
    }
  }

  Logger::Info("SceneSerializer", "Scene deserialized from: {}", filepath);
  return true;
}

} // namespace AstralEngine
