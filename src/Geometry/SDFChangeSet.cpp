#include "Astral/Geometry/SDFChangeSet.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace Astral {

static bool GetLocalBounds(const SDFPrimitiveRecord& rec, glm::vec3& outMin, glm::vec3& outMax) {
    const glm::vec4& dims = rec.dimensions;
    float blendFactor = std::max(0.0f, rec.metallicParams.y);
    switch (rec.primitiveType) {
        case 0: { // Sphere
            float r = std::max(dims.x, 0.001f);
            outMin = glm::vec3(-r);
            outMax = glm::vec3(r);
            break;
        }
        case 1: { // Box
            glm::vec3 h = glm::max(glm::vec3(dims.x, dims.y, dims.z), glm::vec3(0.001f));
            outMin = -h;
            outMax = h;
            break;
        }
        case 2: { // Torus
            float R = std::max(dims.x, 0.001f);
            float r = std::max(dims.y, 0.001f);
            outMin = glm::vec3(-(R + r), -r, -(R + r));
            outMax = glm::vec3(R + r, r, R + r);
            break;
        }
        case 3: { // Plane (Infinite in XZ)
            return false; // Cannot bound infinite plane; triggers global change
        }
        case 4: { // Capsule (+Y direction)
            float r = std::max(dims.x, 0.001f);
            float h = std::max(dims.y, 0.0f);
            outMin = glm::vec3(-r, -r, -r);
            outMax = glm::vec3(r, h + r, r);
            break;
        }
        case 5: { // Cylinder
            float r = std::max(dims.x, 0.001f);
            float h = std::max(dims.y, 0.001f);
            outMin = glm::vec3(-r, -h, -r);
            outMax = glm::vec3(r, h, r);
            break;
        }
        default: {
            float r = std::max(dims.x, 0.001f);
            outMin = glm::vec3(-r);
            outMax = glm::vec3(r);
            break;
        }
    }

    if (rec.operation == 3 || rec.operation == 4) { // Smooth union or subtract
        outMin -= glm::vec3(blendFactor);
        outMax += glm::vec3(blendFactor);
    }
    return true;
}

static bool GetWorldBounds(const SDFPrimitiveRecord& rec, glm::vec3& outMin, glm::vec3& outMax) {
    glm::vec3 localMin, localMax;
    if (!GetLocalBounds(rec, localMin, localMax)) {
        return false;
    }

    float det = glm::determinant(rec.invTransform);
    if (!std::isfinite(det) || std::abs(det) < 1e-7f) {
        return false;
    }

    glm::mat4 worldM = glm::inverse(rec.invTransform);
    glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z}
    };

    outMin = glm::vec3(1e9f);
    outMax = glm::vec3(-1e9f);
    for (int i = 0; i < 8; ++i) {
        glm::vec4 wp = worldM * glm::vec4(corners[i], 1.0f);
        outMin = glm::min(outMin, glm::vec3(wp));
        outMax = glm::max(outMax, glm::vec3(wp));
    }
    return true;
}

