#pragma once

#include "IAssetImporter.h"

namespace AstralEngine {

	class AssetManager; // Forward declaration

	/**
	 * @class MaterialImporter
	 * @brief Materyal dosyalarını (.amat) MaterialData'ya dönüştürür ve
	 *        bağımlı olduğu texture/shader varlıklarının yüklenmesini tetikler.
	 */
	class MaterialImporter : public IAssetImporter {
	public:
		// AssetManager'a ihtiyaç duyar, çünkü bağımlılıkları yüklemesi gerekir.
		explicit MaterialImporter(AssetManager* owner);

		std::shared_ptr<void> Import(const std::string& filePath) override;

	private:
		AssetManager* m_ownerManager;
	};

} // namespace AstralEngine
