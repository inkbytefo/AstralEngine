#pragma once
// Left click shoots a ball from the camera; right click pushes whatever the camera's ray hits.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace shooter {

void install_shooter(astral::FeatureContext& feature);

} // namespace shooter
