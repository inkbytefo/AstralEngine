#pragma once

// Extension point for saving your own components in scene files.
//
//     struct Health { float value = 100; };
//     Astral::RegisterComponent<Health>("Health",
//         [](const Health& h) { return nlohmann::json{{"value", h.value}}; },
//         [](const nlohmann::json& j) { return Health{j.value("value", 100.0f)}; });

#include "Astral/Engine/Ecs/Registry.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

namespace Astral {

struct ComponentSerializer {
    std::string name;
    std::function<bool(const Registry&, Entity)> has;
    std::function<nlohmann::json(const Registry&, Entity)> save;
    std::function<void(Registry&, Entity, const nlohmann::json&)> load;
};

void RegisterComponentSerializer(ComponentSerializer serializer);
const std::vector<ComponentSerializer>& GetComponentSerializers();

template <typename T>
ComponentSerializer MakeComponentSerializer(std::string name, std::function<nlohmann::json(const T&)> save,
                                           std::function<T(const nlohmann::json&)> load) {
    ComponentSerializer s;
    s.name = std::move(name);
    s.has = [](const Registry& r, Entity e) { return r.TryGet<T>(e) != nullptr; };
    s.save = [save](const Registry& r, Entity e) { return save(*r.TryGet<T>(e)); };
    s.load = [load](Registry& r, Entity e, const nlohmann::json& j) { r.Add<T>(e, load(j)); };
    return s;
}

template <typename T>
void RegisterComponent(std::string name, std::function<nlohmann::json(const T&)> save,
                       std::function<T(const nlohmann::json&)> load) {
    RegisterComponentSerializer(MakeComponentSerializer<T>(std::move(name), std::move(save), std::move(load)));
}

} // namespace Astral
