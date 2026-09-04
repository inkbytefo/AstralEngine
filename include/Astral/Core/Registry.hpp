#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <iostream>
#include <limits>

#include "Astral/Core/Components.hpp"

namespace Astral {

using EntityHandle = std::uint64_t;
using EntityIndex = std::uint32_t;
using EntityGeneration = std::uint32_t;
using EntityID = EntityHandle; // Backwards-compatible alias

constexpr EntityHandle NullEntityHandle = 0xFFFFFFFFFFFFFFFFull;
constexpr EntityHandle NullEntity = NullEntityHandle;

[[nodiscard]] constexpr inline EntityIndex GetEntityIndex(EntityHandle handle) noexcept {
    return static_cast<EntityIndex>(handle & 0xFFFFFFFFull);
}

[[nodiscard]] constexpr inline EntityGeneration GetEntityGeneration(EntityHandle handle) noexcept {
    return static_cast<EntityGeneration>((handle >> 32) & 0xFFFFFFFFull);
}

[[nodiscard]] constexpr inline EntityHandle MakeEntityHandle(EntityIndex index, EntityGeneration generation) noexcept {
    return (static_cast<EntityHandle>(generation) << 32) | static_cast<EntityHandle>(index);
}

template <typename T>
class SparseSet {
public:
    using size_type = std::size_t;

    // "Kayit yok" isaret degeri
    static constexpr size_type EMPTY = static_cast<size_type>(-1);

    // ---- Sorgu ----
    bool Has(EntityID entity) const {
        const auto index = GetEntityIndex(entity);
        return index < mSparse.size() &&
               mSparse[index] != EMPTY &&
               mEntities[mSparse[index]] == entity;
    }

    [[nodiscard]] bool Contains(EntityID entity) const { return Has(entity); }

    std::size_t Size() const { return mData.size(); }
    bool Empty() const { return mData.empty(); }

    // GPU'ya ham kopya icin (memcpy dostu)
    const std::vector<T>& Data() const { return mData; }
    const std::vector<EntityID>& Entities() const { return mEntities; }

    // ---- Mutasyon ----
    void Add(EntityID entity, T component) {
        const auto index = GetEntityIndex(entity);
        if (index >= mSparse.size()) {
            mSparse.resize(static_cast<std::size_t>(index) + 1, EMPTY);
        }
        if (Has(entity)) {
            mData[mSparse[index]] = std::move(component);
            return;
        }
        mSparse[index] = mData.size();
        mEntities.push_back(entity);
        mData.push_back(std::move(component));
    }

    void Insert(EntityID entity, T component) { Add(entity, std::move(component)); }

    T& Get(EntityID entity) {
        assert(Has(entity));
        return mData[mSparse[GetEntityIndex(entity)]];
    }

    const T& Get(EntityID entity) const {
        assert(Has(entity));
        return mData[mSparse[GetEntityIndex(entity)]];
    }

    // O(1) swap-and-pop: bosluk son elemanla doldurulur, sondan atilir
    void Remove(EntityID entity) {
        if (!Has(entity)) return;

        const auto entityIndex = GetEntityIndex(entity);
        const size_type index = mSparse[entityIndex];
        const size_type last  = mData.size() - 1;

        if (index != last) {
            const EntityID movedEntity = mEntities[last];
            mEntities[index] = movedEntity;
            mData[index]     = std::move(mData[last]);
            mSparse[GetEntityIndex(movedEntity)] = index;
        }
        mEntities.pop_back();
        mData.pop_back();
        mSparse[entityIndex] = EMPTY;
    }

    void Clear() {
        mSparse.clear();
        mEntities.clear();
        mData.clear();
    }

    const T* RawData() const noexcept { return mData.data(); }
    T* RawData() noexcept { return mData.data(); }

    /// DOD Direct Bulk Invariant Construction: Assigns contiguous mEntities and mData
    /// and reconstructs the sparse lookup table in O(N).
    void AssignDirect(std::vector<EntityID> entities, std::vector<T> data) {
        assert(entities.size() == data.size() && "Entity ve Component boyutlari birebir eslesmelidir!");
        mEntities = std::move(entities);
        mData = std::move(data);
        RebuildSparse();
    }

