#include "Astral/Scene/SceneSerializer.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/Registry.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace Astral {

using ChunkDeserializerFn = bool(*)(Registry&, std::istream&, const ComponentChunkHeader&);

template <TriviallyCopyableComponent T>
static bool ReadChunkDirect(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable for bulk dump.");

    const uint32_t elementCount = chunkHeader.elementCount;
    std::vector<EntityID> entities(elementCount);
    std::vector<T> data(elementCount);

    if (elementCount > 0) {
        const std::streamsize entityBytes = static_cast<std::streamsize>(chunkHeader.entityDataSize);
        const std::streamsize componentBytes = static_cast<std::streamsize>(chunkHeader.componentDataSize);

        if (entityBytes != static_cast<std::streamsize>(elementCount * sizeof(EntityID)) ||
            componentBytes != static_cast<std::streamsize>(elementCount * sizeof(T))) {
            std::cerr << "[Astral::SceneSerializer] Error: Chunk size mismatch for component: "
                      << ComponentTraits<T>::Name << "\n";
            return false;
        }

        stream.read(reinterpret_cast<char*>(entities.data()), entityBytes);
        if (!stream || stream.gcount() != entityBytes) {
            std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading EntityID blob for component: "
                      << ComponentTraits<T>::Name << "\n";
            return false;
        }

        stream.read(reinterpret_cast<char*>(data.data()), componentBytes);
        if (!stream || stream.gcount() != componentBytes) {
            std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading component data blob for component: "
                      << ComponentTraits<T>::Name << "\n";
            return false;
        }
    }

    auto& view = registry.GetView<T>();
    view.AssignDirect(std::move(entities), std::move(data));
    return true;
}

static std::unordered_map<uint64_t, ChunkDeserializerFn>& GetDeserializerRegistry() {
    static std::unordered_map<uint64_t, ChunkDeserializerFn> registry = {
        { ComponentTraits<TransformComponent>::TypeHash, &ReadChunkDirect<TransformComponent> },
        { ComponentTraits<VelocityComponent>::TypeHash,  &ReadChunkDirect<VelocityComponent> },
        { ComponentTraits<HealthComponent>::TypeHash,    &ReadChunkDirect<HealthComponent> },
        { ComponentTraits<SDFComponent>::TypeHash,       &ReadChunkDirect<SDFComponent> }
    };
    return registry;
}

template <TriviallyCopyableComponent T>
void SceneSerializer::RegisterComponent() {
    static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable.");
    GetDeserializerRegistry()[ComponentTraits<T>::TypeHash] = &ReadChunkDirect<T>;
}

template void SceneSerializer::RegisterComponent<TransformComponent>();
template void SceneSerializer::RegisterComponent<VelocityComponent>();
template void SceneSerializer::RegisterComponent<HealthComponent>();
template void SceneSerializer::RegisterComponent<SDFComponent>();

template <TriviallyCopyableComponent T>
bool SceneSerializer::WriteComponentChunk(const SparseSet<T>& sparseSet, std::ostream& stream) {
    static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable.");
    const uint32_t elementCount = static_cast<uint32_t>(sparseSet.Size());
    if (elementCount == 0) return true;

    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityID));
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

    // Write contiguous EntityID blob
    stream.write(reinterpret_cast<const char*>(sparseSet.Entities().data()), entityDataSize);
    if (!stream) return false;

    // Write contiguous Component blob
    stream.write(reinterpret_cast<const char*>(sparseSet.Data().data()), componentDataSize);
    return stream.good();
}

template bool SceneSerializer::WriteComponentChunk<TransformComponent>(const SparseSet<TransformComponent>&, std::ostream&);
template bool SceneSerializer::WriteComponentChunk<VelocityComponent>(const SparseSet<VelocityComponent>&, std::ostream&);
template bool SceneSerializer::WriteComponentChunk<HealthComponent>(const SparseSet<HealthComponent>&, std::ostream&);
template bool SceneSerializer::WriteComponentChunk<SDFComponent>(const SparseSet<SDFComponent>&, std::ostream&);

bool SceneSerializer::Serialize(const std::shared_ptr<Scene>& scene, const std::filesystem::path& filepath) {
    if (!scene) {
        std::cerr << "[Astral::SceneSerializer] Error: Cannot serialize null scene!\n";
        return false;
    }
    return Serialize(*scene, filepath);
}

