#pragma once
#include <astral/app/feature.hpp>

namespace pong {
/// Registers the match's components and events; the rules themselves are the scripts in feature.json.
void install_rules(astral::FeatureContext& feature);
} // namespace pong