    /// Rebuilds mSparse mapping such that mSparse[GetEntityIndex(mEntities[i])] == i
    void RebuildSparse() {
        mSparse.clear();
        if (mEntities.empty()) return;

        EntityID maxEntity = 0;
        for (EntityID id : mEntities) {
            const auto index = GetEntityIndex(id);
            if (index > maxEntity) {
                maxEntity = index;
            }
        }

        mSparse.assign(static_cast<std::size_t>(maxEntity) + 1, EMPTY);
        for (std::size_t i = 0; i < mEntities.size(); ++i) {
            const auto index = GetEntityIndex(mEntities[i]);
            mSparse[index] = i;
        }
    }

// ================= OZEL ITERATOR (EntityID, T&) =================
    // Veriler iki ayri dizide (Entities/Data) yasadigi icin iterator,
    // referans tasiyan bir "proxy" nesnesi dondurur:
    //   auto&& [entity, component] : view  -> entity, gercek T&
    struct PairRef {
        EntityID entity;
        T& component;
    };

    struct ConstPairRef {
        EntityID entity;
        const T& component;
    };

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<EntityID, T>;
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
        using value_type        = std::pair<EntityID, T>;
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
    std::vector<size_type> mSparse;     // entity -> dense indeks
    std::vector<EntityID>  mEntities;   // dense: kimlikler (kontigu)
    std::vector<T>         mData;       // dense: bilesen verisi (kontigu)
};

// ========================= TYPE ERASURE KATMANI =========================
// Farkli tiplerdeki SparseSet'leri tek haritada tutmak icin ortak arayuz.
// Registry'nin DestroyEntity() sinifini tum havuzlara bildirmesini saglar.
class IPool {
public:
    virtual ~IPool() = default;
    virtual void RemoveEntity(EntityID entity) = 0;
    virtual void Clear() = 0;
    virtual std::size_t Size() const = 0;
    virtual std::shared_ptr<IPool> Clone() const = 0;

    virtual uint64_t GetTypeHash() const = 0;
    virtual bool IsTriviallyCopyable() const = 0;
    virtual std::size_t GetComponentSize() const = 0;
    virtual const void* GetRawData() const = 0;
    virtual const void* GetRawEntityData() const = 0;
    virtual std::size_t GetEntityDataSize() const = 0;
};

template <typename T>
class Pool : public IPool {
public:
    SparseSet<T> set;

    void RemoveEntity(EntityID entity) override { set.Remove(entity); }
    void Clear() override { set.Clear(); }
    std::size_t Size() const override { return set.Size(); }
    std::shared_ptr<IPool> Clone() const override {
        auto copy = std::make_shared<Pool<T>>();
        copy->set = this->set;
        return copy;
    }

    uint64_t GetTypeHash() const override {
        return ComponentTraits<T>::TypeHash;
    }

    bool IsTriviallyCopyable() const override {
        return std::is_trivially_copyable_v<T>;
    }

    std::size_t GetComponentSize() const override {
        return sizeof(T);
    }

    const void* GetRawData() const override {
        return set.Data().data();
    }

    const void* GetRawEntityData() const override {
        return set.Entities().data();
    }

    std::size_t GetEntityDataSize() const override {
        return set.Entities().size() * sizeof(EntityID);
    }
};

// =============================== REGISTRY ===============================
class Registry {
private:
    std::vector<EntityGeneration> m_Generations;
    std::vector<EntityIndex> m_FreeIndices;
    uint32_t m_AliveCount = 0;

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
    Registry() = default;

    /// Derin kopyalama (Deep-Copy): Editor durumundan Runtime durumuna gecerken sahneyi klonlar
    Registry(const Registry& other)
        : m_Generations(other.m_Generations),
          m_FreeIndices(other.m_FreeIndices),
          m_AliveCount(other.m_AliveCount) {
        for (const auto& [type, pool] : other.pools) {
            pools.emplace(type, pool->Clone());
        }
    }

    Registry& operator=(const Registry& other) {
        if (this != &other) {
            m_Generations = other.m_Generations;
            m_FreeIndices = other.m_FreeIndices;
            m_AliveCount = other.m_AliveCount;
            pools.clear();
            for (const auto& [type, pool] : other.pools) {
                pools.emplace(type, pool->Clone());
            }
        }
        return *this;
    }

    Registry(Registry&&) noexcept = default;
    Registry& operator=(Registry&&) noexcept = default;

    /// O(1) Free-List Geri Donusumlu Generational Varlik Tahsisi
    EntityHandle CreateEntity() {
        EntityIndex index = 0;
        EntityGeneration generation = 1;

        if (!m_FreeIndices.empty()) {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
            generation = m_Generations[index];
        } else {
            index = static_cast<EntityIndex>(m_Generations.size());
            m_Generations.push_back(1);
            generation = 1;
        }

        m_AliveCount++;
        return MakeEntityHandle(index, generation);
    }

