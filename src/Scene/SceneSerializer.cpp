#include "Astral/Scene/SceneSerializer.hpp"
#include "CameraSerialization.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/Registry.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Astral {

// ============================================================================
// Component Chunk Writers (v3 explicit field layouts & Little-Endian byte order)
// ============================================================================

static bool WriteTransformChunk(const IPool& pool, std::ostream& stream) {
    const auto* transformPool = dynamic_cast<const Pool<TransformComponent>*>(&pool);
    if (!transformPool) return false;

    const auto& set = transformPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    constexpr uint32_t componentBytesPerElem = 40; // 10 x float (pos[3], quat[4], scale[3])
    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = elementCount * componentBytesPerElem;

    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<TransformComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const TransformComponent& t : set.Data()) {
        float f[10] = {
            Endian::ToLittle(t.position.x),
            Endian::ToLittle(t.position.y),
            Endian::ToLittle(t.position.z),
            Endian::ToLittle(t.rotation.x),
            Endian::ToLittle(t.rotation.y),
            Endian::ToLittle(t.rotation.z),
            Endian::ToLittle(t.rotation.w),
            Endian::ToLittle(t.scale.x),
            Endian::ToLittle(t.scale.y),
            Endian::ToLittle(t.scale.z)
        };
        stream.write(reinterpret_cast<const char*>(f), sizeof(f));
    }
    return stream.good();
}

static bool WriteVelocityChunk(const IPool& pool, std::ostream& stream) {
    const auto* velPool = dynamic_cast<const Pool<VelocityComponent>*>(&pool);
    if (!velPool) return false;

    const auto& set = velPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    constexpr uint32_t componentBytesPerElem = 24; // 6 x float (lin[3], ang[3])
    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = elementCount * componentBytesPerElem;

    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<VelocityComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const VelocityComponent& v : set.Data()) {
        float f[6] = {
            Endian::ToLittle(v.linear.x),
            Endian::ToLittle(v.linear.y),
            Endian::ToLittle(v.linear.z),
            Endian::ToLittle(v.angular.x),
            Endian::ToLittle(v.angular.y),
            Endian::ToLittle(v.angular.z)
        };
        stream.write(reinterpret_cast<const char*>(f), sizeof(f));
    }
    return stream.good();
}

static bool WriteHealthChunk(const IPool& pool, std::ostream& stream) {
    const auto* healthPool = dynamic_cast<const Pool<HealthComponent>*>(&pool);
    if (!healthPool) return false;

    const auto& set = healthPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    constexpr uint32_t componentBytesPerElem = 4; // int32_t
    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = elementCount * componentBytesPerElem;

    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<HealthComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const HealthComponent& h : set.Data()) {
        int32_t hpLE = Endian::ToLittle(static_cast<int32_t>(h.hp));
        stream.write(reinterpret_cast<const char*>(&hpLE), sizeof(int32_t));
    }
    return stream.good();
}

static bool WriteVisibilityChunk(const IPool& pool, std::ostream& stream) {
    const auto* visPool = dynamic_cast<const Pool<VisibilityComponent>*>(&pool);
    if (!visPool) return false;

    const auto& set = visPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    constexpr uint32_t componentBytesPerElem = 1; // uint8_t
    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = elementCount * componentBytesPerElem;

    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<VisibilityComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const VisibilityComponent& v : set.Data()) {
        uint8_t b = v.isVisible ? 1u : 0u;
        stream.write(reinterpret_cast<const char*>(&b), sizeof(uint8_t));
    }
    return stream.good();
}

