#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declaration
namespace AstralEngine {
class VulkanMesh;
}

namespace AstralEngine {

/**
 * @class Model
 * @brief 3D model sınıfı - birden fazla mesh'i bir arada tutar
 * 
 * Bu sınıf, Assimp ile yüklenen 3D modellerin tüm mesh parçalarını
 * bir arada tutar ve yönetir. Her bir mesh ayrı bir VulkanMesh nesnesi
 * olarak temsil edilir.
 */
class Model {
public:
    /**
     * @brief Model constructor
     * @param filePath Model dosyasının yolu
     */
    explicit Model(const std::string& filePath);
    
    /**
     * @brief Model destructor
     */
    ~Model() = default;

    // Non-copyable
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Movable
    Model(Model&&) = default;
    Model& operator=(Model&&) = default;

    /**
     * @brief Model'e yeni bir mesh ekle
     * @param mesh Eklenecek VulkanMesh nesnesi
     */
    void AddMesh(std::shared_ptr<VulkanMesh> mesh);

    /**
     * @brief Model'deki tüm mesh'leri döndür
     * @return Mesh'lerin referans vektörü
     */
    const std::vector<std::shared_ptr<VulkanMesh>>& GetMeshes() const;

    /**
     * @brief Model'deki mesh sayısını döndür
     * @return Mesh sayısı
     */
    size_t GetMeshCount() const;

    /**
     * @brief Model dosyasının yolunu döndür
     * @return Dosya yolu
     */
    const std::string& GetFilePath() const;

    /**
     * @brief Model adını döndür (dosya adından)
     * @return Model adı
     */
    std::string GetName() const;

    /**
     * @brief Model'in geçerli olup olmadığını kontrol et
     * @return Geçerli ise true
     */
    bool IsValid() const;

    /**
     * @brief Model'i geçersiz yap
     */
    void Invalidate();

private:
    std::string m_filePath;
    std::vector<std::shared_ptr<VulkanMesh>> m_meshes;
    bool m_isValid;
};

} // namespace AstralEngine