    /// O(1) Generational Handle Gecerlilik Sorgusu (Ghost Mutation Bariyeri)
    [[nodiscard]] bool IsAlive(EntityHandle handle) const noexcept {
        if (handle == NullEntityHandle) return false;
        const EntityIndex index = GetEntityIndex(handle);
        const EntityGeneration gen = GetEntityGeneration(handle);
        if (index >= m_Generations.size() || gen == 0) return false;
        return m_Generations[index] == gen;
    }

    template <typename T>
    void AddComponent(EntityHandle entity, T component) {
        GetOrCreatePool<T>().set.Add(entity, std::move(component));
    }

    template <typename T>
    T& GetComponent(EntityHandle entity) {
        return GetOrCreatePool<T>().set.Get(entity);
    }

    template <typename T>
    const T& GetComponent(EntityHandle entity) const {
        const auto it = pools.find(std::type_index(typeid(T)));
        assert(it != pools.end());
        return static_cast<const Pool<T>*>(it->second.get())->set.Get(entity);
    }

    template <typename T>
    bool HasComponent(EntityHandle entity) const {
        if (!IsAlive(entity)) return false;
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
    bool RemoveComponent(EntityHandle entity) {
        if (!IsAlive(entity)) return false;
        const auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return false;

        auto* pool = static_cast<Pool<T>*>(it->second.get());
        if (!pool->set.Has(entity)) return false;

        pool->set.Remove(entity);
        return true;
    }

    /// Entity'yi tum bilesen havuzlarindan kaldirir, generation artirir ve slotu free-list'e iade eder
    void DestroyEntity(EntityHandle handle) {
        if (!IsAlive(handle)) return;

        // 1. Tum bilesen havuzlarindan cikar
        for (auto& entry : pools) {
            entry.second->RemoveEntity(handle);
        }

        // 2. Generation artir ve slotu free-list'e iade et
        const EntityIndex index = GetEntityIndex(handle);
        if (m_Generations[index] == std::numeric_limits<EntityGeneration>::max()) {
            m_Generations[index] = 0; // Overflow guvenlik politikasi: Slot sonsuza dek emekliye ayrilir
        } else {
            m_Generations[index]++;
            m_FreeIndices.push_back(index);
        }

        if (m_AliveCount > 0) {
            m_AliveCount--;
        }
    }

    void Clear() {
        for (auto& entry : pools) {
            entry.second->Clear();
        }
        m_Generations.clear();
        m_FreeIndices.clear();
        m_AliveCount = 0;
    }

    std::size_t PoolCount() const { return pools.size(); }

    void Swap(Registry& other) noexcept {
        m_Generations.swap(other.m_Generations);
        m_FreeIndices.swap(other.m_FreeIndices);
        std::swap(m_AliveCount, other.m_AliveCount);
        pools.swap(other.pools);
    }

    /// Deserialization sonrasi kimlik tablosunu ve free-list'i deterministik olarak ayaga kaldirir
    void RebuildIdentityFromEntities(const std::vector<EntityHandle>& allEntities) {
        m_Generations.clear();
        m_FreeIndices.clear();
        m_AliveCount = 0;

        EntityIndex maxIndex = 0;
        bool hasEntities = !allEntities.empty();
        for (EntityHandle handle : allEntities) {
            const EntityIndex idx = GetEntityIndex(handle);
            if (idx > maxIndex) {
                maxIndex = idx;
            }
        }

        if (hasEntities) {
            m_Generations.assign(maxIndex + 1, 0);
            for (EntityHandle handle : allEntities) {
                const EntityIndex idx = GetEntityIndex(handle);
                const EntityGeneration gen = GetEntityGeneration(handle);
                if (m_Generations[idx] == 0) {
                    m_AliveCount++;
                }
                m_Generations[idx] = gen;
            }

            for (EntityIndex i = 0; i <= maxIndex; ++i) {
                if (m_Generations[i] == 0) {
                    m_Generations[i] = 1;
                    m_FreeIndices.push_back(i);
                }
            }
        }
    }

    [[nodiscard]] uint32_t GetAliveEntityCount() const noexcept { return m_AliveCount; }
    [[nodiscard]] uint32_t GetTotalSlotCount() const noexcept { return static_cast<uint32_t>(m_Generations.size()); }
    [[nodiscard]] uint32_t GetNextEntityId() const noexcept { return static_cast<uint32_t>(m_Generations.size()); }
    void SetNextEntityId(uint32_t count) noexcept {
        if (m_Generations.size() < count) {
            m_Generations.resize(count, 1);
        }
    }
    [[nodiscard]] const std::unordered_map<std::type_index, std::shared_ptr<IPool>>& GetPools() const noexcept { return pools; }
};

} // namespace Astral