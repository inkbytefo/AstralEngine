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

    /// Compile-time checked manual chunk registration for new types
    template <TriviallyCopyableComponent T>
    static void RegisterComponent();

    /// Compile-time bulk chunk writer for a specific component pool
    template <TriviallyCopyableComponent T>
    static bool WriteComponentChunk(const SparseSet<T>& sparseSet, std::ostream& stream);

private:
    std::shared_ptr<Scene> m_Scene;
};

} // namespace Astral
