#include "Astral/Engine/Scene/SceneSerializer.hpp"
#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Engine/Scene/Serialization.hpp"

#include <fstream>
#include <mutex>
#include <sstream>

namespace Astral {

using json = nlohmann::json;

namespace {

constexpr int kFormatVersion = 1;

json Vec(const glm::vec3& v) { return {v.x, v.y, v.z}; }
json Vec(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }
json Quat(const glm::quat& q) { return {q.x, q.y, q.z, q.w}; }

template <typename V>
V ReadVec(const json& j, const char* key, V fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < size_t(V::length())) return fallback;
    V v;
    for (int i = 0; i < V::length(); ++i) v[i] = j[key][i].get<float>();
    return v;
}

glm::quat ReadQuat(const json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) return glm::quat(1, 0, 0, 0);
    return glm::normalize(glm::quat(j[key][3].get<float>(), j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>()));
}

json SaveMaterial(const MaterialParams& m) {
    return {{"baseColor", Vec(m.baseColor)}, {"metallic", m.metallic}, {"roughness", m.roughness}, {"emissive", Vec(m.emissive)}};
}
MaterialParams LoadMaterial(const json& j) {
    MaterialParams m;
    m.baseColor = ReadVec(j, "baseColor", m.baseColor);
    m.metallic = j.value("metallic", m.metallic);
    m.roughness = j.value("roughness", m.roughness);
    m.emissive = ReadVec(j, "emissive", m.emissive);
    return m;
}

std::vector<ComponentSerializer>& Serializers() {
    static std::vector<ComponentSerializer> list;
    return list;
}

void AddOrReplace(ComponentSerializer serializer) {
    for (auto& s : Serializers()) {
        if (s.name == serializer.name) {
            s = std::move(serializer);
            return;
        }
    }
    Serializers().push_back(std::move(serializer));
}

template <typename T>
void Builtin(std::string name, std::function<json(const T&)> save, std::function<T(const json&)> load) {
    AddOrReplace(MakeComponentSerializer<T>(std::move(name), std::move(save), std::move(load)));
}