static bool WriteSDFChunk(const IPool& pool, std::ostream& stream) {
    const auto* sdfPool = dynamic_cast<const Pool<SDFComponent>*>(&pool);
    if (!sdfPool) return false;

    const auto& set = sdfPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    constexpr uint32_t componentBytesPerElem = 40;
    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = elementCount * componentBytesPerElem;

    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<SDFComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const SDFComponent& s : set.Data()) {
        uint32_t primLE    = Endian::ToLittle(s.primitiveType);
        uint32_t opLE      = Endian::ToLittle(s.operation);
        float blendLE      = Endian::ToLittle(s.blendFactor);
        uint32_t dynamicLE = Endian::ToLittle(s.isDynamic);
        float albXLE       = Endian::ToLittle(s.albedo.x);
        float albYLE       = Endian::ToLittle(s.albedo.y);
        float albZLE       = Endian::ToLittle(s.albedo.z);
        float roughLE      = Endian::ToLittle(s.roughness);
        float metalLE      = Endian::ToLittle(s.metallic);
        uint32_t visLE     = Endian::ToLittle(s.isVisible);

        stream.write(reinterpret_cast<const char*>(&primLE), 4);
        stream.write(reinterpret_cast<const char*>(&opLE), 4);
        stream.write(reinterpret_cast<const char*>(&blendLE), 4);
        stream.write(reinterpret_cast<const char*>(&dynamicLE), 4);
        stream.write(reinterpret_cast<const char*>(&albXLE), 4);
        stream.write(reinterpret_cast<const char*>(&albYLE), 4);
        stream.write(reinterpret_cast<const char*>(&albZLE), 4);
        stream.write(reinterpret_cast<const char*>(&roughLE), 4);
        stream.write(reinterpret_cast<const char*>(&metalLE), 4);
        stream.write(reinterpret_cast<const char*>(&visLE), 4);
    }
    return stream.good();
}

static bool WriteHierarchyChunk(const IPool& pool, std::ostream& stream) {
    const auto* hierarchyPool = dynamic_cast<const Pool<HierarchyComponent>*>(&pool);
    if (!hierarchyPool) return false;

    const auto& set = hierarchyPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    uint64_t componentBytes64 = 0;
    for (const HierarchyComponent& hierarchy : set.Data()) {
        componentBytes64 += sizeof(EntityHandle) + sizeof(uint32_t) +
                            hierarchy.children.size() * sizeof(EntityHandle);
    }
    if (componentBytes64 > std::numeric_limits<uint32_t>::max()) return false;

    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = static_cast<uint32_t>(componentBytes64);
    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<HierarchyComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const HierarchyComponent& hierarchy : set.Data()) {
        const uint32_t childCount = static_cast<uint32_t>(hierarchy.children.size());
        EntityHandle parentLE = Endian::ToLittle(hierarchy.parent);
        uint32_t childCountLE = Endian::ToLittle(childCount);
        stream.write(reinterpret_cast<const char*>(&parentLE), sizeof(EntityHandle));
        stream.write(reinterpret_cast<const char*>(&childCountLE), sizeof(uint32_t));
        for (const EntityHandle& child : hierarchy.children) {
            EntityHandle childLE = Endian::ToLittle(child);
            stream.write(reinterpret_cast<const char*>(&childLE), sizeof(EntityHandle));
        }
    }
    return stream.good();
}

static bool WriteTagChunk(const IPool& pool, std::ostream& stream) {
    const auto* tagPool = dynamic_cast<const Pool<TagComponent>*>(&pool);
    if (!tagPool) return false;

    const auto& set = tagPool->set;
    const uint32_t elementCount = static_cast<uint32_t>(set.Size());
    if (elementCount == 0) return true;

    uint64_t componentBytes64 = 0;
    for (const TagComponent& tagComp : set.Data()) {
        const uint32_t strLen = static_cast<uint32_t>(std::min<size_t>(tagComp.tag.size(), SceneSerializer::MAX_TAG_LENGTH));
        componentBytes64 += sizeof(uint32_t) + strLen;
    }
    if (componentBytes64 > std::numeric_limits<uint32_t>::max()) return false;

    const uint32_t entityDataSize = static_cast<uint32_t>(elementCount * sizeof(EntityHandle));
    const uint32_t componentDataSize = static_cast<uint32_t>(componentBytes64);
    const ComponentChunkHeader chunkHeader{
        .typeId = ComponentTraits<TagComponent>::TypeHash,
        .version = 1,
        .flags = 0,
        .elementCount = elementCount,
        .entityDataSize = entityDataSize,
        .componentDataSize = componentDataSize
    };

    stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(chunkHeader));
    for (const EntityHandle& h : set.Entities()) {
        EntityHandle hLE = Endian::ToLittle(h);
        stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
    }

    for (const TagComponent& tagComp : set.Data()) {
        const uint32_t strLen = static_cast<uint32_t>(std::min<size_t>(tagComp.tag.size(), SceneSerializer::MAX_TAG_LENGTH));
        uint32_t strLenLE = Endian::ToLittle(strLen);
        stream.write(reinterpret_cast<const char*>(&strLenLE), sizeof(uint32_t));
        if (strLen > 0) {
            stream.write(tagComp.tag.data(), static_cast<std::streamsize>(strLen));
        }
    }
    return stream.good();
}

