#pragma once

#include "IAssetImporter.h"

namespace AstralEngine {

	/**
	 * @class ShaderImporter
	 * @brief SPIR-V shader dosyalarını (.spv) ShaderData'ya dönüştürür.
	 */
	class ShaderImporter : public IAssetImporter {
	public:
		std::shared_ptr<void> Import(const std::string& filePath) override;
	};

} // namespace AstralEngine
