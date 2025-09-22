#pragma once

#include "IAssetImporter.h"

namespace AstralEngine {

	/**
	 * @class ModelImporter
	 * @brief 3D model dosyalarını (FBX, OBJ, vb.) ModelData'ya dönüştürür.
	 */
	class ModelImporter : public IAssetImporter {
	public:
		std::shared_ptr<void> Import(const std::string& filePath) override;
	};

} // namespace AstralEngine
