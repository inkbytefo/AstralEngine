#pragma once

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <iosfwd>

#include "Astral/Core/Registry.hpp"

namespace Astral {

class Scene;

template <typename T>
concept TriviallyCopyableComponent = std::is_trivially_copyable_v<T>;

#pragma pack(push, 1)
/// Binary layout: File Header (12 bytes)
struct SceneFileHeader {
    char magic[4] = { 'A', 'S', 'T', 'R' }; // Magic Signature ("ASTR")
    uint32_t version = 2;                   // Version ID (v2 for DOD Entity-Component pairing)
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
 * Trivial componentleri dogrudan bulk dump eder; degisken uzunluklu hierarchy verisini ozel chunk ile yazar.
 * Atomic staging deserialization ve forward-compatible graceful chunk skipping uygular.
 */
class SceneSerializer {
public:
    static constexpr uint32_t CURRENT_VERSION = 2;
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

            stream.read(reinterpret_cast<char*>(data.data()), componentBytes);
            if (!stream || stream.gcount() != componentBytes) return false;
        }

        auto& view = registry.GetView<T>();
        view.AssignDirect(std::move(entities), std::move(data));
        return true;
    }

    /// Compile-time checked manual chunk registration for new types
    template <TriviallyCopyableComponent T>
    static void RegisterComponent() {
        static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable.");
        RegisterChunkDeserializer(ComponentTraits<T>::TypeHash, &ReadChunkDirect<T>);
    }

    /// Compile-time bulk chunk writer for a specific component pool
    template <TriviallyCopyableComponent T>
    static bool WriteComponentChunk(const SparseSet<T>& sparseSet, std::ostream& stream);

private:
    std::shared_ptr<Scene> m_Scene;
};

} // namespace Astral
