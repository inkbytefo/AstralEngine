#pragma once
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace Astral {

using Entity = std::uint32_t;

struct TransformComponent { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct VelocityComponent { float dx = 0.0f, dy = 0.0f, dz = 0.0f; };
struct HealthComponent { int hp = 100; }; // Yeni bir bilesen test icin

// 1. Temel Arayuz (Farkli tipleri tek bir haritada tutabilmek icin)
class IPool {
public:
    virtual ~IPool() = default;
};

// 2. Ozel Veri Havuzu
template <typename T>
class Pool : public IPool {
public:
    std::unordered_map<Entity, T> data;
};

class Registry {
private:
    Entity nextEntityId = 0;
    // std::type_index ile bilesenin tipine gore ilgili havuzu buluyoruz
    std::unordered_map<std::type_index, std::shared_ptr<IPool>> pools;

public:
    Entity CreateEntity() {
        return nextEntityId++;
    }

    template <typename T>
    void AddComponent(Entity entity, T component) {
        auto type = std::type_index(typeid(T));
        // Eger bu tip icin bir havuz yoksa, yeni bir tane olustur
        if (pools.find(type) == pools.end()) {
            pools[type] = std::make_shared<Pool<T>>();
        }
        // Arayuzden (IPool) asil veri tipine (Pool<T>) donusum yap
        std::static_pointer_cast<Pool<T>>(pools[type])->data[entity] = component;
    }

    template <typename T>
    T& GetComponent(Entity entity) {
        auto type = std::type_index(typeid(T));
        return std::static_pointer_cast<Pool<T>>(pools[type])->data[entity];
    }

    template <typename T>
    bool HasComponent(Entity entity) {
        auto type = std::type_index(typeid(T));
        if (pools.find(type) == pools.end()) return false;

        auto pool = std::static_pointer_cast<Pool<T>>(pools[type]);
        return pool->data.find(entity) != pool->data.end();
    }

    // Sistemlerin tum verilere ulasmasi icin
    template <typename T>
    std::unordered_map<Entity, T>& GetView() {
        auto type = std::type_index(typeid(T));
        // Havuz yoksa bos bir havuz yaratip dondur (hata almamak icin)
        if (pools.find(type) == pools.end()) {
            pools[type] = std::make_shared<Pool<T>>();
        }
        return std::static_pointer_cast<Pool<T>>(pools[type])->data;
    }
};

} // namespace Astral