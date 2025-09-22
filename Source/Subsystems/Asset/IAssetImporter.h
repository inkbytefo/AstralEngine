#pragma once

#include <string>
#include <memory>

namespace AstralEngine {

	// Forward-declare AssetData types to avoid including AssetData.h in this interface
	struct ModelData;
	struct TextureData;
	struct ShaderData;
	struct MaterialData;

	/**
	 * @class IAssetImporter
	 * @brief Varlık dosyalarını okuyup CPU-taraflı ham verilere dönüştüren
	 *        tüm importer sınıfları için temel arayüz.
	 */
	class IAssetImporter {
	public:
		virtual ~IAssetImporter() = default;

		/**
		 * @brief Varlığı verilen dosya yolundan içe aktarır.
		 * @param filePath İçe aktarılacak varlığın tam dosya yolu.
		 * @return Başarılı olursa varlık verilerini içeren bir std::shared_ptr<void>,
		 *         başarısız olursa nullptr döndürür.
		 */
		virtual std::shared_ptr<void> Import(const std::string& filePath) = 0;
	};

} // namespace AstralEngine