void RegisterBuiltins() {
    Builtin<Transform>("Transform",
        [](const Transform& t) { return json{{"position", Vec(t.position)}, {"rotation", Quat(t.rotation)}, {"scale", Vec(t.scale)}}; },
        [](const json& j) {
            Transform t;
            t.position = ReadVec(j, "position", t.position);
            t.rotation = ReadQuat(j, "rotation");
            t.scale = ReadVec(j, "scale", t.scale);
            return t;
        });
    Builtin<CameraComponent>("Camera",
        [](const CameraComponent& c) {
            return json{{"verticalFov", c.verticalFov}, {"near", c.nearPlane}, {"far", c.farPlane}, {"primary", c.primary}};
        },
        [](const json& j) {
            CameraComponent c;
            c.verticalFov = j.value("verticalFov", c.verticalFov);
            c.nearPlane = j.value("near", c.nearPlane);
            c.farPlane = j.value("far", c.farPlane);
            c.primary = j.value("primary", c.primary);
            return c;
        });
    Builtin<MeshRenderer>("MeshRenderer",
        [](const MeshRenderer& m) {
            return json{{"mesh", m.mesh}, {"useAssetMaterials", m.useAssetMaterials}, {"material", SaveMaterial(m.material)},
                        {"castShadows", m.castShadows}};
        },
        [](const json& j) {
            MeshRenderer m;
            m.mesh = j.value("mesh", m.mesh);
            m.useAssetMaterials = j.value("useAssetMaterials", m.useAssetMaterials);
            if (j.contains("material")) m.material = LoadMaterial(j["material"]);
            m.castShadows = j.value("castShadows", m.castShadows);
            return m;
        });
    Builtin<SdfShapeComponent>("SdfShape",
        [](const SdfShapeComponent& s) {
            return json{{"primitive", static_cast<uint32_t>(s.primitive)}, {"operation", static_cast<uint32_t>(s.operation)},
                        {"size", Vec(s.size)}, {"blend", s.blend}, {"order", s.order},
                        {"albedo", Vec(s.material.albedo)}, {"roughness", s.material.roughness},
                        {"metallic", s.material.metallic}, {"emissive", Vec(s.material.emissive)}, {"collision", s.collision}};
        },
        [](const json& j) {
            SdfShapeComponent s;
            s.primitive = static_cast<Render::SdfPrimitive>(std::min(j.value("primitive", 0u), Render::kSdfPrimitiveCount - 1));
            s.operation = static_cast<Render::SdfOperation>(std::min(j.value("operation", 0u), Render::kSdfOperationCount - 1));
            s.size = ReadVec(j, "size", s.size);
            s.blend = j.value("blend", s.blend);
            s.order = j.value("order", s.order);
            s.material.albedo = ReadVec(j, "albedo", s.material.albedo);
            s.material.roughness = j.value("roughness", s.material.roughness);
            s.material.metallic = j.value("metallic", s.material.metallic);
            s.material.emissive = ReadVec(j, "emissive", s.material.emissive);
            s.collision = j.value("collision", s.collision);
            return s;
        });
    Builtin<LightComponent>("Light",
        [](const LightComponent& l) {
            return json{{"type", l.type == Render::LightType::Directional ? "directional" : "point"}, {"color", Vec(l.color)},
                        {"intensity", l.intensity}, {"range", l.range}, {"castShadows", l.castShadows}};
        },
        [](const json& j) {
            LightComponent l;
            l.type = j.value("type", std::string("point")) == "directional" ? Render::LightType::Directional : Render::LightType::Point;
            l.color = ReadVec(j, "color", l.color);
            l.intensity = j.value("intensity", l.intensity);
            l.range = j.value("range", l.range);
            l.castShadows = j.value("castShadows", l.castShadows);
            return l;
        });
    Builtin<RigidBody>("RigidBody",
        [](const RigidBody& b) {
            return json{{"type", static_cast<uint32_t>(b.type)}, {"mass", b.mass}, {"friction", b.friction},
                        {"restitution", b.restitution}, {"linearDamping", b.linearDamping}, {"angularDamping", b.angularDamping},
                        {"initialVelocity", Vec(b.initialVelocity)}};
        },
        [](const json& j) {
            RigidBody b;
            b.type = static_cast<BodyType>(std::min(j.value("type", 1u), 2u));
            b.mass = j.value("mass", b.mass);
            b.friction = j.value("friction", b.friction);
            b.restitution = j.value("restitution", b.restitution);
            b.linearDamping = j.value("linearDamping", b.linearDamping);
            b.angularDamping = j.value("angularDamping", b.angularDamping);
            b.initialVelocity = ReadVec(j, "initialVelocity", b.initialVelocity);
            return b;
        });
    Builtin<Collider>("Collider",
        [](const Collider& c) {
            return json{{"shape", static_cast<uint32_t>(c.shape)}, {"size", Vec(c.size)}, {"offset", Vec(c.offset)}};
        },
        [](const json& j) {
            Collider c;
            c.shape = static_cast<ColliderShape>(std::min(j.value("shape", 0u), 2u));
            c.size = ReadVec(j, "size", c.size);
            c.offset = ReadVec(j, "offset", c.offset);
            return c;
        });
}

void EnsureBuiltins() {
    static std::once_flag once;
    std::call_once(once, RegisterBuiltins);
}

} // namespace

void RegisterComponentSerializer(ComponentSerializer serializer) {
    EnsureBuiltins();
    AddOrReplace(std::move(serializer));
}

const std::vector<ComponentSerializer>& GetComponentSerializers() {
    EnsureBuiltins();
    return Serializers();
}

