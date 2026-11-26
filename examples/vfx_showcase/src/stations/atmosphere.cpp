// Station 4 (atmosphere): Rayleigh/Mie sky shader, sky IBL, aerial perspective across distant courtyard towers,
// and global froxel fog with self-shadows and DDGI probe lighting.
#include "stations/stations.hpp"

using namespace astral;

namespace vfx_showcase {

void build_atmosphere(StationBuild& build) {
    if (!build.load_courtyard(17.2f, 0.035f, false)) {
        build.ctx.quit();
        return;
    }
    build.create(Transform{.position = {0.0f, 2.0f, -2.0f}},
                 AudioSource{.sound = "sounds/courtyard_wind.wav",
                             .bus = AudioBus::Ambience,
                             .volume = 0.3f,
                             .loop = true,
                             .spatial = false,
                             .playing = true});
    build.camera({0.0f, 3.8f, 11.5f}, {0.0f, 2.2f, -15.0f}, 58.0f);
}

} // namespace vfx_showcase