static ScreenRect ProjectWorldBoundsToScreen(
    const glm::vec3& worldMin,
    const glm::vec3& worldMax,
    const glm::mat4& viewProj,
    float dilation = 0.015f
) {
    // 1. Check if camera position is strictly inside the world bounds
    glm::mat4 invVP = glm::inverse(viewProj);
    glm::vec4 camH = invVP * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    if (std::abs(camH.w) > 1e-6f) {
        glm::vec3 camPos = glm::vec3(camH) / camH.w;
        if (camPos.x >= worldMin.x && camPos.x <= worldMax.x &&
            camPos.y >= worldMin.y && camPos.y <= worldMax.y &&
            camPos.z >= worldMin.z && camPos.z <= worldMax.z) {
            return ScreenRect{glm::vec2(0.0f), glm::vec2(1.0f)};
        }
    }

    glm::vec3 corners[8] = {
        {worldMin.x, worldMin.y, worldMin.z},
        {worldMax.x, worldMin.y, worldMin.z},
        {worldMin.x, worldMax.y, worldMin.z},
        {worldMax.x, worldMax.y, worldMin.z},
        {worldMin.x, worldMin.y, worldMax.z},
        {worldMax.x, worldMin.y, worldMax.z},
        {worldMin.x, worldMax.y, worldMax.z},
        {worldMax.x, worldMax.y, worldMax.z}
    };

    glm::vec4 clipCorners[8];
    bool allBehind = true;
    constexpr float wNear = 0.001f;

    for (int i = 0; i < 8; ++i) {
        clipCorners[i] = viewProj * glm::vec4(corners[i], 1.0f);
        if (clipCorners[i].w > wNear) {
            allBehind = false;
        }
    }

    if (allBehind) {
        return ScreenRect{};
    }

    // 2. Conservative edge clipping against near plane w = wNear
    static constexpr std::pair<int, int> edges[12] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, // X
        {0, 2}, {1, 3}, {4, 6}, {5, 7}, // Y
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Z
    };

    glm::vec2 minUV(1.0f);
    glm::vec2 maxUV(0.0f);
    bool anyValidPoint = false;

    auto includePoint = [&](const glm::vec4& clip) {
        if (clip.w <= 0.0001f) return;
        glm::vec2 ndc = glm::vec2(clip.x, clip.y) / clip.w;
        glm::vec2 uv = ndc * 0.5f + 0.5f;
        if (std::isfinite(uv.x) && std::isfinite(uv.y)) {
            minUV = glm::min(minUV, uv);
            maxUV = glm::max(maxUV, uv);
            anyValidPoint = true;
        }
    };

    for (int i = 0; i < 8; ++i) {
        if (clipCorners[i].w > wNear) {
            includePoint(clipCorners[i]);
        }
    }

    for (const auto& [i, j] : edges) {
        const glm::vec4& c0 = clipCorners[i];
        const glm::vec4& c1 = clipCorners[j];
        bool c0InFront = (c0.w > wNear);
        bool c1InFront = (c1.w > wNear);

        if (c0InFront != c1InFront) {
            float t = (wNear - c0.w) / (c1.w - c0.w);
            t = glm::clamp(t, 0.0f, 1.0f);
            glm::vec4 clipped = glm::mix(c0, c1, t);
            clipped.w = wNear;
            includePoint(clipped);
        }
    }

    if (!anyValidPoint) {
        return ScreenRect{};
    }

    minUV = glm::clamp(minUV, glm::vec2(0.0f), glm::vec2(1.0f));
    maxUV = glm::clamp(maxUV, glm::vec2(0.0f), glm::vec2(1.0f));

    // 3. Conservative screen-space dilation
    minUV = glm::max(glm::vec2(0.0f), minUV - dilation);
    maxUV = glm::min(glm::vec2(1.0f), maxUV + dilation);

    return ScreenRect{minUV, maxUV};
}