std::string SerializeScene(const Scene& scene) {
    EnsureBuiltins();
    const Registry& reg = scene.GetRegistry();
    json root;
    root["format"] = "astral-scene";
    root["version"] = kFormatVersion;
    root["name"] = scene.name;
    const auto& env = scene.environment;
    root["environment"] = {{"zenithColor", Vec(env.zenithColor)}, {"horizonColor", Vec(env.horizonColor)},
                           {"groundColor", Vec(env.groundColor)}, {"skyIntensity", env.skyIntensity},
                           {"ambientIntensity", env.ambientIntensity}, {"exposure", env.exposure}};

    json entities = json::array();
    for (Entity e : reg.Entities()) {
        const auto* id = reg.TryGet<IdComponent>(e);
        if (!id) continue; // runtime-only helpers without identity are not saved
        json entity;
        entity["id"] = std::to_string(id->id);
        entity["name"] = std::string(scene.GetName(e));
        if (Entity parent = scene.GetParent(e)) {
            if (const auto* pid = reg.TryGet<IdComponent>(parent)) entity["parent"] = std::to_string(pid->id);
        }
        json components = json::object();
        for (const auto& s : GetComponentSerializers())
            if (s.has(reg, e)) components[s.name] = s.save(reg, e);
        entity["components"] = components;
        entities.push_back(entity);
    }
    root["entities"] = entities;
    return root.dump(2);
}

bool DeserializeScene(std::string_view text, Scene& scene, std::string& error) {
    EnsureBuiltins();
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded()) {
        error = "not valid JSON";
        return false;
    }
    if (root.value("format", std::string()) != "astral-scene") {
        error = "not an Astral scene file";
        return false;
    }
    if (root.value("version", 0) > kFormatVersion) {
        error = "scene was saved by a newer version of Astral";
        return false;
    }

    try {
        Registry& reg = scene.GetRegistry();
        reg.Clear();
        scene.name = root.value("name", std::string("Untitled"));
        if (root.contains("environment")) {
            const json& j = root["environment"];
            auto& env = scene.environment;
            env.zenithColor = ReadVec(j, "zenithColor", env.zenithColor);
            env.horizonColor = ReadVec(j, "horizonColor", env.horizonColor);
            env.groundColor = ReadVec(j, "groundColor", env.groundColor);
            env.skyIntensity = j.value("skyIntensity", env.skyIntensity);
            env.ambientIntensity = j.value("ambientIntensity", env.ambientIntensity);
            env.exposure = j.value("exposure", env.exposure);
        }

        std::vector<std::pair<Entity, std::string>> parents;
        for (const json& je : root.value("entities", json::array())) {
            uint64_t id = std::stoull(je.value("id", std::string("0")));
            Entity e = scene.CreateEntityWithId(id ? id : static_cast<uint64_t>(reg.AliveCount() + 1), je.value("name", std::string("Entity")));
            if (je.contains("parent")) parents.emplace_back(e, je["parent"].get<std::string>());
            const json& components = je.value("components", json::object());
            for (const auto& s : GetComponentSerializers())
                if (components.contains(s.name)) s.load(reg, e, components[s.name]);
        }
        for (auto& [child, parentId] : parents) {
            Entity parent = scene.FindById(std::stoull(parentId));
            if (parent) scene.SetParent(child, parent, false);
        }
    } catch (const std::exception& ex) {
        error = std::string("malformed scene: ") + ex.what();
        return false;
    }
    return true;
}

bool SaveScene(const Scene& scene, const std::filesystem::path& path, std::string& error) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "cannot write " + path.string();
        return false;
    }
    out << SerializeScene(scene);
    return static_cast<bool>(out);
}

std::unique_ptr<Scene> LoadScene(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path.string();
        return nullptr;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    auto scene = std::make_unique<Scene>();
    if (!DeserializeScene(buffer.str(), *scene, error)) return nullptr;
    return scene;
}

} // namespace Astral
