#pragma once

#include "../../Core/Math/Bounds.h"
#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>


namespace AstralEngine {

/**
 * @struct Vertex
 * @brief 3D model vertex verisi
 *
 * Position, normal, texture coordinates ve diğer vertex attributelarını içerir.
 */
struct Vertex {
  glm::vec3 position;  ///< Position (x, y, z)
  glm::vec3 normal;    ///< Normal vector (nx, ny, nz)
  glm::vec2 texCoord;  ///< Texture coordinates (u, v)
  glm::vec3 tangent;   ///< Tangent vector (tx, ty, tz)
  glm::vec3 bitangent; ///< Bitangent vector (bx, by, bz)

  // Constructor with default values
  Vertex(const glm::vec3 &pos = glm::vec3(0.0f),
         const glm::vec3 &norm = glm::vec3(0.0f),
         const glm::vec2 &uv = glm::vec2(0.0f),
         const glm::vec3 &tang = glm::vec3(0.0f),
         const glm::vec3 &bitang = glm::vec3(0.0f))
      : position(pos), normal(norm), texCoord(uv), tangent(tang),
        bitangent(bitang) {}

  // Equality operator for comparison
  bool operator==(const Vertex &other) const {
    return position == other.position && normal == other.normal &&
           texCoord == other.texCoord && tangent == other.tangent &&
           bitangent == other.bitangent;
  }
};

/**
 * @struct ModelData
 * @brief CPU-side model verisi
 *
 * 3D modellerin ham vertex ve index verilerini içerir.
 * GPU'ya göndermeden önceki ara formattır.
 */
struct ModelData {
  std::vector<Vertex> vertices;  ///< Vertex verileri
  std::vector<uint32_t> indices; ///< Index verileri
  AABB boundingBox;              ///< Modelin sınırlayıcı kutusu
  std::string filePath;          ///< Model dosyasının yolu
  std::string name;              ///< Model adı
  bool isValid = false;          ///< Veri geçerli mi?

  ModelData() = default;

  /**
   * @brief Constructor with file path
   * @param path Model dosyasının yolu
   */
  explicit ModelData(const std::string &path) : filePath(path) {}

  /**
   * @brief Model verisinin geçerli olup olmadığını kontrol et
   * @return Geçerli ise true
   */
  bool IsValid() const {
    return isValid && !vertices.empty() && !indices.empty();
  }

  /**
   * @brief Model verisini geçersiz yap
   */
  void Invalidate() {
    isValid = false;
    vertices.clear();
    indices.clear();
  }

  /**
   * @brief Vertex sayısını döndür
   * @return Vertex sayısı
   */
  size_t GetVertexCount() const { return vertices.size(); }

  /**
   * @brief Index sayısını döndür
   * @return Index sayısı
   */
  size_t GetIndexCount() const { return indices.size(); }

  /**
   * @brief Modelin bellek kullanımını hesapla (bytes)
   * @return Bellek kullanımı
   */
  size_t GetMemoryUsage() const {
    return (vertices.size() * sizeof(Vertex)) +
           (indices.size() * sizeof(uint32_t));
  }
};

/**
 * @struct TextureData
 * @brief CPU-side texture verisi
 *
 * Texture'ların ham pixel verilerini içerir.
 * GPU'ya göndermeden önceki ara formattır.
 */
struct TextureData {
  void *data = nullptr;  ///< Pixel verisi (stbi_uc* veya benzeri)
  uint32_t width = 0;    ///< Genişlik
  uint32_t height = 0;   ///< Yükseklik
  uint32_t channels = 0; ///< Kanal sayısı (1-4)
  std::string filePath;  ///< Texture dosyasının yolu
  std::string name;      ///< Texture adı
  bool isValid = false;  ///< Veri geçerli mi?

  TextureData() = default;

  /**
   * @brief Constructor with file path
   * @param path Texture dosyasının yolu
   */
  explicit TextureData(const std::string &path) : filePath(path) {}