bool SceneSerializer::Serialize(const Scene& scene, const std::filesystem::path& filepath) {
    try {
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[Astral::SceneSerializer] Error: Failed to create directories: " << e.what() << "\n";
        return false;
    }

    std::ofstream stream(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        std::cerr << "[Astral::SceneSerializer] Error: Cannot open file for writing: " << filepath.string() << "\n";
        return false;
    }

    const auto& registry = scene.GetRegistry();
    uint32_t activeEntityCount = registry.GetAliveEntityCount();
    if (activeEntityCount == 0) {
        for (const auto& [typeIndex, pool] : registry.GetPools()) {
            activeEntityCount = std::max(activeEntityCount, static_cast<uint32_t>(pool->Size()));
        }
    }

    // 1. File Header (12 bytes)
    SceneFileHeader fileHeader{
        .magic = { 'A', 'S', 'T', 'R' },
        .version = CURRENT_VERSION,
        .activeEntityCount = activeEntityCount
    };

    stream.write(reinterpret_cast<const char*>(&fileHeader), sizeof(SceneFileHeader));
    if (!stream) {
        std::cerr << "[Astral::SceneSerializer] Error: Failed writing file header to: " << filepath.string() << "\n";
        return false;
    }

    // 2. Component Chunks (Data-Oriented Dump: zero per-entity iteration)
    for (const auto& [typeIndex, pool] : registry.GetPools()) {
        if (!pool || pool->Size() == 0) continue;
        if (!pool->IsTriviallyCopyable()) {
            // Cannot be bulk dumped according to C++20 trivial copyability mandate
            continue;
        }

        const uint64_t typeId = pool->GetTypeHash();
        if (typeId == 0) continue;

        const uint32_t elementCount = static_cast<uint32_t>(pool->Size());
        const uint32_t entityDataSize = static_cast<uint32_t>(pool->GetEntityDataSize());
        const uint32_t componentDataSize = static_cast<uint32_t>(elementCount * pool->GetComponentSize());

        const ComponentChunkHeader chunkHeader{
            .typeId = typeId,
            .version = 1,
            .flags = 0,
            .elementCount = elementCount,
            .entityDataSize = entityDataSize,
            .componentDataSize = componentDataSize
        };

        // Write Chunk Header (28 bytes)
        stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ComponentChunkHeader));
        if (!stream) {
            std::cerr << "[Astral::SceneSerializer] Error: Failed writing chunk header for TypeID 0x"
                      << std::hex << typeId << std::dec << "\n";
            return false;
        }

        // Write Raw EntityID Blob: contiguous array of EntityID
        stream.write(reinterpret_cast<const char*>(pool->GetRawEntityData()), entityDataSize);
        if (!stream) {
            std::cerr << "[Astral::SceneSerializer] Error: Failed writing entity blob for TypeID 0x"
                      << std::hex << typeId << std::dec << "\n";
            return false;
        }

        // Write Raw Component Blob: contiguous array of ComponentData
        stream.write(reinterpret_cast<const char*>(pool->GetRawData()), componentDataSize);
        if (!stream) {
            std::cerr << "[Astral::SceneSerializer] Error: Failed writing raw component blob for TypeID 0x"
                      << std::hex << typeId << std::dec << "\n";
            return false;
        }
    }

    stream.flush();
    if (!stream.good()) {
        std::cerr << "[Astral::SceneSerializer] Error: Stream failure while finalizing: " << filepath.string() << "\n";
        return false;
    }

    std::cout << "[Astral::SceneSerializer] Scene successfully serialized to: " << filepath.string()
              << " (" << activeEntityCount << " entities)\n";
    return true;
}

