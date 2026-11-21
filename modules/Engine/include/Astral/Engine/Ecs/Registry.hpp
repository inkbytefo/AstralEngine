#pragma once

#include "Astral/Core/Assert.hpp"
#include "Astral/Core/Handle.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Astral {

using Entity = Handle<struct EntityTag>;

namespace Detail {

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual bool Contains(Entity e) const = 0;
    virtual void Remove(Entity e) = 0;
    virtual size_t Size() const = 0;
    virtual void Clear() = 0;
};

// Sparse set: dense arrays of components and owners, plus a sparse index per entity slot.
// Iteration is over contiguous memory; add/remove are O(1) (swap-and-pop).
template <typename T>
class ComponentPool final : public IComponentPool {
public:
    static constexpr uint32_t kEmpty = 0xFFFFFFFFu;

    template <typename... Args>
    T& Emplace(Entity e, Args&&... args) {
        if (e.index >= m_Sparse.size()) m_Sparse.resize(e.index + 1, kEmpty);
        uint32_t& slot = m_Sparse[e.index];
        if (slot != kEmpty && m_Owners[slot] == e) {
            m_Dense[slot] = T{std::forward<Args>(args)...};
            return m_Dense[slot];
        }
        slot = static_cast<uint32_t>(m_Dense.size());
        m_Owners.push_back(e);
        m_Dense.push_back(T{std::forward<Args>(args)...});
        return m_Dense.back();
    }

    bool Contains(Entity e) const override {
        return e.index < m_Sparse.size() && m_Sparse[e.index] != kEmpty && m_Owners[m_Sparse[e.index]] == e;
    }

    T* TryGet(Entity e) { return Contains(e) ? &m_Dense[m_Sparse[e.index]] : nullptr; }
    const T* TryGet(Entity e) const { return Contains(e) ? &m_Dense[m_Sparse[e.index]] : nullptr; }

    void Remove(Entity e) override {
        if (!Contains(e)) return;
        uint32_t slot = m_Sparse[e.index];
        uint32_t last = static_cast<uint32_t>(m_Dense.size() - 1);
        if (slot != last) {
            m_Dense[slot] = std::move(m_Dense[last]);
            m_Owners[slot] = m_Owners[last];
            m_Sparse[m_Owners[slot].index] = slot;
        }
        m_Dense.pop_back();
        m_Owners.pop_back();
        m_Sparse[e.index] = kEmpty;
    }

    size_t Size() const override { return m_Dense.size(); }
    void Clear() override {
        m_Dense.clear();
        m_Owners.clear();
        m_Sparse.clear();
    }

    const std::vector<Entity>& Owners() const { return m_Owners; }
    std::vector<T>& Components() { return m_Dense; }

private:
    std::vector<T> m_Dense;
    std::vector<Entity> m_Owners;
    std::vector<uint32_t> m_Sparse;
};

} // namespace Detail

// Entity-component registry. Entities are generational handles; components are plain
// structs stored per type in sparse sets.
//
//     Entity e = registry.Create();
//     registry.Add<Transform>(e, Transform{...});
//     registry.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) { ... });
class Registry {
public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = default;
    Registry& operator=(Registry&&) = default;

    Entity Create() {
        uint32_t index;
        if (!m_Free.empty()) {
            index = m_Free.back();
            m_Free.pop_back();
        } else {
            index = static_cast<uint32_t>(m_Generations.size());
            m_Generations.push_back(1);
        }
        ++m_Alive;
        return {index, m_Generations[index]};
    }

    void Destroy(Entity e) {
        if (!Valid(e)) return;
        for (auto& [type, pool] : m_Pools) pool->Remove(e);
        if (++m_Generations[e.index] == 0) m_Generations[e.index] = 1;
        m_Free.push_back(e.index);
        --m_Alive;
    }

    bool Valid(Entity e) const {
        return e.IsValid() && e.index < m_Generations.size() && m_Generations[e.index] == e.generation;
    }

    // The live entity currently occupying slot `index`, if any.
    Entity FromIndex(uint32_t index) const {
        if (index >= m_Generations.size()) return {};
        Entity e{index, m_Generations[index]};
        for (uint32_t free : m_Free)
            if (free == index) return {};
        return e;
    }

    template <typename T, typename... Args>
    T& Add(Entity e, Args&&... args) {
        ASTRAL_ASSERT(Valid(e), "Add on an invalid entity");
        return Pool<T>().Emplace(e, std::forward<Args>(args)...);
    }

    template <typename T>
    T& Get(Entity e) {
        T* c = TryGet<T>(e);
        ASTRAL_ASSERT(c, "entity does not have the requested component");
        return *c;
    }

    template <typename T>
    T* TryGet(Entity e) {
        auto* pool = FindPool<T>();
        return pool && Valid(e) ? pool->TryGet(e) : nullptr;
    }
    template <typename T>
    const T* TryGet(Entity e) const {
        auto* pool = FindPool<T>();
        return pool && Valid(e) ? pool->TryGet(e) : nullptr;
    }

    template <typename T>
    bool Has(Entity e) const { return TryGet<T>(e) != nullptr; }

    template <typename T>
    void Remove(Entity e) {
        if (auto* pool = FindPool<T>()) pool->Remove(e);
    }

    template <typename T>
    size_t Count() const {
        auto* pool = FindPool<T>();
        return pool ? pool->Size() : 0;
    }

    // Calls fn(Entity, First&, Rest&...) for every entity that has all the listed components.
    // Iterates the first type's pool; do not add/remove that type inside fn.
    template <typename First, typename... Rest, typename Fn>
    void Each(Fn&& fn) {
        auto* first = FindPool<First>();
        if (!first) return;
        // Copy owners: fn may add other components (which can reallocate other pools).
        std::vector<Entity> owners = first->Owners();
        for (Entity e : owners) {
            First* a = first->TryGet(e);
            if (!a) continue;
            if constexpr (sizeof...(Rest) == 0) {
                fn(e, *a);
            } else {
                auto rest = std::make_tuple(TryGet<Rest>(e)...);
                if (!std::apply([](auto*... p) { return (p && ...); }, rest)) continue;
                std::apply([&](auto*... p) { fn(e, *a, *p...); }, rest);
            }
        }
    }

    // All live entities, in slot order.
    std::vector<Entity> Entities() const {
        std::vector<Entity> out;
        std::vector<bool> free(m_Generations.size(), false);
        for (uint32_t f : m_Free) free[f] = true;
        for (uint32_t i = 0; i < m_Generations.size(); ++i)
            if (!free[i]) out.push_back({i, m_Generations[i]});
        return out;
    }

    uint32_t AliveCount() const { return m_Alive; }

    void Clear() {
        for (auto& [type, pool] : m_Pools) pool->Clear();
        m_Generations.clear();
        m_Free.clear();
        m_Alive = 0;
    }

private:
    template <typename T>
    Detail::ComponentPool<T>& Pool() {
        auto& slot = m_Pools[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<Detail::ComponentPool<T>>();
        return static_cast<Detail::ComponentPool<T>&>(*slot);
    }
    template <typename T>
    Detail::ComponentPool<T>* FindPool() const {
        auto it = m_Pools.find(std::type_index(typeid(T)));
        return it == m_Pools.end() ? nullptr : static_cast<Detail::ComponentPool<T>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<Detail::IComponentPool>> m_Pools;
    std::vector<uint32_t> m_Generations;
    std::vector<uint32_t> m_Free;
    uint32_t m_Alive = 0;
};

} // namespace Astral
