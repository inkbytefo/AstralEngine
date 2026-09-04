#pragma once

#include <cstddef>

namespace Astral {

/// Engine statistics and performance panel.
class Statistics {
public:
    void Draw(float gpuTimeMs, float cpuTimeMs, size_t activeEntities);
};

} // namespace Astral
