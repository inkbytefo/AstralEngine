#pragma once
#include <cstdint>
#include <functional>

namespace Astral {

    // 64-bit Benzersiz Kimlik Sınıfı
    class UUID {
    public:
        UUID(); // Yeni ve benzersiz bir ID oluşturur
        UUID(uint64_t uuid); // Var olan bir ID ile oluşturur (Dosyadan okurken)
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID; }
        bool IsValid() const { return m_UUID != 0; }
        
    private:
        uint64_t m_UUID;
    };

} // namespace Astral

// UUID'nin std::unordered_map gibi hash tabanlı konteynerlerde kullanılabilmesi için:
namespace std {
    template<>
    struct hash<Astral::UUID> {
        std::size_t operator()(const Astral::UUID& uuid) const {
            return hash<uint64_t>()((uint64_t)uuid);
        }
    };
}