  /**
   * @brief Destructor - belleği temizle
   */
  ~TextureData() { Free(); }

  // Non-copyable
  TextureData(const TextureData &) = delete;
  TextureData &operator=(const TextureData &) = delete;

  // Movable
  TextureData(TextureData &&other) noexcept { *this = std::move(other); }

  TextureData &operator=(TextureData &&other) noexcept {
    if (this != &other) {
      Free();

      data = other.data;
      width = other.width;
      height = other.height;
      channels = other.channels;
      filePath = std::move(other.filePath);
      name = std::move(other.name);
      isValid = other.isValid;

      other.data = nullptr;
      other.width = 0;
      other.height = 0;
      other.channels = 0;
      other.isValid = false;
    }
    return *this;
  }

  /**
   * @brief Bellek tahsis et
   * @param w Genişlik
   * @param h Yükseklik
   * @param c Kanal sayısı
   * @return Başarılı ise true
   */
  bool Allocate(uint32_t w, uint32_t h, uint32_t c) {
    Free();

    width = w;
    height = h;
    channels = c;

    size_t dataSize = width * height * channels;
    data = malloc(dataSize);

    if (!data) {
      isValid = false;
      return false;
    }

    isValid = true;
    return true;
  }

  /**
   * @brief Belleği serbest bırak
   */
  void Free() {
    if (data) {
      free(data);
      data = nullptr;
    }
    width = 0;
    height = 0;
    channels = 0;
    isValid = false;
  }

  /**
   * @brief Texture verisinin geçerli olup olmadığını kontrol et
   * @return Geçerli ise true
   */
  bool IsValid() const {
    return isValid && data != nullptr && width > 0 && height > 0 &&
           channels > 0;
  }

  /**
   * @brief Texture verisini geçersiz yap
   */
  void Invalidate() { Free(); }

  /**
   * @brief Texture'in bellek kullanımını hesapla (bytes)
   * @return Bellek kullanımı
   */
  size_t GetMemoryUsage() const { return width * height * channels; }
};

/**
 * @struct ShaderData
 * @brief CPU-side shader verisi
 *
 * Shader'ların ham kaynak kodunu içerir.
 * GPU'ya göndermeden önceki ara formattır.
 */
struct ShaderData {
  std::vector<uint32_t> spirvCode; ///< Pre-compiled SPIR-V binary code
  std::string filePath;            ///< Shader dosyasının yolu (referans için)
  std::string name;                ///< Shader adı
  enum class Type {
    Vertex,                 ///< Vertex shader
    Fragment,               ///< Fragment shader
    Compute,                ///< Compute shader
    Geometry,               ///< Geometry shader
    TessellationControl,    ///< Tessellation control shader
    TessellationEvaluation, ///< Tessellation evaluation shader
    Unknown                 ///< Bilinmeyen tip
  } type = Type::Unknown;   ///< Shader tipi
  bool isValid = false;     ///< Veri geçerli mi?

  ShaderData() = default;

  /**
   * @brief Constructor with file path and type
   * @param path Shader dosyasının yolu
   * @param shaderType Shader tipi
   */
  ShaderData(const std::string &path, Type shaderType = Type::Unknown)
      : filePath(path), type(shaderType) {}

  /**
   * @brief Shader verisinin geçerli olup olmadığını kontrol et
   * @return Geçerli ise true
   */
  bool IsValid() const { return isValid && !spirvCode.empty(); }

  /**
   * @brief Shader verisini geçersiz yap
   */
  void Invalidate() {
    spirvCode.clear();
    isValid = false;
  }

  /**
   * @brief Shader tipini string olarak döndür
   * @return Shader tipi string'i
   */
  std::string GetTypeString() const {
    switch (type) {
    case Type::Vertex:
      return "Vertex";
    case Type::Fragment:
      return "Fragment";
    case Type::Compute:
      return "Compute";
    case Type::Geometry:
      return "Geometry";
    case Type::TessellationControl:
      return "TessellationControl";
    case Type::TessellationEvaluation:
      return "TessellationEvaluation";
    default:
      return "Unknown";
    }
  }

