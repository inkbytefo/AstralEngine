#include "stations/stations.hpp"

#include <iterator>

using namespace astral;

namespace world_showcase {

namespace {

constexpr StationInfo stations[] = {
    {"world", "4x4 km açık dünya", "Hücre akışı, 2 km kayan orijin, uzak kuleler ve N ile gece lambası katmanı."},
    {"range", "Dağ silsilesi (6x6 arazi)", "Yükseklik ve eğim katmanları (.ashader), yarıçap tabanlı karo ve fizik akışı."},
    {"hill", "Tepe (2x2 arazi LOD)", "Yakın 65x65 / uzak 33x33 dikişli LOD ve tam 65x65 fizik; F ile uzak açı."},
};

std::size_t index(StationId id) noexcept { return static_cast<std::size_t>(id); }

} // namespace

const StationInfo& info(StationId id) { return stations[index(id)]; }

StationId station_from_name(std::string_view name) {
    for (std::size_t i = 0; i < std::size(stations); ++i) {
        if (name == stations[i].name) return static_cast<StationId>(i);
    }
    return StationId::World;
}

StationId resolve_initial_station(std::string_view view_name, bool far_view) {
    if (!view_name.empty()) return station_from_name(view_name);
    return far_view ? StationId::Hill : StationId::World;
}

bool StationBuild::load_scene(std::string_view path) {
    const LoadResult result = ctx.scenes().load_scene(path);
    for (const std::string& e : result.errors) ctx.log.error("world_showcase: {}: {}", path, e);
    owned.insert(owned.end(), result.entities.begin(), result.entities.end());
    return result.errors.empty() && !result.entities.empty();
}

Vec3 hill_camera_eye(bool far_view) noexcept {
    return far_view ? Vec3{64.0f, 108.0f, 190.0f} : Vec3{64.0f, 93.0f, 158.0f};
}

} // namespace world_showcase
