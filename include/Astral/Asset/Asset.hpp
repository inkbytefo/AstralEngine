#pragma once
#include "Astral/Core/UUID.hpp"

namespace Astral {

    // Motorun destekleyeceği varlık türleri
    enum class AssetType : uint16_t {
        None = 0,
        Scene,
        Texture2D,
        Mesh,
        Material,
        Shader,
        Audio
    };

    // Tüm varlıkların türeyeceği temel sınıf
    class Asset {
    public:
        virtual ~Asset() = default;

        // Varlığın bellekteki ve diskteki benzersiz kimliği
        UUID Handle;
        
        virtual AssetType GetType() const = 0;
        
        bool IsValid() const { return Handle != 0; }
    };

} // namespace Astral
