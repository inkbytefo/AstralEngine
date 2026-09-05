#pragma once
#include "Astral/Scene/SceneSerializer.hpp"
#include <array>
#include <bit>

namespace Astral::CameraSerialization {
inline bool Write(const IPool& pool, std::ostream& stream) {
    const auto* cameraPool = dynamic_cast<const Pool<CameraComponent>*>(&pool);
    if (!cameraPool) return false;
    const auto& set = cameraPool->set;
    if (set.Size() > SceneSerializer::MAX_ELEMENT_COUNT) return false;
    const auto count = static_cast<uint32_t>(set.Size());
    const ComponentChunkHeader header{ComponentTraits<CameraComponent>::TypeHash, 1, 0,
        count, count * static_cast<uint32_t>(sizeof(EntityHandle)), count * 16u};
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (const auto handle : set.Entities()) {
        const auto encoded = Endian::ToLittle(handle);
        stream.write(reinterpret_cast<const char*>(&encoded), sizeof(encoded));
    }
    for (const auto& camera : set.Data()) {
        const std::array<uint32_t, 4> fields{
            Endian::ToLittle(std::bit_cast<uint32_t>(camera.verticalFovRadians)),
            Endian::ToLittle(std::bit_cast<uint32_t>(camera.nearClip)),
            Endian::ToLittle(std::bit_cast<uint32_t>(camera.farClip)),
            Endian::ToLittle(camera.primary)};
        stream.write(reinterpret_cast<const char*>(fields.data()), sizeof(fields));
    }
    return stream.good();
}

inline bool Read(Registry& registry, std::istream& stream, const ComponentChunkHeader& header) {
    const auto count = header.elementCount;
    if (header.version != 1 || count > SceneSerializer::MAX_ELEMENT_COUNT ||
        header.entityDataSize != uint64_t{count} * sizeof(EntityHandle) ||
        header.componentDataSize != uint64_t{count} * 16) return false;
    std::vector<EntityHandle> entities(count);
    std::vector<CameraComponent> cameras(count);
    for (auto& handle : entities) {
        stream.read(reinterpret_cast<char*>(&handle), sizeof(handle));
        handle = Endian::FromLittle(handle);
    }
    for (auto& camera : cameras) {
        std::array<uint32_t, 4> fields{};
        stream.read(reinterpret_cast<char*>(fields.data()), sizeof(fields));
        if (!stream) return false;
        camera.verticalFovRadians = std::bit_cast<float>(Endian::FromLittle(fields[0]));
        camera.nearClip = std::bit_cast<float>(Endian::FromLittle(fields[1]));
        camera.farClip = std::bit_cast<float>(Endian::FromLittle(fields[2]));
        camera.primary = Endian::FromLittle(fields[3]);
        if (camera.primary > 1) return false;
    }
    if (!stream) return false;
    registry.GetView<CameraComponent>().AssignDirect(std::move(entities), std::move(cameras));
    return true;
}
}
