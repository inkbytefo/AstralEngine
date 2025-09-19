#pragma once

#include <glm/glm.hpp>
#include <array>

namespace AstralEngine {

    /**
     * @struct AABB
     * @brief Axis-Aligned Bounding Box (Eksenle Hizalı Sınırlayıcı Kutu).
     * 
     * Bir nesneyi çevreleyen ve eksenlere paralel olan en küçük kutuyu temsil eder.
     * Hızlı kesişim testleri için kullanılır.
     */
    struct AABB {
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

        AABB() = default;
        AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

        // İki AABB'yi birleştirerek yeni bir AABB oluşturur
        void Merge(const AABB& other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        // Bir noktayı içerecek şekilde AABB'yi genişletir
        void Extend(const glm::vec3& point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        // AABB'yi bir matris ile dönüştürür
        AABB Transform(const glm::mat4& matrix) const {
            std::array<glm::vec3, 8> corners = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, max.z),
            };

            AABB transformedAABB;
            for (const auto& corner : corners) {
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
