#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Astral {

enum class SDFShapeEncoding : uint32_t {
    LegacyPackedScale = 0,
    ExplicitShape = 1
};

struct SDFShapeParameters {
    glm::vec4 dimensions{1.0f};
};
// Sphere:   x = radius
// Box:      xyz = half extents
// Torus:    x = major radius (R), y = minor radius (r)
// Capsule:  x = radius, y = segment length (+Y, origin at segment start)
// Cylinder: x = radius, y = half height
// Plane:    x = local Y offset

} // namespace Astral
