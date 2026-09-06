#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Astral/Geometry/SDFSceneSnapshot.hpp"

namespace Astral {

enum class SDFChangeType : uint32_t {
    None                = 0,
    TransformMotion     = 1 << 0,
    ShapeOrCSGChange    = 1 << 1,
    MaterialChange      = 1 << 2,
    VisibilityOrRemoval = 1 << 3,
    VisibilityOrAddition= 1 << 4,
    GlobalChange        = 1 << 5
};

inline SDFChangeType operator|(SDFChangeType a, SDFChangeType b) {
    return static_cast<SDFChangeType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SDFChangeType operator&(SDFChangeType a, SDFChangeType b) {
    return static_cast<SDFChangeType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasFlag(SDFChangeType val, SDFChangeType flag) {
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct ScreenRect {
    glm::vec2 minUV{1.0f};
    glm::vec2 maxUV{0.0f};

    [[nodiscard]] bool IsValid() const noexcept {
        return minUV.x <= maxUV.x && minUV.y <= maxUV.y;
    }

    [[nodiscard]] bool Contains(const glm::vec2& uv) const noexcept {
        if (!IsValid()) return false;
        return uv.x >= minUV.x && uv.x <= maxUV.x &&
               uv.y >= minUV.y && uv.y <= maxUV.y;
    }

    void Expand(const glm::vec2& p) noexcept {
        minUV = glm::min(minUV, p);
        maxUV = glm::max(maxUV, p);
    }

    void Union(const ScreenRect& o) noexcept {
        if (!o.IsValid()) return;
        if (!IsValid()) {
            minUV = o.minUV;
            maxUV = o.maxUV;
            return;
        }
        minUV = glm::min(minUV, o.minUV);
        maxUV = glm::max(maxUV, o.maxUV);
    }

    [[nodiscard]] float Area() const noexcept {
        if (!IsValid()) return 0.0f;
        return (maxUV.x - minUV.x) * (maxUV.y - minUV.y);
    }

    [[nodiscard]] bool Intersects(const ScreenRect& o) const noexcept {
        if (!IsValid() || !o.IsValid()) return false;
        return minUV.x <= o.maxUV.x && maxUV.x >= o.minUV.x &&
               minUV.y <= o.maxUV.y && maxUV.y >= o.minUV.y;
    }
};

struct PrimitiveChange {
    uint32_t surfaceId = 0;
    SDFChangeType changeType = SDFChangeType::None;
    glm::vec3 prevMinWorld{0.0f};
    glm::vec3 prevMaxWorld{0.0f};
    glm::vec3 currMinWorld{0.0f};
    glm::vec3 currMaxWorld{0.0f};
    glm::vec3 combinedMinWorld{0.0f};
    glm::vec3 combinedMaxWorld{0.0f};
    ScreenRect screenRect{};
};

class SDFChangeSet {
public:
    SDFChangeSet() = default;

    /// Compares two snapshots and camera viewProjections to compute changed primitives and screen-space invalidation regions
    [[nodiscard]] static SDFChangeSet Compare(
        const SDFSceneSnapshot& prevSnapshot,
        const SDFSceneSnapshot& currSnapshot,
        const glm::mat4& prevViewProj,
        const glm::mat4& currViewProj
    );

    [[nodiscard]] bool HasChanges() const noexcept {
        return m_IsGlobalChange || !m_Changes.empty();
    }

    [[nodiscard]] bool IsGlobalChange() const noexcept {
        return m_IsGlobalChange;
    }

    [[nodiscard]] const std::vector<PrimitiveChange>& GetChanges() const noexcept {
        return m_Changes;
    }

    [[nodiscard]] const std::vector<ScreenRect>& GetScreenRects() const noexcept {
        return m_ScreenRects;
    }

    /// Returns true if a given screen UV falls within any invalidated region
    [[nodiscard]] bool IsPixelInvalidated(const glm::vec2& uv) const noexcept {
        if (m_IsGlobalChange) return true;
        for (const auto& rect : m_ScreenRects) {
            if (rect.Contains(uv)) return true;
        }
        return false;
    }

    /// Returns confidence factor [0, 1] (0.0 = completely invalidated, 1.0 = unchanged history preserved)
    [[nodiscard]] float GetConfidence(const glm::vec2& uv) const noexcept {
        return IsPixelInvalidated(uv) ? 0.0f : 1.0f;
    }

    /// Packs up to 4 screen-space rects into vec4s (xy: minUV, zw: maxUV) for push constants
    void GetPackedRects(std::array<glm::vec4, 4>& outRects, uint32_t& outCount) const noexcept;

private:
    std::vector<PrimitiveChange> m_Changes;
    std::vector<ScreenRect> m_ScreenRects;
    bool m_IsGlobalChange = false;
};

} // namespace Astral
