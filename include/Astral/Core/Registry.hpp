#pragma once
#include <cstdint>
#include <unordered_map>

namespace Astral {

// Entity sadece kimlik numarasıdır
using Entity = std::uint32_t;

// Saf veri bileşenleri (Hiçbir fonksiyon içermezler)
struct TransformComponent {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct VelocityComponent {
    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
};

class Registry {
private:
    Entity nextEntityId = 0;

    // Bileşenleri Entity ID'leri ile eşleştiriyoruz
    std::unordered_map<Entity, TransformComponent> transforms;
    std::unordered_map<Entity, VelocityComponent> velocities;

public:
    Entity CreateEntity() {
        return nextEntityId++;
    }

    void AddTransform(Entity entity, TransformComponent transform) {
        transforms[entity] = transform;
    }

    void AddVelocity(Entity entity, VelocityComponent velocity) {
        velocities[entity] = velocity;
    }

    TransformComponent& GetTransform(Entity entity) {
        return transforms[entity];
    }

    VelocityComponent& GetVelocity(Entity entity) {
        return velocities[entity];
    }

    bool HasVelocity(Entity entity) {
        return velocities.find(entity) != velocities.end();
    }

    auto& GetTransforms() { return transforms; }
};

} // namespace Astral