// ============================================================================
// Component Chunk Readers (v3 explicit field decoding & v2 backwards-compatibility)
// ============================================================================

static bool ReadTransformChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;

    constexpr uint32_t componentBytesPerElem = 40;
    if (chunkHeader.entityDataSize != elementCount * sizeof(EntityHandle) ||
        chunkHeader.componentDataSize != elementCount * componentBytesPerElem) {
        return false;
    }

    std::vector<EntityHandle> entities(elementCount);
    std::vector<TransformComponent> data(elementCount);

    if (elementCount > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
        if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) return false;
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }

        for (uint32_t i = 0; i < elementCount; ++i) {
            float f[10];
            stream.read(reinterpret_cast<char*>(f), sizeof(f));
            if (!stream || stream.gcount() != sizeof(f)) return false;

            data[i].position.x = Endian::FromLittle(f[0]);
            data[i].position.y = Endian::FromLittle(f[1]);
            data[i].position.z = Endian::FromLittle(f[2]);
            data[i].rotation.x = Endian::FromLittle(f[3]);
            data[i].rotation.y = Endian::FromLittle(f[4]);
            data[i].rotation.z = Endian::FromLittle(f[5]);
            data[i].rotation.w = Endian::FromLittle(f[6]);
            data[i].scale.x    = Endian::FromLittle(f[7]);
            data[i].scale.y    = Endian::FromLittle(f[8]);
            data[i].scale.z    = Endian::FromLittle(f[9]);
        }
    }

    registry.GetView<TransformComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static bool ReadVelocityChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;

    constexpr uint32_t componentBytesPerElem = 24;
    if (chunkHeader.entityDataSize != elementCount * sizeof(EntityHandle) ||
        chunkHeader.componentDataSize != elementCount * componentBytesPerElem) {
        return false;
    }

    std::vector<EntityHandle> entities(elementCount);
    std::vector<VelocityComponent> data(elementCount);

    if (elementCount > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
        if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) return false;
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }

        for (uint32_t i = 0; i < elementCount; ++i) {
            float f[6];
            stream.read(reinterpret_cast<char*>(f), sizeof(f));
            if (!stream || stream.gcount() != sizeof(f)) return false;

            data[i].linear.x  = Endian::FromLittle(f[0]);
            data[i].linear.y  = Endian::FromLittle(f[1]);
            data[i].linear.z  = Endian::FromLittle(f[2]);
            data[i].angular.x = Endian::FromLittle(f[3]);
            data[i].angular.y = Endian::FromLittle(f[4]);
            data[i].angular.z = Endian::FromLittle(f[5]);
        }
    }

    registry.GetView<VelocityComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static bool ReadHealthChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;

    constexpr uint32_t componentBytesPerElem = 4;
    if (chunkHeader.entityDataSize != elementCount * sizeof(EntityHandle) ||
        chunkHeader.componentDataSize != elementCount * componentBytesPerElem) {
        return false;
    }

    std::vector<EntityHandle> entities(elementCount);
    std::vector<HealthComponent> data(elementCount);

    if (elementCount > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
        if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) return false;
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }

        for (uint32_t i = 0; i < elementCount; ++i) {
            int32_t hpVal = 0;
            stream.read(reinterpret_cast<char*>(&hpVal), sizeof(int32_t));
            if (!stream || stream.gcount() != sizeof(int32_t)) return false;
            data[i].hp = Endian::FromLittle(hpVal);
        }
    }

    registry.GetView<HealthComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static bool ReadVisibilityChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;

    constexpr uint32_t componentBytesPerElem = 1;
    if (chunkHeader.entityDataSize != elementCount * sizeof(EntityHandle) ||
        chunkHeader.componentDataSize != elementCount * componentBytesPerElem) {
        return false;
    }

    std::vector<EntityHandle> entities(elementCount);
    std::vector<VisibilityComponent> data(elementCount);

    if (elementCount > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
        if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) return false;
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }

        for (uint32_t i = 0; i < elementCount; ++i) {
            uint8_t byte = 0;
            stream.read(reinterpret_cast<char*>(&byte), sizeof(uint8_t));
            if (!stream || stream.gcount() != sizeof(uint8_t)) return false;
            data[i].isVisible = (byte != 0);
        }
    }

    registry.GetView<VisibilityComponent>().AssignDirect(std::move(entities), std::move(data));
    return true;
}

