#include "Astral/Scene/SceneSerializer.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/Registry.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Astral {

using ChunkDeserializerFn = bool(*)(Registry&, std::istream&, const ComponentChunkHeader&);

template <TriviallyCopyableComponent T>
static bool ReadChunkDirect(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    static_assert(std::is_trivially_copyable_v<T>, "Component T must be trivially copyable for bulk dump.");

    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) {
        std::cerr << "[Astral::SceneSerializer] Error: Element count exceeds safety limit for component: "
                  << ComponentTraits<T>::Name << "\n";
        return false;
    }

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

static bool ReadHierarchyChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;
    const std::streamsize entityBytes = static_cast<std::streamsize>(chunkHeader.entityDataSize);
    if (entityBytes != static_cast<std::streamsize>(elementCount * sizeof(EntityHandle))) return false;

    std::vector<EntityHandle> entities(elementCount);
    if (entityBytes > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), entityBytes);
        if (!stream || stream.gcount() != entityBytes) return false;
    }

    std::vector<HierarchyComponent> data(elementCount);
    uint64_t bytesRead = 0;
    for (HierarchyComponent& hierarchy : data) {
        uint32_t childCount = 0;
        stream.read(reinterpret_cast<char*>(&hierarchy.parent), sizeof(EntityHandle));
        stream.read(reinterpret_cast<char*>(&childCount), sizeof(uint32_t));
        if (!stream) return false;
        bytesRead += sizeof(EntityHandle) + sizeof(uint32_t);

        if (childCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;
        const uint64_t childBytes = static_cast<uint64_t>(childCount) * sizeof(EntityHandle);
        if (bytesRead + childBytes > chunkHeader.componentDataSize) return false;
        hierarchy.children.resize(childCount);
        if (childBytes > 0) {
            stream.read(reinterpret_cast<char*>(hierarchy.children.data()), static_cast<std::streamsize>(childBytes));
            if (!stream || stream.gcount() != static_cast<std::streamsize>(childBytes)) return false;
        }
        bytesRead += childBytes;
    }

    if (bytesRead != chunkHeader.componentDataSize) return false;
    registry.GetView<HierarchyComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static bool ReadTagChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;
    const std::streamsize entityBytes = static_cast<std::streamsize>(chunkHeader.entityDataSize);
    if (entityBytes != static_cast<std::streamsize>(elementCount * sizeof(EntityHandle))) return false;

    std::vector<EntityHandle> entities(elementCount);
    if (entityBytes > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), entityBytes);
        if (!stream || stream.gcount() != entityBytes) return false;
    }

    std::vector<TagComponent> data(elementCount);
    uint64_t bytesRead = 0;
    for (TagComponent& tagComp : data) {
        uint32_t strLen = 0;
        stream.read(reinterpret_cast<char*>(&strLen), sizeof(uint32_t));
        if (!stream) return false;
        bytesRead += sizeof(uint32_t);

        if (strLen > SceneSerializer::MAX_TAG_LENGTH) return false;
        if (bytesRead + strLen > chunkHeader.componentDataSize) return false;

        tagComp.tag.resize(strLen);
        if (strLen > 0) {
            stream.read(tagComp.tag.data(), static_cast<std::streamsize>(strLen));
            if (!stream || stream.gcount() != static_cast<std::streamsize>(strLen)) return false;
        }
        bytesRead += strLen;
    }

    if (bytesRead != chunkHeader.componentDataSize) return false;
    registry.GetView<TagComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static std::unordered_map<uint64_t, ChunkDeserializerFn>& GetDeserializerRegistry() {
    static std::unordered_map<uint64_t, ChunkDeserializerFn> registry = {
        { ComponentTraits<TransformComponent>::TypeHash,  &ReadChunkDirect<TransformComponent> },
        { ComponentTraits<HierarchyComponent>::TypeHash,  &ReadHierarchyChunk },
        { ComponentTraits<TagComponent>::TypeHash,        &ReadTagChunk },
        { ComponentTraits<VelocityComponent>::TypeHash,   &ReadChunkDirect<VelocityComponent> },
        { ComponentTraits<HealthComponent>::TypeHash,     &ReadChunkDirect<HealthComponent> },
        { ComponentTraits<SDFComponent>::TypeHash,        &ReadChunkDirect<SDFComponent> },
        { ComponentTraits<VisibilityComponent>::TypeHash, &ReadChunkDirect<VisibilityComponent> }
    };
    return registry;
}

