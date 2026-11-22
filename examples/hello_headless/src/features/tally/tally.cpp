#include "features/tally/tally.hpp"

namespace tally {

void install_tally(astral::FeatureContext& feature) { feature.component<Tally>(); }

} // namespace tally
