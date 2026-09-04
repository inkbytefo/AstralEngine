#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include <array>

namespace Astral {

/// Inspector panel for editing the components of the selected entity.
/// Shows Transform, SDF Geometry & Material, and Physics sections.
class Inspector {
public:
    void Draw(Scene& scene, Entity& selectedEntity);

private:
    EntityHandle m_NameEntity = NullEntityHandle;
    std::array<char, 128> m_NameBuffer{};
};

} // namespace Astral