SDFChangeSet SDFChangeSet::Compare(
    const SDFSceneSnapshot& prevSnapshot,
    const SDFSceneSnapshot& currSnapshot,
    const glm::mat4& prevViewProj,
    const glm::mat4& currViewProj
) {
    SDFChangeSet result;

    const auto& prevRecords = prevSnapshot.GetRecords();
    const auto& currRecords = currSnapshot.GetRecords();

    // Map previous records by surfaceId
    std::unordered_map<uint32_t, size_t> prevMap;
    prevMap.reserve(prevRecords.size());
    for (size_t i = 0; i < prevRecords.size(); ++i) {
        prevMap[prevRecords[i].surfaceId] = i;
    }

    // Map current records by surfaceId
    std::unordered_map<uint32_t, size_t> currMap;
    currMap.reserve(currRecords.size());
    for (size_t i = 0; i < currRecords.size(); ++i) {
        currMap[currRecords[i].surfaceId] = i;
    }

    // 1. Check for removed primitives
    for (size_t i = 0; i < prevRecords.size(); ++i) {
        const auto& prev = prevRecords[i];
        if (currMap.find(prev.surfaceId) == currMap.end()) {
            PrimitiveChange pc{};
            pc.surfaceId = prev.surfaceId;
            pc.changeType = SDFChangeType::VisibilityOrRemoval;
            glm::vec3 wmin, wmax;
            if (GetWorldBounds(prev, wmin, wmax)) {
                pc.prevMinWorld = wmin;
                pc.prevMaxWorld = wmax;
                pc.combinedMinWorld = wmin;
                pc.combinedMaxWorld = wmax;
                pc.screenRect = ProjectWorldBoundsToScreen(wmin, wmax, prevViewProj);
                if (pc.screenRect.IsValid()) {
                    result.m_ScreenRects.push_back(pc.screenRect);
                }
            } else {
                result.m_IsGlobalChange = true;
            }
            result.m_Changes.push_back(pc);
        }
    }

    // 2. Check for added or modified primitives
    for (size_t i = 0; i < currRecords.size(); ++i) {
        const auto& curr = currRecords[i];
        auto it = prevMap.find(curr.surfaceId);

        if (it == prevMap.end()) {
            // Newly added primitive
            PrimitiveChange pc{};
            pc.surfaceId = curr.surfaceId;
            pc.changeType = SDFChangeType::VisibilityOrAddition;
            glm::vec3 wmin, wmax;
            if (GetWorldBounds(curr, wmin, wmax)) {
                pc.currMinWorld = wmin;
                pc.currMaxWorld = wmax;
                pc.combinedMinWorld = wmin;
                pc.combinedMaxWorld = wmax;
                pc.screenRect = ProjectWorldBoundsToScreen(wmin, wmax, currViewProj);
                if (pc.screenRect.IsValid()) {
                    result.m_ScreenRects.push_back(pc.screenRect);
                }
            } else {
                result.m_IsGlobalChange = true;
            }
            result.m_Changes.push_back(pc);
        } else {
            // Existing primitive: compare attributes
            const auto& prev = prevRecords[it->second];
            SDFChangeType type = SDFChangeType::None;

            if (curr.invTransform != prev.invTransform) {
                type = type | SDFChangeType::TransformMotion;
            }
            if (curr.dimensions != prev.dimensions ||
                curr.primitiveType != prev.primitiveType ||
                curr.operation != prev.operation ||
                curr.metallicParams.y != prev.metallicParams.y) {
                type = type | SDFChangeType::ShapeOrCSGChange;
            }
            if (curr.albedoRoughness != prev.albedoRoughness ||
                curr.metallicParams.x != prev.metallicParams.x) {
                type = type | SDFChangeType::MaterialChange;
            }
            if (curr.csgOrder != prev.csgOrder) {
                result.m_IsGlobalChange = true;
            }

            if (type != SDFChangeType::None) {
                PrimitiveChange pc{};
                pc.surfaceId = curr.surfaceId;
                pc.changeType = type;

                glm::vec3 prevMin, prevMax, currMin, currMax;
                bool prevOk = GetWorldBounds(prev, prevMin, prevMax);
                bool currOk = GetWorldBounds(curr, currMin, currMax);

                if (!prevOk || !currOk) {
                    result.m_IsGlobalChange = true;
                } else {
                    pc.prevMinWorld = prevMin;
                    pc.prevMaxWorld = prevMax;
                    pc.currMinWorld = currMin;
                    pc.currMaxWorld = currMax;
                    pc.combinedMinWorld = glm::min(prevMin, currMin);
                    pc.combinedMaxWorld = glm::max(prevMax, currMax);

                    ScreenRect sPrev = ProjectWorldBoundsToScreen(pc.combinedMinWorld, pc.combinedMaxWorld, prevViewProj);
                    ScreenRect sCurr = ProjectWorldBoundsToScreen(pc.combinedMinWorld, pc.combinedMaxWorld, currViewProj);
                    pc.screenRect = sPrev;
                    pc.screenRect.Union(sCurr);

                    if (pc.screenRect.IsValid()) {
                        result.m_ScreenRects.push_back(pc.screenRect);
                    }
                }
                result.m_Changes.push_back(pc);
            }
        }
    }

    // 3. Compact screen rects: merge intersecting/overlapping rects
    bool merged = true;
    while (merged && result.m_ScreenRects.size() > 1) {
        merged = false;
        for (size_t i = 0; i < result.m_ScreenRects.size() && !merged; ++i) {
            for (size_t j = i + 1; j < result.m_ScreenRects.size() && !merged; ++j) {
                if (result.m_ScreenRects[i].Intersects(result.m_ScreenRects[j])) {
                    result.m_ScreenRects[i].Union(result.m_ScreenRects[j]);
                    result.m_ScreenRects.erase(result.m_ScreenRects.begin() + j);
                    merged = true;
                }
            }
        }
    }

    // If more than 4 non-intersecting rects remain, merge pairs that introduce the least dead space
    while (result.m_ScreenRects.size() > 4) {
        size_t bestA = 0, bestB = 1;
        float bestCost = 1e9f;
        for (size_t i = 0; i < result.m_ScreenRects.size(); ++i) {
            for (size_t j = i + 1; j < result.m_ScreenRects.size(); ++j) {
                ScreenRect u = result.m_ScreenRects[i];
                u.Union(result.m_ScreenRects[j]);
                float cost = u.Area() - (result.m_ScreenRects[i].Area() + result.m_ScreenRects[j].Area());
                if (cost < bestCost) {
                    bestCost = cost;
                    bestA = i;
                    bestB = j;
                }
            }
        }
        result.m_ScreenRects[bestA].Union(result.m_ScreenRects[bestB]);
        result.m_ScreenRects.erase(result.m_ScreenRects.begin() + bestB);
    }

    // If total invalidation area covers > 70% of screen, consider global change
    float totalArea = 0.0f;
    for (const auto& r : result.m_ScreenRects) {
        totalArea += r.Area();
    }
    if (totalArea >= 0.70f) {
        result.m_IsGlobalChange = true;
    }

    return result;
}