  /**
   * @brief Shader'in bellek kullanımını hesapla (bytes)
   * @return Bellek kullanımı
   */
  size_t GetMemoryUsage() const { return spirvCode.size() * sizeof(uint32_t); }
};

/**
 * @struct MaterialData
 * @brief CPU-side material verisi
 *
 * Material'ların ham verilerini içerir (texture path'leri, shader path'leri,
 * özellikler). GPU'ya göndermeden önceki ara formattır.
 */
struct MaterialData {
  std::string name;               ///< Material adı
  std::string vertexShaderPath;   ///< Vertex shader yolu
  std::string fragmentShaderPath; ///< Fragment shader yolu
  // PBR Texture Maps
  std::string albedoMapPath;    ///< Albedo/BaseColor map
  std::string normalMapPath;    ///< Normal map
  std::string metallicMapPath;  ///< Metallic map
  std::string roughnessMapPath; ///< Roughness map
  std::string aoMapPath;        ///< Ambient Occlusion map
  std::string emissiveMapPath;  ///< Emissive map

  // Legacy support (to be removed)
  std::vector<std::string> texturePaths;

  struct Properties {
    glm::vec3 baseColor = glm::vec3(1.0f); ///< Base color (RGB)
    float metallic = 0.0f;                 ///< Metallic value (0.0 - 1.0)
    float roughness = 0.5f;                ///< Roughness value (0.0 - 1.0)
    float ao = 1.0f;                       ///< Ambient occlusion (0.0 - 1.0)
    glm::vec3 emissiveColor = glm::vec3(0.0f); ///< Emissive color (RGB)
    float emissiveIntensity = 0.0f;            ///< Emissive intensity
    float opacity = 1.0f;                      ///< Opacity/Alpha (0.0 - 1.0)
    bool transparent = false;                  ///< Transparent materyal mi?
    bool doubleSided = false;                  ///< Double-sided rendering
    bool wireframe = false;                    ///< Wireframe rendering
  } properties;                                ///< Material özellikleri
  std::string filePath;                        ///< Material dosyasının yolu
  bool isValid = false;                        ///< Veri geçerli mi?

  MaterialData() = default;

  /**
   * @brief Constructor with file path
   * @param path Material dosyasının yolu
   */
  explicit MaterialData(const std::string &path) : filePath(path) {}

  /**
   * @brief Material verisinin geçerli olup olmadığını kontrol et
   * @return Geçerli ise true
   */
  bool IsValid() const {
    return isValid && !name.empty() && !vertexShaderPath.empty() &&
           !fragmentShaderPath.empty();
  }

  /**
   * @brief Material verisini geçersiz yap
   */
  void Invalidate() {
    name.clear();
    vertexShaderPath.clear();
    fragmentShaderPath.clear();
    texturePaths.clear();
    properties = Properties();
    isValid = false;
  }

  /**
   * @brief Texture path ekle
   * @param texturePath Texture yolu
   */
  void AddTexturePath(const std::string &texturePath) {
    texturePaths.push_back(texturePath);
  }

  /**
   * @brief Texture sayısını döndür
   * @return Texture sayısı
   */
  size_t GetTextureCount() const { return texturePaths.size(); }

  /**
   * @brief Material'in bellek kullanımını hesapla (bytes)
   * @return Bellek kullanımı
   */
  size_t GetMemoryUsage() const {
    size_t usage = name.size() + vertexShaderPath.size() +
                   fragmentShaderPath.size() + filePath.size();
    for (const auto &path : texturePaths) {
      usage += path.size();
    }
    return usage * sizeof(char);
  }
};

} // namespace AstralEngine
