#include "features/pong_scorelights/lights.hpp"

#include <utility>
#include <vector>

using namespace astral;

namespace pong {

namespace {

/// Lights the score pips: a pip glows while its side has more points than its index.
class ScoreLights final : public System {
public:
    void configure(SystemAccess& access) override { access.query<const Match, const ScorePip, PrimitiveMesh>(); }

    void update(Context& ctx) override {
        const Match* m = ctx.singleton<Match>();
        if (!m) return;
        const Match score = *m;
        World& w = ctx.world;
        changes_.clear();
        w.each<const ScorePip, const PrimitiveMesh>([&](Entity e, const ScorePip& pip, const PrimitiveMesh& mesh) {
            const bool lit = (pip.side == Side::Player ? score.player : score.opponent) > pip.index;
            const Vec3 emissive = pip.color * (lit ? 6.0f : 0.03f);
            const Vec3& now = mesh.material.emissive;
            if (now.x != emissive.x || now.y != emissive.y || now.z != emissive.z) changes_.emplace_back(e, emissive);
        });
        for (const auto& [e, emissive] : changes_) {
            w.get<PrimitiveMesh>(e)->material.emissive = emissive; // only real changes
        }
    }

private:
    std::vector<std::pair<Entity, Vec3>> changes_;
};

} // namespace

void install_scorelights(FeatureContext& feature) {
    feature.component<ScorePip>();
    feature.system<ScoreLights>(Phase::PostUpdate);
}

} // namespace pong
