#pragma once

#include <cstddef>
#include <array>

namespace Astral {

/// Engine statistics and performance panel.
class Statistics {
public:
    void Update(float gpuTimeMs, float cpuTimeMs);
    void Draw(float gpuTimeMs, float cpuTimeMs, size_t activeEntities);
private:
    std::array<float, 120> m_FpsHistory{};
    int m_HistoryOffset = 0;
};

} // namespace Astral
