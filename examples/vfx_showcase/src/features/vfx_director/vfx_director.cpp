#include "features/vfx_director/vfx_director.hpp"

namespace vfx_director {

void install_vfx_director(astral::FeatureContext& feature) {
    feature.component<Beacon>();
    feature.component<SmokeDisperser>();
    feature.component<SoundOneShot>();
}

} // namespace vfx_director
