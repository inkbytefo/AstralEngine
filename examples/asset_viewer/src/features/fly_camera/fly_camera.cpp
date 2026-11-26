#include "features/fly_camera/fly_camera.hpp"

namespace fly {

void install_fly_camera(astral::FeatureContext& feature) { feature.component<FlyCamera>(); }

} // namespace fly
