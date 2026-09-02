#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Astral {

using Entity = std::uint32_t;

struct TransformComponent { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct VelocityComponent { float dx = 0.0f, dy = 0.0f, dz = 0.0f; };
struct HealthComponent { int hp = 100; };

template <typename T>
class SparseSet {
public:
    using size_type = std::size_t;

    // "Kayit yok" isaret degeri
    static constexpr size_type EMPTY = static_cast<size_type>(-1);

    // ---- Sorgu ----
    bool Has(Entity entity) const {
        return entity < mSparse.size() &&
               mSparse[entity] != EMPTY &&
               mEntities[mSparse[entity]] == entity;
    }

    std::size_t Size() const { return mData.size(); }
    bool Empty() const { return mData.empty(); }

    // GPU'ya ham kopya icin (memcpy dostu)
    const std::vector<T>& Data() const { return mData; }
    const std::vector<Entity>& Entities() const { return mEntities; }

    // ---- Mutasyon ----
    void Add(Entity entity, T component) {
        if (entity >= mSparse.size()) {
            mSparse.resize(static_cast<std::size_t>(entity) + 1, EMPTY);
        }
        if (Has(entity)) {
            mData[mSparse[entity]] = std::move(component);
            return;
        }
        mSparse[entity] = mData.size();
        mEntities.push_back(entity);
        mData.push_back(std::move(component));
    }

    T& Get(Entity entity) {
        // Anlasma: cagiran Has() ile dogruladi
        assert(Has(entity));
        return mData[mSparse[entity]];
    }

    const T& Get(Entity entity) const {
        assert(Has(entity));
        return mData[mSparse[entity]];
    }

    // O(1) swap-and-pop: bosluk son elemanla doldurulur, sondan atilir
    void Remove(Entity entity) {
        if (!Has(entity)) return;

        const size_type index = mSparse[entity];
        const size_type last  = mData.size() - 1;

        if (index != last) {
            const Entity movedEntity = mEntities[last];
            mEntities[index] = movedEntity;
            mData[index]     = std::move(mData[last]);
            mSparse[movedEntity] = index;
        }
        mEntities.pop_back();
        mData.pop_back();
        mSparse[entity] = EMPTY;
    }

    void Clear() {
        mSparse.clear();
        mEntities.clear();
        mData.clear();
    }

// ================= OZEL ITERATOR (Entity, T&) =================
    // Veriler iki ayri dizide (Entities/Data) yasadigi icin iterator,
    // referans tasiyan bir "proxy" nesnesi dondurur:
    //   auto&& [entity, component] : view  -> entity, gercek T&
    struct PairRef {
        Entity entity;
        T& component;
    };

    struct ConstPairRef {
        Entity entity;
        const T& component;
    };

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<Entity, T>;
        using pointer           = void;
        using reference         = PairRef;

        Iterator(SparseSet* owner, size_type index)
            : mOwner(owner), mIndex(index) {}

        PairRef operator*() const {
            return PairRef{ mOwner->mEntities[mIndex], mOwner->mData[mIndex] };
        }

        Iterator& operator++() { ++mIndex; return *this; }
        Iterator operator++(int) { Iterator copy = *this; ++(*this); return copy; }

        friend bool operator==(const Iterator& a, const Iterator& b) {
            return a.mIndex == b.mIndex;
        }
        friend bool operator!=(const Iterator& a, const Iterator& b) {
            return !(a == b);
        }

    private:
        SparseSet* mOwner = nullptr;
        size_type  mIndex = 0;
    };

    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<Entity, T>;
        using pointer           = void;
        using reference         = ConstPairRef;

        ConstIterator(const SparseSet* owner, size_type index)
            : mOwner(owner), mIndex(index) {}

        ConstPairRef operator*() const {
            return ConstPairRef{ mOwner->mEntities[mIndex], mOwner->mData[mIndex] };
        }

        ConstIterator& operator++() { ++mIndex; return *this; }
        ConstIterator operator++(int) { ConstIterator copy = *this; ++(*this); return copy; }

        friend bool operator==(const ConstIterator& a, const ConstIterator& b) {
            return a.mIndex == b.mIndex;
        }
        friend bool operator!=(const ConstIterator& a, const ConstIterator& b) {
            return !(a == b);
        }

    private:
        const SparseSet* mOwner = nullptr;
        size_type        mIndex = 0;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, mData.size()); }

    ConstIterator begin() const { return ConstIterator(this, 0); }
    ConstIterator end() const   { return ConstIterator(this, mData.size()); }

private:
    std::vector<size_type> mSparse;   // entity -> dense indeks
    std::vector<Entity>    mEntities; // dense: kimlikler (kontigu)
    std::vector<T>         mData;     // dense: bilesen verisi (kontigu)
};

// ========================= TYPE ERASURE KATMANI =========================
// Farkli tiplerdeki SparseSet'leri tek haritada tutmak icin ortak arayuz.
// Registry'nin DestroyEntity() sinifini tum havuzlara bildirmesini saglar.
class IPool {
public:
    virtual ~IPool() = default;
    virtual void RemoveEntity(Entity entity) = 0;
    virtual void Clear() = 0;
    virtual std::size_t Size() const = 0;
};

template <typename T>
class Pool : public IPool {
public:
    SparseSet<T> set;

    void RemoveEntity(Entity entity) override { set.Remove(entity); }
    void Clear() override { set.Clear(); }
    std::size_t Size() const override { return set.Size(); }
};

// =============================== REGISTRY ===============================
class Registry {
private:
    Entity nextEntityId = 0;
    // std::type_index ile bilesen tipine gore ilgili havuzu buluyoruz
    std::unordered_map<std::type_index, std::shared_ptr<IPool>> pools;

    template <typename T>
    Pool<T>& GetOrCreatePool() {
        const std::type_index type = std::type_index(typeid(T));
        auto it = pools.find(type);
        if (it != pools.end()) {
            return *static_cast<Pool<T>*>(it->second.get());
        }
        auto pool = std::make_shared<Pool<T>>();
        Pool<T>& ref = *pool;
        pools.emplace(type, std::move(pool));
        return ref;
    }

public:
    Entity CreateEntity() { return nextEntityId++; }

    template <typename T>
    void AddComponent(Entity entity, T component) {
        GetOrCreatePool<T>().set.Add(entity, std::move(component));
    }

    template <typename T>
    T& GetComponent(Entity entity) {
        return GetOrCreatePool<T>().set.Get(entity);
    }

    template <typename T>
    bool HasComponent(Entity entity) const {
        const auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return false;
        return static_cast<const Pool<T>*>(it->second.get())->set.Has(entity);
    }

    // Sistemlerin tum verilere ulasmasi (cache dostu dense dizi)
    template <typename T>
    SparseSet<T>& GetView() {
        return GetOrCreatePool<T>().set;
    }

    // O(1) swap-and-pop silme. Basariliysa true.
    template <typename T>
    bool RemoveComponent(Entity entity) {
        const auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return false;

        auto* pool = static_cast<Pool<T>*>(it->second.get());
        if (!pool->set.Has(entity)) return false;

        pool->set.Remove(entity);
        return true;
    }

    // Entity'yi tum havuzlardan kaldirir.
    void DestroyEntity(Entity entity) {
        for (auto& entry : pools) {
            entry.second->RemoveEntity(entity);
        }
    }

    std::size_t PoolCount() const { return pools.size(); }
};

} // namespace Astral