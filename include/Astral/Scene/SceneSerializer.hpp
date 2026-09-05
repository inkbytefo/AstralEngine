#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <iosfwd>
#include <istream>
#include <ostream>
#include <vector>

#include "Astral/Core/Registry.hpp"

namespace Astral {

class Scene;

template <typename T>
concept TriviallyCopyableComponent = std::is_trivially_copyable_v<T>;

namespace Endian {

template <typename T>
requires (sizeof(T) == 1)
constexpr T ToLittle(T val) noexcept { return val; }

template <typename T>
requires (sizeof(T) == 2)
constexpr T ToLittle(T val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return val;
    } else {
        uint16_t u = std::bit_cast<uint16_t>(val);
        u = static_cast<uint16_t>((u >> 8) | (u << 8));
        return std::bit_cast<T>(u);
    }
}

template <typename T>
requires (sizeof(T) == 4)
constexpr T ToLittle(T val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return val;
    } else {
        uint32_t u = std::bit_cast<uint32_t>(val);
        u = ((u >> 24) & 0x000000FF) |
            ((u >> 8)  & 0x0000FF00) |
            ((u << 8)  & 0x00FF0000) |
            ((u << 24) & 0xFF000000);
        return std::bit_cast<T>(u);
    }
}

template <typename T>
requires (sizeof(T) == 8)
constexpr T ToLittle(T val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return val;
    } else {
        uint64_t u = std::bit_cast<uint64_t>(val);
        u = ((u >> 56) & 0x00000000000000FFULL) |
            ((u >> 40) & 0x000000000000FF00ULL) |
            ((u >> 24) & 0x0000000000FF0000ULL) |
            ((u >> 8)  & 0x00000000FF000000ULL) |
            ((u << 8)  & 0x000000FF00000000ULL) |
            ((u << 24) & 0x0000FF0000000000ULL) |
            ((u << 40) & 0x00FF000000000000ULL) |
            ((u << 56) & 0xFF00000000000000ULL);
        return std::bit_cast<T>(u);
    }
}

template <typename T>
constexpr T FromLittle(T val) noexcept {
    return ToLittle(val);
}

} // namespace Endian

#pragma pack(push, 1)
/// Binary layout: File Header (12 bytes)
struct SceneFileHeader {
    char magic[4] = { 'A', 'S', 'T', 'R' }; // Magic Signature ("ASTR")
    uint32_t version = 3;                   // Version ID (v3 for explicit schema layout & LE byte order)
    uint32_t activeEntityCount = 0;          // Max Entity Index / Active Entity Count
};

/// Binary layout: Component Chunk Header (28 bytes)
struct ComponentChunkHeader {
    uint64_t typeId = 0;                    // Deterministic TypeHash / TypeID
    uint32_t version = 1;                   // Component Chunk Schema Version
    uint32_t flags = 0;                     // Component Flags
    uint32_t elementCount = 0;              // Element Count in contiguous buffers
    uint32_t entityDataSize = 0;            // Total bytes of contiguous EntityID[] blob
    uint32_t componentDataSize = 0;         // Total bytes of contiguous ComponentData[] blob
};
#pragma pack(pop)

/**
 * @brief High-performance Data-Oriented custom binary serialization module
 *        for AstralEngine C++20 SparseSet ECS.
 *
 * v3 format: Explicit field layout, standard Little-Endian byte order,
 * atomic file saving via temporary file replacement, and backwards-compatible v2 parsing.
 */
class SceneSerializer {
public:
    static constexpr uint32_t CURRENT_VERSION       = 3;
    static constexpr uint32_t MIN_SUPPORTED_VERSION = 2;
    static constexpr char MAGIC[4] = { 'A', 'S', 'T', 'R' };
    static constexpr uint64_t SCENE_METADATA_TYPE_ID = Detail::FNV1a64("SceneMetadata");
    static constexpr uint64_t ENTITY_TABLE_TYPE_ID   = Detail::FNV1a64("EntityTable");
    static constexpr uint32_t MAX_TAG_LENGTH        = 4096;
    static constexpr uint32_t MAX_SCENE_NAME_LENGTH = 1024;
    static constexpr uint32_t MAX_ELEMENT_COUNT     = 10'000'000;
    static constexpr uint64_t MAX_FILE_SIZE_BUDGET  = 1024ULL * 1024ULL * 1024ULL; // 1 GB

    /// Primary C++20 DOD Static Interfaces
    [[nodiscard]] static bool Serialize(const std::shared_ptr<Scene>& scene, const std::filesystem::path& filepath);
    [[nodiscard]] static bool Deserialize(const std::shared_ptr<Scene>& scene, const std::filesystem::path& filepath);
    [[nodiscard]] static bool Serialize(const Scene& scene, const std::filesystem::path& filepath);
    [[nodiscard]] static bool Deserialize(Scene& scene, const std::filesystem::path& filepath);

