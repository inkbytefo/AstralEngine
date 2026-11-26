#include "features/orbits/orbits.hpp"

namespace orbits {

void install_orbits(astral::FeatureContext& feature) {
    feature.component<Orbit>();
    feature.component<Tumble>();
}

} // namespace orbits
