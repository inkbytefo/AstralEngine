#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"

namespace Astral {

/// Inspector panel for editing the components of the selected entity.
/// Shows Transform, SDF Geometry & Material, and Physics sections.
class Inspector {
public:
    void Draw(Scene& scene, Entity& selectedEntity);
};

} // namespace Astral
