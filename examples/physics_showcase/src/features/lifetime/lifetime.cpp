#include "features/lifetime/lifetime.hpp"

namespace lifetime {

void install_lifetime(astral::FeatureContext& feature) { feature.component<Lifetime>(); }

} // namespace lifetime