    /// Convenience instance constructor & methods
    explicit SceneSerializer(std::shared_ptr<Scene> scene) : m_Scene(std::move(scene)) {}
    ~SceneSerializer() = default;

    [[nodiscard]] bool Serialize(const std::filesystem::path& filepath) { return Serialize(m_Scene, filepath); }
    [[nodiscard]] bool Deserialize(const std::filesystem::path& filepath) { return Deserialize(m_Scene, filepath); }

    /// Runtime aliases for backwards-compatibility
    [[nodiscard]] bool SerializeRuntime(const std::filesystem::path& filepath) { return Serialize(m_Scene, filepath); }
    [[nodiscard]] bool DeserializeRuntime(const std::filesystem::path& filepath) { return Deserialize(m_Scene, filepath); }

    void SetScene(std::shared_ptr<Scene> scene) noexcept { m_Scene = std::move(scene); }
    [[nodiscard]] const std::shared_ptr<Scene>& GetScene() const noexcept { return m_Scene; }

    using ChunkDeserializerFn = bool(*)(Registry&, std::istream&, const ComponentChunkHeader&);
    static void RegisterChunkDeserializer(uint64_t typeId, ChunkDeserializerFn deserializer);

    using ChunkSerializerFn = bool(*)(const IPool&, std::ostream&);
    static void RegisterChunkSerializer(uint64_t typeId, ChunkSerializerFn serializer);

    template <TriviallyCopyableComponent T>
    static bool ReadChunkDirect(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
        static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable for bulk dump.");

        const uint32_t elementCount = chunkHeader.elementCount;
        if (elementCount > MAX_ELEMENT_COUNT) {
            return false;
        }

        std::vector<EntityID> entities(elementCount);
        std::vector<T> data(elementCount);

        if (elementCount > 0) {
            const std::streamsize entityBytes = static_cast<std::streamsize>(chunkHeader.entityDataSize);
            const std::streamsize componentBytes = static_cast<std::streamsize>(chunkHeader.componentDataSize);

            if (entityBytes != static_cast<std::streamsize>(elementCount * sizeof(EntityID)) ||
                componentBytes != static_cast<std::streamsize>(elementCount * sizeof(T))) {
                return false;
            }

            stream.read(reinterpret_cast<char*>(entities.data()), entityBytes);
            if (!stream || stream.gcount() != entityBytes) return false;

            for (EntityID& id : entities) {
                id = Endian::FromLittle(id);
            }

            stream.read(reinterpret_cast<char*>(data.data()), componentBytes);
            if (!stream || stream.gcount() != componentBytes) return false;
        }

        auto& view = registry.GetView<T>();
        view.AssignDirect(std::move(entities), std::move(data));
        return true;
    }

    /// Compile-time bulk chunk writer for a specific component pool
    template <TriviallyCopyableComponent T>
    static bool WriteComponentChunk(const SparseSet<T>& sparseSet, std::ostream& stream) {
        static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable.");
        const uint32_t elementCount = static_cast<uint32_t>(sparseSet.Size());
        if (elementCount == 0) return true;

        const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
        const uint32_t componentDataSize = static_cast<uint32_t>(elementCount * sizeof(T));

        const ComponentChunkHeader chunkHeader{
            .typeId = ComponentTraits<T>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = elementCount,
            .entityDataSize = entityDataSize,
            .componentDataSize = componentDataSize
        };

        stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ComponentChunkHeader));
        if (!stream) return false;

        for (const EntityHandle& h : sparseSet.Entities()) {
            EntityHandle hLE = Endian::ToLittle(h);
            stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
        }
        if (!stream) return false;

        stream.write(reinterpret_cast<const char*>(sparseSet.Data().data()), componentDataSize);
        return stream.good();
    }

    /// Compile-time checked manual chunk registration for new types
    template <TriviallyCopyableComponent T>
    static void RegisterComponent() {
        static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable.");
        RegisterChunkDeserializer(ComponentTraits<T>::TypeHash, &ReadChunkDirect<T>);
        RegisterChunkSerializer(ComponentTraits<T>::TypeHash, [](const IPool& pool, std::ostream& stream) -> bool {
            const auto* p = dynamic_cast<const Pool<T>*>(&pool);
            if (!p) return false;
            return WriteComponentChunk<T>(p->set, stream);
        });
    }

private:
    std::shared_ptr<Scene> m_Scene;
};

} // namespace Astral
