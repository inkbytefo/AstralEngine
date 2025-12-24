#pragma once

#include "../../Subsystems/Scene/Scene.h"
#include <memory>
#include <string>

namespace AstralEngine {

/**
 * @brief Scene serialization and deserialization using nlohmann/json.
 */
class SceneSerializer {
public:
  SceneSerializer(Scene *scene);

  void Serialize(const std::string &filepath);
  bool Deserialize(const std::string &filepath);

private:
  Scene *m_scene;
};

} // namespace AstralEngine
