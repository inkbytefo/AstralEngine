#pragma once

#include <vector>
#include <cstdint>

namespace Astral {

class Registry;
struct SDFEditGPU;

/// ECS Registry icindeki TransformComponent ve SDFComponent bilesenlerini
/// filtreleyip GPU tarafinda dogrudan okunabilir std430 SDFEditGPU dizisine ve Entity ID haritasina donusturur.
void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits, std::vector<uint32_t>& outEntities);

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits);

/// Dogrudan Persistent Mapped Buffer bellegine (memcpy ile) aktarim saglar.
void ExtractAndUploadRenderData(Registry& registry, void* mappedGpuBuffer, uint32_t& outEditCount);

} // namespace Astral