void SceneSerializer::RegisterChunkDeserializer(uint64_t typeId, ChunkDeserializerFn deserializer) {
    GetDeserializerRegistry()[typeId] = deserializer;
}

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
template bool SceneSerializer::WriteComponentChunk<VisibilityComponent>(const SparseSet<VisibilityComponent>&, std::ostream&);

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

    // 2. Scene Metadata Chunk (Scene Name)
    {
        const std::string& sceneName = scene.GetName();
        const uint32_t nameLen = static_cast<uint32_t>(std::min<size_t>(sceneName.size(), MAX_SCENE_NAME_LENGTH));
        const uint32_t componentDataSize = static_cast<uint32_t>(sizeof(uint32_t) + nameLen);
        const ComponentChunkHeader metaHeader{
            .typeId = SCENE_METADATA_TYPE_ID,
            .version = 1,
            .flags = 0,
            .elementCount = 1,
            .entityDataSize = 0,
            .componentDataSize = componentDataSize
        };
        stream.write(reinterpret_cast<const char*>(&metaHeader), sizeof(metaHeader));
        stream.write(reinterpret_cast<const char*>(&nameLen), sizeof(uint32_t));
        if (nameLen > 0) {
            stream.write(sceneName.data(), static_cast<std::streamsize>(nameLen));
        }
        if (!stream) return false;
    }

    // 3. Active Entity Table Chunk (Preserves all alive entities, including empty nodes)
    {
        const std::vector<EntityHandle> aliveEntities = registry.GetAliveEntities();
        const uint32_t entityCount = static_cast<uint32_t>(aliveEntities.size());
        const uint32_t entityDataSize = static_cast<uint32_t>(entityCount * sizeof(EntityHandle));
        const ComponentChunkHeader entityChunkHeader{
            .typeId = ENTITY_TABLE_TYPE_ID,
            .version = 1,
            .flags = 0,
            .elementCount = entityCount,
            .entityDataSize = entityDataSize,
            .componentDataSize = 0
        };
        stream.write(reinterpret_cast<const char*>(&entityChunkHeader), sizeof(entityChunkHeader));
        if (entityDataSize > 0) {
            stream.write(reinterpret_cast<const char*>(aliveEntities.data()), static_cast<std::streamsize>(entityDataSize));
        }
        if (!stream) return false;
    }

    // 4. Component Chunks (Data-Oriented Dump: zero per-entity iteration)
    for (const auto& [typeIndex, pool] : registry.GetPools()) {
        if (!pool || pool->Size() == 0) continue;
        const uint64_t typeId = pool->GetTypeHash();

        if (typeId == ComponentTraits<HierarchyComponent>::TypeHash) {
            const auto* hierarchyPool = dynamic_cast<const Pool<HierarchyComponent>*>(pool.get());
            if (!hierarchyPool) return false;

            const auto& set = hierarchyPool->set;
            uint64_t componentBytes64 = 0;
            for (const HierarchyComponent& hierarchy : set.Data()) {
                componentBytes64 += sizeof(EntityHandle) + sizeof(uint32_t) +
                                    hierarchy.children.size() * sizeof(EntityHandle);
            }
            if (componentBytes64 > std::numeric_limits<uint32_t>::max()) return false;

            const uint32_t elementCount = static_cast<uint32_t>(set.Size());
            const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
            const uint32_t componentDataSize = static_cast<uint32_t>(componentBytes64);
            const ComponentChunkHeader chunkHeader{
                .typeId = typeId,
                .version = 1,
                .flags = 0,
                .elementCount = elementCount,
                .entityDataSize = entityDataSize,
                .componentDataSize = componentDataSize
            };

            stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
            stream.write(reinterpret_cast<const char*>(set.Entities().data()), entityDataSize);
            for (const HierarchyComponent& hierarchy : set.Data()) {
                const uint32_t childCount = static_cast<uint32_t>(hierarchy.children.size());
                stream.write(reinterpret_cast<const char*>(&hierarchy.parent), sizeof(EntityHandle));
                stream.write(reinterpret_cast<const char*>(&childCount), sizeof(uint32_t));
                if (childCount > 0) {
                    stream.write(
                        reinterpret_cast<const char*>(hierarchy.children.data()),
                        static_cast<std::streamsize>(childCount * sizeof(EntityHandle))
                    );
                }
            }
            if (!stream) return false;
            continue;
        }

        if (typeId == ComponentTraits<TagComponent>::TypeHash) {
            const auto* tagPool = dynamic_cast<const Pool<TagComponent>*>(pool.get());
            if (!tagPool) return false;

            const auto& set = tagPool->set;
            uint64_t componentBytes64 = 0;
            for (const TagComponent& tagComp : set.Data()) {
                const uint32_t strLen = static_cast<uint32_t>(std::min<size_t>(tagComp.tag.size(), MAX_TAG_LENGTH));
                componentBytes64 += sizeof(uint32_t) + strLen;
            }
            if (componentBytes64 > std::numeric_limits<uint32_t>::max()) return false;

            const uint32_t elementCount = static_cast<uint32_t>(set.Size());
            const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
            const uint32_t componentDataSize = static_cast<uint32_t>(componentBytes64);
            const ComponentChunkHeader chunkHeader{
                .typeId = typeId,
                .version = 1,
                .flags = 0,
                .elementCount = elementCount,
                .entityDataSize = entityDataSize,
                .componentDataSize = componentDataSize
            };

            stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
            stream.write(reinterpret_cast<const char*>(set.Entities().data()), entityDataSize);
            for (const TagComponent& tagComp : set.Data()) {
                const uint32_t strLen = static_cast<uint32_t>(std::min<size_t>(tagComp.tag.size(), MAX_TAG_LENGTH));
                stream.write(reinterpret_cast<const char*>(&strLen), sizeof(uint32_t));
                if (strLen > 0) {
                    stream.write(tagComp.tag.data(), static_cast<std::streamsize>(strLen));
                }
            }
            if (!stream) return false;
            continue;
        }

        if (!pool->IsTriviallyCopyable()) {
            // Cannot be bulk dumped according to C++20 trivial copyability mandate
            continue;
        }

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
    // 0. Determine file size and enforce safety budget
    stream.seekg(0, std::ios::end);
    const auto fileSizeBytes = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (fileSizeBytes < static_cast<std::streamoff>(sizeof(SceneFileHeader))) {
        std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading file header in: " << filepath.string() << "\n";
        return false;
    }
    if (static_cast<uint64_t>(fileSizeBytes) > SceneSerializer::MAX_FILE_SIZE_BUDGET) {
        std::cerr << "[Astral::SceneSerializer] Error: File size exceeds safety budget in: " << filepath.string() << "\n";
        return false;
    }

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

    if (fileHeader.activeEntityCount > SceneSerializer::MAX_ELEMENT_COUNT) {
        std::cerr << "[Astral::SceneSerializer] Error: Corrupt activeEntityCount (" << fileHeader.activeEntityCount
                  << ") in: " << filepath.string() << "\n";
        return false;
    }

    stagingScene->GetRegistry().SetNextEntityId(fileHeader.activeEntityCount);
    const auto& deserializers = GetDeserializerRegistry();

    std::vector<EntityHandle> loadedEntityTable;
    bool hasExplicitEntityTable = false;

    // 2. Read Component Chunks until EOF
    while (stream.peek() != std::char_traits<char>::eof()) {
        ComponentChunkHeader chunkHeader{};
        stream.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ComponentChunkHeader));
        if (!stream || stream.gcount() != sizeof(ComponentChunkHeader)) {
            std::cerr << "[Astral::SceneSerializer] Error: Premature EOF reading chunk header in: " << filepath.string() << "\n";
            return false;
        }

        // Bounded budget validation: chunk payload cannot exceed remaining bytes in file
        const uint64_t totalChunkDataBytes = static_cast<uint64_t>(chunkHeader.entityDataSize) +
                                             static_cast<uint64_t>(chunkHeader.componentDataSize);
        const auto currentPos = stream.tellg();
        if (currentPos < 0 || totalChunkDataBytes > static_cast<uint64_t>(fileSizeBytes - currentPos)) {
            std::cerr << "[Astral::SceneSerializer] Error: Corrupt chunk payload size (" << totalChunkDataBytes
                      << " bytes) exceeds remaining file size in: " << filepath.string() << "\n";
            return false;
        }

        // Special handling for SceneMetadata chunk
        if (chunkHeader.typeId == SceneSerializer::SCENE_METADATA_TYPE_ID) {
            uint32_t nameLen = 0;
            stream.read(reinterpret_cast<char*>(&nameLen), sizeof(uint32_t));
            if (!stream || nameLen > SceneSerializer::MAX_SCENE_NAME_LENGTH ||
                sizeof(uint32_t) + nameLen != chunkHeader.componentDataSize) {
                std::cerr << "[Astral::SceneSerializer] Error: Invalid scene name length in metadata chunk in: "
                          << filepath.string() << "\n";
                return false;
            }
            std::string sceneName(nameLen, '\0');
            if (nameLen > 0) {
                stream.read(sceneName.data(), static_cast<std::streamsize>(nameLen));
                if (!stream || stream.gcount() != static_cast<std::streamsize>(nameLen)) {
                    return false;
                }
            }
            stagingScene->SetName(std::move(sceneName));
            continue;
        }

        // Special handling for EntityTable chunk
        if (chunkHeader.typeId == SceneSerializer::ENTITY_TABLE_TYPE_ID) {
            const uint32_t entityCount = chunkHeader.elementCount;
            if (entityCount > SceneSerializer::MAX_ELEMENT_COUNT ||
                chunkHeader.entityDataSize != entityCount * sizeof(EntityHandle)) {
                std::cerr << "[Astral::SceneSerializer] Error: Corrupt entity table chunk in: " << filepath.string() << "\n";
                return false;
            }
            loadedEntityTable.resize(entityCount);
            if (chunkHeader.entityDataSize > 0) {
                stream.read(reinterpret_cast<char*>(loadedEntityTable.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
                if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) {
                    return false;
                }
            }
            hasExplicitEntityTable = true;
            continue;
        }

        auto it = deserializers.find(chunkHeader.typeId);
        if (it == deserializers.end()) {
            // Graceful Unknown Chunk Skipping (Forward Compatibility)
            std::cerr << "[Astral::SceneSerializer] Warning: Skipping unknown component TypeID: 0x"
                      << std::hex << chunkHeader.typeId << std::dec << " ("
                      << totalChunkDataBytes << " bytes) in: "
                      << filepath.string() << "\n";
            const auto skipBytes = static_cast<std::streamoff>(totalChunkDataBytes);
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
    if (hasExplicitEntityTable) {
        stagingScene->GetRegistry().RebuildIdentityFromEntities(loadedEntityTable);
    } else {
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
    }

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

    // Atomic Staging: deserialize into temporary staging scene first, retaining scene's current name as default
    auto stagingScene = std::make_shared<Scene>(scene.GetName());
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