static bool InternalDeserialize(const std::shared_ptr<Scene>& stagingScene, std::istream& stream, const std::filesystem::path& filepath) {
    // 1. Read and validate File Header (12 bytes)
    SceneFileHeader fileHeader{};
    stream.read(reinterpret_cast<char*>(&fileHeader), sizeof(SceneFileHeader));
    if (!stream || stream.gcount() != sizeof(SceneFileHeader)) {
        std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading file header in: " << filepath.string() << "\n";
        return false;
    }

    // Strict Magic Check: Fast-fail
    if (std::memcmp(fileHeader.magic, SceneSerializer::MAGIC, 4) != 0) {
        std::cerr << "[Astral::SceneSerializer] Error: Corrupt or invalid magic signature in: " << filepath.string()
                  << " (got '" << fileHeader.magic[0] << fileHeader.magic[1] << fileHeader.magic[2] << fileHeader.magic[3] << "')\n";
        return false;
    }

    // Strict Version Check: Fast-fail
    if (fileHeader.version != SceneSerializer::CURRENT_VERSION) {
        std::cerr << "[Astral::SceneSerializer] Error: Unsupported version " << fileHeader.version
                  << " (current supported version is " << SceneSerializer::CURRENT_VERSION << ") in: " << filepath.string() << "\n";
        return false;
    }

    stagingScene->GetRegistry().SetNextEntityId(fileHeader.activeEntityCount);
    const auto& deserializers = GetDeserializerRegistry();

    // 2. Read Component Chunks until EOF
    while (stream.peek() != std::char_traits<char>::eof()) {
        ComponentChunkHeader chunkHeader{};
        stream.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ComponentChunkHeader));
        if (!stream || stream.gcount() != sizeof(ComponentChunkHeader)) {
            std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading chunk header in: " << filepath.string() << "\n";
            return false;
        }

        auto it = deserializers.find(chunkHeader.typeId);
        if (it == deserializers.end()) {
            // Graceful Unknown Chunk Skipping (Forward Compatibility)
            std::cerr << "[Astral::SceneSerializer] Warning: Skipping unknown component TypeID: 0x"
                      << std::hex << chunkHeader.typeId << std::dec << " ("
                      << (chunkHeader.entityDataSize + chunkHeader.componentDataSize) << " bytes) in: "
                      << filepath.string() << "\n";
            const auto skipBytes = static_cast<std::streamoff>(chunkHeader.entityDataSize + chunkHeader.componentDataSize);
            stream.seekg(skipBytes, std::ios::cur);
            if (!stream) {
                std::cerr << "[Astral::SceneSerializer] Error: Premature EOF while skipping unknown chunk in: "
                          << filepath.string() << "\n";
                return false;
            }
            continue;
        }

        // Execute bulk chunk load in two direct block reads
        if (!it->second(stagingScene->GetRegistry(), stream, chunkHeader)) {
            std::cerr << "[Astral::SceneSerializer] Error: Failed deserializing chunk body for TypeID: 0x"
                      << std::hex << chunkHeader.typeId << std::dec << "\n";
            return false;
        }
    }

    // 3. Reconstruct Identity Table & FreeList from loaded entity handles
    std::vector<EntityHandle> allEntities;
    for (const auto& [typeIndex, pool] : stagingScene->GetRegistry().GetPools()) {
        if (!pool || pool->Size() == 0) continue;
        const EntityHandle* rawEntities = reinterpret_cast<const EntityHandle*>(pool->GetRawEntityData());
        const size_t count = pool->Size();
        for (size_t i = 0; i < count; ++i) {
            allEntities.push_back(rawEntities[i]);
        }
    }
    stagingScene->GetRegistry().RebuildIdentityFromEntities(allEntities);

    return true;
}

bool SceneSerializer::Deserialize(const std::shared_ptr<Scene>& scene, const std::filesystem::path& filepath) {
    if (!scene) {
        std::cerr << "[Astral::SceneSerializer] Error: Cannot deserialize into null scene!\n";
        return false;
    }
    return Deserialize(*scene, filepath);
}

bool SceneSerializer::Deserialize(Scene& scene, const std::filesystem::path& filepath) {
    std::ifstream stream(filepath, std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        std::cerr << "[Astral::SceneSerializer] Error: Cannot open file for reading: " << filepath.string() << "\n";
        return false;
    }

    // Atomic Staging: deserialize into temporary staging scene first
    auto stagingScene = std::make_shared<Scene>("Staging");
    if (!InternalDeserialize(stagingScene, stream, filepath)) {
        return false;
    }

    // Atomic Commit: Two-Phase Commit swap ensures live scene was never partially mutated
    scene.Swap(*stagingScene);

    std::cout << "[Astral::SceneSerializer] Scene successfully deserialized from: " << filepath.string()
              << " (" << scene.GetRegistry().GetAliveEntityCount() << " entities)\n";
    return true;
}

} // namespace Astral
