#pragma once

#include "../RHI/IRHIDevice.h"
#include "Texture.h"
#include "Mesh.h"
#include <memory>
#include <vector>

namespace AstralEngine {

/**
 * @brief IBL verilerini (Irradiance, Prefilter, BRDF LUT) GPU üzerinde üreten yardımcı sınıf.
 */
class IBLProcessor {
public:
    IBLProcessor(IRHIDevice* device);
    ~IBLProcessor() = default;

    /**
     * @brief Equirectangular HDR dokusunu Cubemap'e dönüştürür.
     */
    std::shared_ptr<Texture> ConvertEquirectangularToCubemap(const std::shared_ptr<Texture>& equirectTexture, uint32_t size = 1024);

    /**
     * @brief Cubemap'ten Irradiance Map üretir.
     */
    std::shared_ptr<Texture> CreateIrradianceMap(const std::shared_ptr<Texture>& cubemap, uint32_t size = 32);

    /**
     * @brief Cubemap'ten Prefiltered Map üretir.
     */
    std::shared_ptr<Texture> CreatePrefilteredMap(const std::shared_ptr<Texture>& cubemap, uint32_t size = 128);

    /**
     * @brief BRDF Look-up Table üretir.
     */
    std::shared_ptr<Texture> CreateBRDFLookUpTable(uint32_t size = 512);

private:
    IRHIDevice* m_device;
    std::shared_ptr<Mesh> m_cubeMesh;
    std::shared_ptr<Mesh> m_quadMesh;

    // Shader yardımcıları (Slang dosyalarından yüklenecek)
    std::vector<uint8_t> LoadShaderCode(const std::string& path);
};

} // namespace AstralEngine
