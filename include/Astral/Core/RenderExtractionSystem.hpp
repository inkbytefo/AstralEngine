#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include "Astral/Core/Registry.hpp"
#include "Astral/Renderer/RenderCamera.hpp"

namespace Astral {

class Registry;
struct LegacySDFEdit;

/// Null clears the selection. Invalid selections leave the previous camera intact.
[[nodiscard]] bool SetActiveCamera(Registry& registry, EntityHandle camera);
/// Uses cached world transforms when available; invalid/missing cameras return nullopt.
/// Multiple primary flags (e.g. imported data) resolve to the lowest live handle.
[[nodiscard]] std::optional<RenderCamera> ExtractActiveCamera(Registry& registry, float aspect);

struct SDFPrimitiveRecord;

/// ECS Registry icindeki TransformComponent ve SDFComponent bilesenlerini
/// filtreleyip GPU tarafinda dogrudan okunabilir std430 SDFPrimitiveRecord dizisine ve Entity ID haritasina donusturur.
void ExtractRenderData(Registry& registry, std::vector<SDFPrimitiveRecord>& outEdits, std::vector<EntityHandle>& outEntities);
void ExtractRenderData(Registry& registry, std::vector<SDFPrimitiveRecord>& outEdits);

/// Geriye donuk uyumluluk arayuzleri
void ExtractRenderData(Registry& registry, std::vector<LegacySDFEdit>& outEdits, std::vector<EntityHandle>& outEntities);
void ExtractRenderData(Registry& registry, std::vector<LegacySDFEdit>& outEdits);

/// Dogrudan Persistent Mapped Buffer bellegine (memcpy ile) SDFPrimitiveRecord aktarimi saglar.
void ExtractAndUploadRenderData(Registry& registry, void* mappedGpuBuffer, uint32_t& outEditCount);

} // namespace Astral
