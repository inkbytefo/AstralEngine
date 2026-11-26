#include "features/showcase/showcase.hpp"

using namespace astral;

namespace showcase {

void install_showcase(FeatureContext& feature) {
    feature.component<Station>();
    feature.component<Spinner>();
    feature.component<Scanner>();
}

} // namespace showcase
