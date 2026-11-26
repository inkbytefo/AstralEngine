#include "features/pong_rules/rules.hpp"

#include "features/pong_rules/types.hpp"

namespace pong {

void install_rules(astral::FeatureContext& feature) {
    feature.component<Court>();
    feature.component<Paddle>();
    feature.component<Ball>();
    feature.component<DepthMarker>();
    feature.component<Match>();
    feature.component<MatchSave>();
    feature.event<Scored>();
    feature.event<Bounced>();
    feature.event<SaveNotice>();
}

} // namespace pong
