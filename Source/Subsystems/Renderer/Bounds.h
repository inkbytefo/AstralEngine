#pragma once

#include <glm/glm.hpp>
#include <array>
#include <limits>

namespace AstralEngine {

    /**
     * @struct AABB
     * @brief Axis-Aligned Bounding Box (Eksenle Hizalı Sınırlayıcı Kutu).
     *
     * Bir nesneyi çevreleyen ve eksenlere paralel olan en küçük kutuyu temsil eder.
     * Hızlı kesişim testleri için kullanılır. Modern C++ özellikleriyle optimize edilmiştir.
     */
    struct AABB {
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

        constexpr AABB() noexcept = default;
        constexpr AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) noexcept : min(minPoint), max(maxPoint) {}

        // Resets the AABB to an invalid state, ready to be extended.
        void Reset() noexcept {
            min = glm::vec3(std::numeric_limits<float>::max());
            max = glm::vec3(std::numeric_limits<float>::lowest());
        }

        // Checks if the AABB is valid (i.e., has been extended).
        [[nodiscard]] constexpr bool IsValid() const noexcept {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }

        // Returns the center point of the AABB.
        [[nodiscard]] constexpr glm::vec3 GetCenter() const noexcept {
            return (min + max) * 0.5f;
        }

        // Returns the size (dimensions) of the AABB.
        [[nodiscard]] constexpr glm::vec3 GetSize() const noexcept {
            return max - min;
        }

        // Merges another AABB into this one.
        void Merge(const AABB& other) noexcept {
            if (other.IsValid()) {
                min = glm::min(min, other.min);
                max = glm::max(max, other.max);
            }
        }

        // Extends the AABB to include a point.
        void Extend(const glm::vec3& point) noexcept {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        // Transforms the AABB by a matrix and returns the new AABB.
        [[nodiscard]] AABB Transform(const glm::mat4& matrix) const noexcept {
            if (!IsValid()) {
                return {};
            }

            static const std::array<glm::vec3, 8> cornersTemplate = {
                glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1),
                glm::vec3(1, 1, 0), glm::vec3(0, 1, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1)
            };

            AABB transformedAABB;
            const glm::vec3 size = GetSize();
            for (const auto& cornerT : cornersTemplate) {
                glm::vec3 corner = min + size * cornerT;
                transformedAABB.Extend(glm::vec3(matrix * glm::vec4(corner, 1.0f)));
            }
            return transformedAABB;
        }
    };

    /**
     * @struct Frustum
     * @brief Kameranın görüş alanını tanımlayan 6 düzlem.
     * 
     * Frustum culling için kullanılır. Her düzlem (plane) Ax + By + Cz + D = 0 denklemiyle
     * temsil edilir, burada (A, B, C) normal vektörüdür ve D mesafedir.
     */
    struct Frustum {
        std::array<glm::vec4, 6> planes; // Left, Right, Bottom, Top, Near, Far

        // Bir AABB'nin frustum içinde olup olmadığını kontrol eder.
        // Tamamen dışarıdaysa false, içeride veya kesişiyorsa true döner.
        bool Intersects(const AABB& aabb) const {
            for (int i = 0; i < 6; ++i) {
                const auto& plane = planes[i];
                glm::vec3 positiveVertex(
                    (plane.x < 0) ? aabb.min.x : aabb.max.x,
                    (plane.y < 0) ? aabb.min.y : aabb.max.y,
                    (plane.z < 0) ? aabb.min.z : aabb.max.z
                );

                if (glm::dot(glm::vec3(plane), positiveVertex) + plane.w < 0.0f) {
                    return false; // Kutu tamamen düzlemin negatif tarafında, yani frustum dışında
                }
            }
            return true; // Kutu ya içeride ya da kesişiyor
        }
    };

} // namespace AstralEngine
