#include "features/spin/spin.hpp"

namespace blank {

void install_spin(astral::FeatureContext& feature) {
    feature.component<Spin>(); // must be listed in feature.json: the manifest is the feature's API
}

} // namespace blank
