#pragma once

#include "Astral/Render/World.hpp"
#include "Astral/Render/Renderer.hpp"

#include <vector>

namespace Astral::Render {

struct InstanceData {
    MeshHandle mesh;
    MaterialHandle material;
    glm::mat4 transform{1.0f};
    glm::mat4 lastUploaded{1.0f}; // previous frame's transform, for motion vectors
    bool hasHistory = false;
    uint32_t objectId = 0;
    bool visible = true;
    bool castsShadows = true;
};

struct SdfShapeData {
    SdfShapeDesc desc;
    uint64_t sequence = 0; // creation order, breaks ties between equal `order`
};

// A world-space box whose SDF content changed and must be re-baked.
struct DirtyBox {
    glm::vec3 min;
    glm::vec3 max;
};

struct World::Impl {
    HandlePool<InstanceTag, InstanceData> instances;
    HandlePool<SdfShapeTag, SdfShapeData> sdfShapes;
    HandlePool<LightTag, LightDesc> lights;
    EnvironmentDesc environment;
    SdfVolumeSettings volume;

    uint64_t nextSequence = 0;
    uint64_t sdfVersion = 1;         // bumped on any SDF change
    uint64_t environmentVersion = 1; // bumped when the sky or the sun changes

    // Shapes sorted in evaluation order. Rebuilt lazily when sdfVersion changes.
    mutable std::vector<SdfShapeDesc> sortedShapes;
    mutable uint64_t sortedVersion = 0;
    const std::vector<SdfShapeDesc>& SortedShapes() const;
};

} // namespace Astral::Render