static bool ReadSDFChunk(Registry& registry, std::istream& stream, const ComponentChunkHeader& chunkHeader) {
    const uint32_t elementCount = chunkHeader.elementCount;
    if (elementCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;

    constexpr uint32_t componentBytesPerElem = 40;
    if (chunkHeader.entityDataSize != elementCount * sizeof(EntityHandle) ||
        chunkHeader.componentDataSize != elementCount * componentBytesPerElem) {
        return false;
    }

    std::vector<EntityHandle> entities(elementCount);
    std::vector<SDFComponent> data(elementCount);

    if (elementCount > 0) {
        stream.read(reinterpret_cast<char*>(entities.data()), static_cast<std::streamsize>(chunkHeader.entityDataSize));
        if (!stream || stream.gcount() != static_cast<std::streamsize>(chunkHeader.entityDataSize)) return false;
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }

        for (uint32_t i = 0; i < elementCount; ++i) {
            uint32_t primLE = 0, opLE = 0, dynamicLE = 0, visLE = 0;
            float blendLE = 0.0f, albXLE = 0.0f, albYLE = 0.0f, albZLE = 0.0f, roughLE = 0.0f, metalLE = 0.0f;

            stream.read(reinterpret_cast<char*>(&primLE), 4);
            stream.read(reinterpret_cast<char*>(&opLE), 4);
            stream.read(reinterpret_cast<char*>(&blendLE), 4);
            stream.read(reinterpret_cast<char*>(&dynamicLE), 4);
            stream.read(reinterpret_cast<char*>(&albXLE), 4);
            stream.read(reinterpret_cast<char*>(&albYLE), 4);
            stream.read(reinterpret_cast<char*>(&albZLE), 4);
            stream.read(reinterpret_cast<char*>(&roughLE), 4);
            stream.read(reinterpret_cast<char*>(&metalLE), 4);
            stream.read(reinterpret_cast<char*>(&visLE), 4);
            if (!stream) return false;

            data[i].primitiveType = Endian::FromLittle(primLE);
            data[i].operation     = Endian::FromLittle(opLE);
            data[i].blendFactor   = Endian::FromLittle(blendLE);
            data[i].isDynamic     = Endian::FromLittle(dynamicLE);
            data[i].albedo.x      = Endian::FromLittle(albXLE);
            data[i].albedo.y      = Endian::FromLittle(albYLE);
            data[i].albedo.z      = Endian::FromLittle(albZLE);
            data[i].roughness     = Endian::FromLittle(roughLE);
            data[i].metallic      = Endian::FromLittle(metalLE);
            data[i].isVisible     = Endian::FromLittle(visLE);
        }
    }

    registry.GetView<SDFComponent>().AssignDirect(std::move(entities), std::move(data));
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
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }
    }

    std::vector<HierarchyComponent> data(elementCount);
    uint64_t bytesRead = 0;
    for (HierarchyComponent& hierarchy : data) {
        uint32_t childCount = 0;
        EntityHandle parent{};
        stream.read(reinterpret_cast<char*>(&parent), sizeof(EntityHandle));
        stream.read(reinterpret_cast<char*>(&childCount), sizeof(uint32_t));
        if (!stream) return false;
        bytesRead += sizeof(EntityHandle) + sizeof(uint32_t);

        hierarchy.parent = Endian::FromLittle(parent);
        childCount = Endian::FromLittle(childCount);

        if (childCount > SceneSerializer::MAX_ELEMENT_COUNT) return false;
        const uint64_t childBytes = static_cast<uint64_t>(childCount) * sizeof(EntityHandle);
        if (bytesRead + childBytes > chunkHeader.componentDataSize) return false;
        hierarchy.children.resize(childCount);
        if (childBytes > 0) {
            stream.read(reinterpret_cast<char*>(hierarchy.children.data()), static_cast<std::streamsize>(childBytes));
            if (!stream || stream.gcount() != static_cast<std::streamsize>(childBytes)) return false;
            for (EntityHandle& child : hierarchy.children) {
                child = Endian::FromLittle(child);
            }
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
        for (EntityHandle& h : entities) {
            h = Endian::FromLittle(h);
        }
    }

    std::vector<TagComponent> data(elementCount);
    uint64_t bytesRead = 0;
    for (TagComponent& tagComp : data) {
        uint32_t strLen = 0;
        stream.read(reinterpret_cast<char*>(&strLen), sizeof(uint32_t));
        if (!stream) return false;
        bytesRead += sizeof(uint32_t);

        strLen = Endian::FromLittle(strLen);
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

// ============================================================================
// Serialization Registries
// ============================================================================

static std::unordered_map<uint64_t, SceneSerializer::ChunkSerializerFn>& GetSerializerRegistry() {
    static std::unordered_map<uint64_t, SceneSerializer::ChunkSerializerFn> registry = {
        { ComponentTraits<TransformComponent>::TypeHash,  &WriteTransformChunk },
        { ComponentTraits<CameraComponent>::TypeHash,     &CameraSerialization::Write },
        { ComponentTraits<VelocityComponent>::TypeHash,   &WriteVelocityChunk },
        { ComponentTraits<HealthComponent>::TypeHash,     &WriteHealthChunk },
        { ComponentTraits<VisibilityComponent>::TypeHash, &WriteVisibilityChunk },
        { ComponentTraits<SDFComponent>::TypeHash,        &WriteSDFChunk },
        { ComponentTraits<HierarchyComponent>::TypeHash,  &WriteHierarchyChunk },
        { ComponentTraits<TagComponent>::TypeHash,        &WriteTagChunk }
    };
    return registry;
}

static std::unordered_map<uint64_t, SceneSerializer::ChunkDeserializerFn>& GetDeserializerRegistry() {
    static std::unordered_map<uint64_t, SceneSerializer::ChunkDeserializerFn> registry = {
        { ComponentTraits<TransformComponent>::TypeHash,  &ReadTransformChunk },
        { ComponentTraits<CameraComponent>::TypeHash,     &CameraSerialization::Read },
        { ComponentTraits<VelocityComponent>::TypeHash,   &ReadVelocityChunk },
        { ComponentTraits<HealthComponent>::TypeHash,     &ReadHealthChunk },
        { ComponentTraits<VisibilityComponent>::TypeHash, &ReadVisibilityChunk },
        { ComponentTraits<SDFComponent>::TypeHash,        &ReadSDFChunk },
        { ComponentTraits<HierarchyComponent>::TypeHash,  &ReadHierarchyChunk },
        { ComponentTraits<TagComponent>::TypeHash,        &ReadTagChunk }
    };
    return registry;
}

void SceneSerializer::RegisterChunkSerializer(uint64_t typeId, ChunkSerializerFn serializer) {
    GetSerializerRegistry()[typeId] = serializer;
}

void SceneSerializer::RegisterChunkDeserializer(uint64_t typeId, ChunkDeserializerFn deserializer) {
    GetDeserializerRegistry()[typeId] = deserializer;
}

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

    // Atomic Save Pattern:
    // Write full payload to temporary file first. If any write or validation fails,
    // the temp file is deleted and the target filepath is completely untouched.
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path tempPath = filepath;
    tempPath += ".tmp." + std::to_string(timestamp);

    auto cleanupTemp = [&tempPath]() {
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
    };

    std::ofstream stream(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        std::cerr << "[Astral::SceneSerializer] Error: Cannot open temp file for writing: " << tempPath.string() << "\n";
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
        std::cerr << "[Astral::SceneSerializer] Error: Failed writing file header to: " << tempPath.string() << "\n";
        stream.close();
        cleanupTemp();
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
        const uint32_t nameLenLE = Endian::ToLittle(nameLen);
        stream.write(reinterpret_cast<const char*>(&nameLenLE), sizeof(uint32_t));
        if (nameLen > 0) {
            stream.write(sceneName.data(), static_cast<std::streamsize>(nameLen));
        }
        if (!stream) {
            stream.close();
            cleanupTemp();
            return false;
        }
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
        for (const EntityHandle& h : aliveEntities) {
            EntityHandle hLE = Endian::ToLittle(h);
            stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
        }
        if (!stream) {
            stream.close();
            cleanupTemp();
            return false;
        }
    }

    // 4. Component Chunks (Data-Oriented Chunk Writers via Dispatcher)
    const auto& serializers = GetSerializerRegistry();
    for (const auto& [typeIndex, pool] : registry.GetPools()) {
        if (!pool || pool->Size() == 0) continue;
        const uint64_t typeId = pool->GetTypeHash();
        if (typeId == 0) continue;

        auto it = serializers.find(typeId);
        if (it != serializers.end()) {
            if (!it->second(*pool, stream)) {
                std::cerr << "[Astral::SceneSerializer] Error: Failed serializing chunk for TypeID 0x"
                          << std::hex << typeId << std::dec << "\n";
                stream.close();
                cleanupTemp();
                return false;
            }
            continue;
        }

        if (!pool->IsTriviallyCopyable()) {
            continue;
        }

        // Generic fallback for unregistered trivially copyable components
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

        stream.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ComponentChunkHeader));
        if (!stream) { stream.close(); cleanupTemp(); return false; }

        const EntityHandle* rawEntities = reinterpret_cast<const EntityHandle*>(pool->GetRawEntityData());
        for (uint32_t i = 0; i < elementCount; ++i) {
            EntityHandle hLE = Endian::ToLittle(rawEntities[i]);
            stream.write(reinterpret_cast<const char*>(&hLE), sizeof(EntityHandle));
        }
        if (!stream) { stream.close(); cleanupTemp(); return false; }

        stream.write(reinterpret_cast<const char*>(pool->GetRawData()), componentDataSize);
        if (!stream) { stream.close(); cleanupTemp(); return false; }
    }

    stream.flush();
    if (!stream.good()) {
        std::cerr << "[Astral::SceneSerializer] Error: Stream failure while finalizing: " << tempPath.string() << "\n";
        stream.close();
        cleanupTemp();
        return false;
    }

    stream.close();

    // Atomic replace: safely move temp file to destination
    std::error_code ec;
    std::filesystem::rename(tempPath, filepath, ec);
    if (ec) {
        std::error_code copyEc;
        if (!std::filesystem::copy_file(tempPath, filepath, std::filesystem::copy_options::overwrite_existing, copyEc)) {
            std::cerr << "[Astral::SceneSerializer] Error: Failed replacing destination file: " << copyEc.message() << "\n";
            cleanupTemp();
            return false;
        }
        cleanupTemp();
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

    // Strict Version Range Check: Accepts supported versions [MIN_SUPPORTED_VERSION .. CURRENT_VERSION]
    if (fileHeader.version < SceneSerializer::MIN_SUPPORTED_VERSION ||
        fileHeader.version > SceneSerializer::CURRENT_VERSION) {
        std::cerr << "[Astral::SceneSerializer] Error: Unsupported version " << fileHeader.version
                  << " (supported range: " << SceneSerializer::MIN_SUPPORTED_VERSION
                  << " - " << SceneSerializer::CURRENT_VERSION << ") in: " << filepath.string() << "\n";
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
            nameLen = Endian::FromLittle(nameLen);
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
                for (EntityHandle& h : loadedEntityTable) {
                    h = Endian::FromLittle(h);
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

        // Execute bulk chunk load via registered deserializer
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
