#include "features/lab/lab.hpp"

using namespace astral;

namespace lab {

void install_lab(FeatureContext& feature) {
    feature.component<MotorOscillator>();
    feature.component<LabStats>();
}

} // namespace lab
