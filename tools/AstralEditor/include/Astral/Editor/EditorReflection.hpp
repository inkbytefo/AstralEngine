#pragma once
#include "Astral/Core/Components.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cstddef>

namespace Astral {

    // Desteklenen özellik tipleri
    enum class PropType {
        Float,
        Int,
        Vector3,
        Color3,
        Bool,
        AssetHandle // Gelecekte UUID bağlamak için
    };

    // Tek bir değişkenin (Property) meta verisi
    struct PropertyMeta {
        std::string Name;
        PropType Type;
        size_t Offset;     // Bellekteki konumu (offsetof ile alınacak)
        float MinVal = 0.0f;
        float MaxVal = 0.0f;
    };

    // Bir bileşenin (Component) meta verisi
    struct ComponentMeta {
        std::string Name;
        std::vector<PropertyMeta> Properties;
    };

    // Editörün bileşenleri otomatik çizmesi için Kayıt Defteri
    class EditorReflection {
    public:
        // Yeni bir bileşeni kaydeder
        template<typename T>
        static void RegisterComponent(const std::string& name) {
            s_Registry[typeid(T)] = ComponentMeta{ name, {} };
        }

        // Bileşene ait bir değişkeni (Property) kaydeder
        template<typename Class>
        static void RegisterProperty(const std::string& propName, PropType type, size_t offset, float min = 0.0f, float max = 0.0f) {
            s_Registry[typeid(Class)].Properties.push_back({ propName, type, offset, min, max });
        }

        // Tip indeksine göre meta veriyi döndürür
        static const ComponentMeta* GetComponentMeta(std::type_index type) {
            auto it = s_Registry.find(type);
            return it != s_Registry.end() ? &it->second : nullptr;
        }

        static const std::unordered_map<std::type_index, ComponentMeta>& GetRegistry() {
            return s_Registry;
        }

    private:
        static inline std::unordered_map<std::type_index, ComponentMeta> s_Registry;
    };

} // namespace Astral

// Kullanımı kolaylaştıracak harika bir makro (offsetof kullanır)
#define REGISTER_PROPERTY(ComponentClass, MemberName, propType, DisplayName, Min, Max) \
    Astral::EditorReflection::RegisterProperty<ComponentClass>(DisplayName, Astral::PropType::propType, offsetof(ComponentClass, MemberName), Min, Max)

namespace Astral {
class EditorReflectionInit {
public:
    static void RegisterAll() {
        static const bool registered = [] {
            EditorReflection::RegisterComponent<VelocityComponent>("Fizik & Hiz (Velocity)");
            REGISTER_PROPERTY(VelocityComponent, linear, Vector3, "Lineer Hiz", 0.0f, 0.0f);
            REGISTER_PROPERTY(VelocityComponent, angular, Vector3, "Acisal Hiz", 0.0f, 0.0f);
            EditorReflection::RegisterComponent<HealthComponent>("Can / Saglik (Health)");
            REGISTER_PROPERTY(HealthComponent, hp, Int, "Mevcut Can", 0.0f, 100.0f);
            return true;
        }();
        (void)registered;
    }
};
} // namespace Astral
