#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include "Astral/Core/Registry.hpp"
#include "Astral/Renderer/RenderCamera.hpp"

namespace Astral {

class Registry;
struct SDFEditGPU;

/// Null clears the selection. Invalid selections leave the previous camera intact.
[[nodiscard]] bool SetActiveCamera(Registry& registry, EntityHandle camera);
/// Uses cached world transforms when available; invalid/missing cameras return nullopt.
/// Multiple primary flags (e.g. imported data) resolve to the lowest live handle.
[[nodiscard]] std::optional<RenderCamera> ExtractActiveCamera(Registry& registry, float aspect);

/// ECS Registry icindeki TransformComponent ve SDFComponent bilesenlerini
/// filtreleyip GPU tarafinda dogrudan okunabilir std430 SDFEditGPU dizisine ve Entity ID haritasina donusturur.
void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits, std::vector<EntityHandle>& outEntities);

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits);

/// Dogrudan Persistent Mapped Buffer bellegine (memcpy ile) aktarim saglar.
void ExtractAndUploadRenderData(Registry& registry, void* mappedGpuBuffer, uint32_t& outEditCount);

} // namespace Astral