void SDFChangeSet::GetPackedRects(std::array<glm::vec4, 4>& outRects, uint32_t& outCount) const noexcept {
    outCount = 0;
    for (size_t i = 0; i < 4; ++i) {
        outRects[i] = glm::vec4(0.0f);
    }
    if (m_IsGlobalChange) {
        outRects[0] = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        outCount = 1;
        return;
    }

    std::vector<ScreenRect> rects = m_ScreenRects;
    // Iteratively merge until <= 4 rects using area-weighted dead space minimization
    while (rects.size() > 4) {
        size_t bestA = 0, bestB = 1;
        float bestCost = 1e9f;
        for (size_t i = 0; i < rects.size(); ++i) {
            for (size_t j = i + 1; j < rects.size(); ++j) {
                ScreenRect u = rects[i];
                u.Union(rects[j]);
                float cost = u.Area() - (rects[i].Area() + rects[j].Area());
                if (cost < bestCost) {
                    bestCost = cost;
                    bestA = i;
                    bestB = j;
                }
            }
        }
        rects[bestA].Union(rects[bestB]);
        rects.erase(rects.begin() + bestB);
    }

    outCount = static_cast<uint32_t>(rects.size());
    for (size_t i = 0; i < rects.size() && i < 4; ++i) {
        outRects[i] = glm::vec4(rects[i].minUV.x, rects[i].minUV.y, rects[i].maxUV.x, rects[i].maxUV.y);
    }
}

} // namespace Astral
