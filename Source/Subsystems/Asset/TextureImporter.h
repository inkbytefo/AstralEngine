#pragma once

#include "IAssetImporter.h"

namespace AstralEngine {

	/**
	 * @class TextureImporter
	 * @brief Görüntü dosyalarını (PNG, JPG, vb.) TextureData'ya dönüştürür.
	 */
	class TextureImporter : public IAssetImporter {
	public:
		std::shared_ptr<void> Import(const std::string& filePath) override;
	};

} // namespace AstralEngine
