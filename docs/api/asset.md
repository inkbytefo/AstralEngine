# Astral Engine Asset Management API Referansı

## Giriş

Bu dokümantasyon, Astral Engine'in varlık (asset) yönetim sisteminin API'lerini detaylı olarak açıklar. Asset sistemi, modeller, dokular, shader'lar ve diğer oyun varlıklarının yüklenmesi, önbelleğe alınması ve yönetilmesi için kapsamlı bir çözüm sunar.

## İçindekiler

1. [AssetSubsystem Sınıfı](#assetsubsystem-sınıfı)
2. [AssetManager Sınıfı](#assetmanager-sınıfı)
3. [AssetHandle Sınıfı](#assethandle-sınıfı)
4. [Asset Türleri](#asset-türleri)
5. [Hot Reload Sistemi](#hot-reload-sistemi)
6. [Asset Validation](#asset-validation)
7. [Asset Dependency Tracking](#asset-dependency-tracking)
8. [Thread Safety](#thread-safety)
9. [Performans Optimizasyonları](#performans-optimizasyonları)

## AssetSubsystem Sınıfı

### Genel Bakış

`AssetSubsystem` sınıfı, oyun varlıklarının yaşam döngüsünü yönetir. Asset'lerin diskten yüklenmesi, bellekte tutulması, hot reload desteği ve diğer sistemlere sunulması işlemlerinden sorumludur.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class AssetSubsystem : public ISubsystem {
public:
    AssetSubsystem();
    ~AssetSubsystem() override;
    
    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "AssetSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::PreUpdate; }
    
    // Asset Manager erişimi
    AssetManager* GetAssetManager() const { return m_assetManager.get(); }
    
    // Asset dizin yönetimi
    void SetAssetDirectory(const std::string& directory);
    const std::string& GetAssetDirectory() const;
    
private:
    std::unique_ptr<AssetManager> m_assetManager;
    Engine* m_owner = nullptr;
    std::string m_assetDirectory = "Assets";
};

} // namespace AstralEngine
```

### Başlatma ve Yapılandırma

#### AssetSubsystem Başlatma
```cpp
void AssetSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("AssetSubsystem", "Initializing asset subsystem...");
    
    // Asset dizinini çalışma dizinine göre ayarla
    m_assetDirectory = (owner->GetBasePath() / "Assets").string();
    
    // Asset Manager oluştur
    m_assetManager = std::make_unique<AssetManager>();
    if (!m_assetManager->Initialize(m_assetDirectory)) {
        Logger::Error("AssetSubsystem", "Failed to initialize AssetManager!");
        return;
    }
    
    // Hot reload'u etkinleştir
    m_assetManager->EnableHotReload(true);
    
    Logger::Info("AssetSubsystem", "Asset subsystem initialized successfully. Asset directory: '{}'", m_assetDirectory);
}
```

#### Asset Dizini Yapılandırması
```cpp
// Asset dizinini ayarla
assetSubsystem->SetAssetDirectory("MyGame/Assets");

// Mevcut dizini al
const std::string& assetDir = assetSubsystem->GetAssetDirectory();
Logger::Info("Game", "Loading assets from: {}", assetDir);
```

## AssetManager Sınıfı

### Genel Bakış

`AssetManager` sınıfı, asset'lerin yüklenmesi, önbelleğe alınması ve yönetilmesi için merkezi sistemdir. Tip güvenli asset erişimi, asenkron yükleme ve hot reload desteği sağlar.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class AssetManager {
public:
    AssetManager();
    ~AssetManager();
    
    // Yaşam döngüsü
    bool Initialize(const std::string& assetDirectory);
    void Shutdown();
    void Update(); // Hot reload kontrolü için
    
    // Asset yükleme
    template<typename T>
    AssetHandle Load(const std::string& path);
    
    template<typename T>
    std::future<AssetHandle> LoadAsync(const std::string& path);
    
    // Asset erişimi
    template<typename T>
    std::shared_ptr<T> Get(const AssetHandle& handle) const;
    
    template<typename T>
    bool IsLoaded(const AssetHandle& handle) const;
    
    // Asset bilgisi
    template<typename T>
    AssetType GetAssetType() const;
    
    // Hot reload
    void EnableHotReload(bool enable);
    void CheckForAssetChanges();
    void ReloadAsset(const AssetHandle& handle);
    
    // Asset validation
    bool ValidateAsset(const AssetHandle& handle) const;
    bool ValidateAssetFile(const std::string& filePath) const;
    std::vector<std::string> GetValidationErrors(const AssetHandle& handle) const;
    
    // Dependency tracking
    void AddAssetDependency(const AssetHandle& dependent, const AssetHandle& dependency);
    std::vector<AssetHandle> GetAssetDependencies(const AssetHandle& handle) const;
    std::vector<AssetHandle> GetDependentAssets(const AssetHandle& handle) const;
    void PropagateAssetChange(const AssetHandle& changedAsset);
    
    // İstatistikler
    size_t GetLoadedAssetCount() const;
    size_t GetTotalAssetMemory() const;
    std::vector<AssetHandle> GetAllAssets() const;
    
private:
    std::string m_assetDirectory;
    std::unique_ptr<AssetRegistry> m_registry;
    std::unique_ptr<ThreadPool> m_threadPool;
    
    // Hot reload
    bool m_hotReloadEnabled = true;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;
    
    // Dependency tracking
    std::unordered_map<AssetHandle, std::vector<AssetHandle>> m_dependencies;
    std::unordered_map<AssetHandle, std::vector<AssetHandle>> m_dependents;
    
    // Asset cache
    mutable std::shared_mutex m_cacheMutex;
    std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_cache;
};

} // namespace AstralEngine
```

### Asset Yükleme

#### Senkron Yükleme
```cpp
// Texture yükleme
AssetHandle textureHandle = assetManager->Load<Texture>("textures/brick_wall.png");

// Model yükleme
AssetHandle modelHandle = assetManager->Load<Model>("models/cube.fbx");

// Shader yükleme
AssetHandle shaderHandle = assetManager->Load<Shader>("shaders/pbr_lit.slang");

// Material yükleme
AssetHandle materialHandle = assetManager->Load<Material>("materials/brick.mat");
```

#### Asenkron Yükleme
```cpp
// Birden fazla asset'i asenkron yükle
std::vector<std::future<AssetHandle>> futures;

futures.push_back(assetManager->LoadAsync<Texture>("textures/diffuse.png"));
futures.push_back(assetManager->LoadAsync<Texture>("textures/normal.png"));
futures.push_back(assetManager->LoadAsync<Texture>("textures/metallic.png"));

// Ana thread'de devam et
LoadGameLevel();

// Asset'lerin yüklenmesini bekle
for (auto& future : futures) {
    AssetHandle handle = future.get();
    if (handle.IsValid()) {
        Logger::Info("Asset", "Asset loaded: {}", handle.GetPath());
    }
}
```

### Asset Erişimi

#### Tip Güvenli Erişim
```cpp
// Texture erişimi
std::shared_ptr<Texture> texture = assetManager->Get<Texture>(textureHandle);
if (texture && texture->IsLoaded()) {
    VkImageView imageView = texture->GetImageView();
    VkSampler sampler = texture->GetSampler();
}

// Model erişimi
std::shared_ptr<Model> model = assetManager->Get<Model>(modelHandle);
if (model && model->IsLoaded()) {
    const std::vector<Mesh>& meshes = model->GetMeshes();
    for (const auto& mesh : meshes) {
        RenderMesh(mesh);
    }
}
```

#### Asset Durumu Kontrolü
```cpp
// Asset'in yüklendiğini kontrol et
if (assetManager->IsLoaded<Texture>(textureHandle)) {
    // Asset kullanılabilir
    UseTexture(textureHandle);
} else {
    // Asset hâlâ yükleniyor veya hatalı
    UseFallbackTexture();
}
```

## AssetHandle Sınıfı

### Genel Bakış

`AssetHandle` sınıfı, asset'lere tip güvenli referanslar sağlar. Handle'lar kopyalanabilir, karşılaştırılabilir ve null kontrolü yapılabilir.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class AssetHandle {
public:
    // Constructor'lar
    AssetHandle() = default;
    AssetHandle(uint32_t id, AssetType type, const std::string& path);
    
    // Temel operatörler
    bool IsValid() const { return m_id != INVALID_ID; }
    bool operator==(const AssetHandle& other) const;
    bool operator!=(const AssetHandle& other) const;
    bool operator<(const AssetHandle& other) const;
    
    // Bilgi erişimi
    uint32_t GetID() const { return m_id; }
    AssetType GetType() const { return m_type; }
    const std::string& GetPath() const { return m_path; }
    size_t GetHash() const;
    
    // String conversion
    std::string ToString() const;
    
private:
    static constexpr uint32_t INVALID_ID = 0;
    
    uint32_t m_id = INVALID_ID;
    AssetType m_type = AssetType::Unknown;
    std::string m_path;
};

} // namespace AstralEngine
```

### Kullanım Örnekleri

#### Handle Karşılaştırması
```cpp
AssetHandle handle1 = assetManager->Load<Texture>("texture1.png");
AssetHandle handle2 = assetManager->Load<Texture>("texture2.png");

if (handle1 == handle2) {
    Logger::Debug("Asset", "Same asset loaded twice");
}

if (handle1.IsValid() && handle2.IsValid()) {
    // Her iki asset de geçerli
    UseAssets(handle1, handle2);
}
```

#### Handle Kullanımı Map'lerde
```cpp
std::map<AssetHandle, std::shared_ptr<Material>> materialCache;

// Handle'ı key olarak kullan
AssetHandle textureHandle = assetManager->Load<Texture>("metal.png");
materialCache[textureHandle] = CreateMetalMaterial();
```

## Asset Türleri

### Temel Asset Türleri

#### Texture Asset
```cpp
class Texture : public Asset {
public:
    // Vulkan resource'lar
    VkImage GetImage() const { return m_image; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    
    // Texture bilgisi
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    VkFormat GetFormat() const { return m_format; }
    uint32_t GetMipLevels() const { return m_mipLevels; }
    
    // Loading kontrolü
    bool IsLoaded() const override;
    bool LoadFromFile(const std::string& path) override;
    
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    uint32_t m_mipLevels = 1;
    
    VulkanTexture m_vulkanTexture;
};
```

#### Model Asset
```cpp
class Model : public Asset {
public:
    // Mesh erişimi
    const std::vector<Mesh>& GetMeshes() const { return m_meshes; }
    size_t GetMeshCount() const { return m_meshes.size(); }
    
    // Bounding box
    const AABB& GetBoundingBox() const { return m_boundingBox; }
    
    // Loading kontrolü
    bool IsLoaded() const override;
    bool LoadFromFile(const std::string& path) override;
    
private:
    std::vector<Mesh> m_meshes;
    AABB m_boundingBox;
    
    bool LoadFromOBJ(const std::string& path);
    bool LoadFromFBX(const std::string& path);
    bool LoadFromGLTF(const std::string& path);
};
```

#### Shader Asset
```cpp
class Shader : public Asset {
public:
    // Shader modülleri
    VkShaderModule GetVertexModule() const { return m_vertexModule; }
    VkShaderModule GetFragmentModule() const { return m_fragmentModule; }
    VkShaderModule GetGeometryModule() const { return m_geometryModule; }
    VkShaderModule GetComputeModule() const { return m_computeModule; }
    
    // Shader tipi
    ShaderType GetShaderType() const { return m_shaderType; }
    
    // Loading kontrolü
    bool IsLoaded() const override;
    bool LoadFromFile(const std::string& path) override;
    bool LoadFromSource(const std::string& source, ShaderType type);
    
private:
    ShaderType m_shaderType = ShaderType::Graphics;
    
    VkShaderModule m_vertexModule = VK_NULL_HANDLE;
    VkShaderModule m_fragmentModule = VK_NULL_HANDLE;
    VkShaderModule m_geometryModule = VK_NULL_HANDLE;
    VkShaderModule m_computeModule = VK_NULL_HANDLE;
};
```

### Asset Kullanımı

#### Texture Kullanımı
```cpp
// Texture yükle
AssetHandle textureHandle = assetManager->Load<Texture>("textures/wood.png");

// Texture'a eriş
std::shared_ptr<Texture> texture = assetManager->Get<Texture>(textureHandle);
if (texture && texture->IsLoaded()) {
    // Vulkan resource'ları al
    VkImage image = texture->GetImage();
    VkImageView imageView = texture->GetImageView();
    VkSampler sampler = texture->GetSampler();
    
    // Texture bilgileri
    uint32_t width = texture->GetWidth();
    uint32_t height = texture->GetHeight();
    VkFormat format = texture->GetFormat();
    
    // Material'de kullan
    material->SetAlbedoTexture(textureHandle);
}
```

#### Model Kullanımı
```cpp
// Model yükle
AssetHandle modelHandle = assetManager->Load<Model>("models/cube.fbx");

// Model'e eriş
std::shared_ptr<Model> model = assetManager->Get<Model>(modelHandle);
if (model && model->IsLoaded()) {
    // Tüm mesh'leri al
    const std::vector<Mesh>& meshes = model->GetMeshes();
    
    for (const auto& mesh : meshes) {
        // Vertex buffer'lar
        VkBuffer vertexBuffer = mesh.GetVertexBuffer();
        VkBuffer indexBuffer = mesh.GetIndexBuffer();
        
        // Vertex attribute'lar
        const std::vector<VertexAttribute>& attributes = mesh.GetAttributes();
        
        // Draw call
        vkCmdDrawIndexed(commandBuffer, mesh.GetIndexCount(), 1, 0, 0, 0);
    }
    
    // Bounding box
    const AABB& bounds = model->GetBoundingBox();
    glm::vec3 center = bounds.GetCenter();
    glm::vec3 extents = bounds.GetExtents();
}
```

## Hot Reload Sistemi

### Genel Bakış

Hot reload sistemi, asset dosyalarındaki değişiklikleri otomatik olarak algılar ve asset'leri yeniden yükler. Developer productivity'yi artırır ve hızlı iterasyon sağlar.

### Hot Reload Yapılandırması

#### Hot Reload Etkinleştirme
```cpp
// Hot reload'u etkinleştir
assetManager->EnableHotReload(true);

// Hot reload kontrolünü devre dışı bırak (production için)
assetManager->EnableHotReload(false);
```

#### Manuel Hot Reload Kontrolü
```cpp
void AssetSubsystem::OnUpdate(float deltaTime) {
    // Hot reload kontrolü
    if (m_assetManager) {
        m_assetManager->Update(); // Asset Manager güncellemeleri
        m_assetManager->CheckForAssetChanges(); // Dosya değişikliklerini kontrol et
    }
}
```

### Dosya İzleme ve Değişiklik Algılama

#### Dosya Timestamp İzleme
```cpp
void AssetManager::CheckForAssetChanges() {
    if (!m_hotReloadEnabled) return;
    
    // Tüm kayıtlı asset'leri kontrol et
    for (const auto& [path, handle] : m_registry->GetAllAssets()) {
        try {
            // Dosya hâlâ var mı?
            if (!std::filesystem::exists(path)) {
                Logger::Warning("AssetManager", "Asset file missing: {}", path);
                continue;
            }
            
            // Dosya zamanını al
            auto currentTime = std::filesystem::last_write_time(path);
            auto it = m_fileTimestamps.find(path);
            
            // İlk kontrol mü?
            if (it == m_fileTimestamps.end()) {
                m_fileTimestamps[path] = currentTime;
                continue;
            }
            
            // Dosya değişmiş mi?
            if (it->second != currentTime) {
                Logger::Info("AssetManager", "Asset changed: {}", path);
                
                // Asset'i yeniden yükle
                ReloadAsset(handle);
                
                // Timestamp'i güncelle
                it->second = currentTime;
                
                // Bağımlı asset'leri güncelle
                PropagateAssetChange(handle);
            }
            
        } catch (const std::exception& e) {
            Logger::Error("AssetManager", "Failed to check asset {}: {}", path, e.what());
        }
    }
}
```

### Asset Yeniden Yükleme

#### Tekil Asset Yeniden Yükleme
```cpp
void AssetManager::ReloadAsset(const AssetHandle& handle) {
    try {
        auto asset = GetAsset(handle);
        if (!asset) {
            Logger::Warning("AssetManager", "Cannot reload asset: not found");
            return;
        }
        
        Logger::Info("AssetManager", "Reloading asset: {}", handle.GetPath());
        
        // Asset'i yeniden yükle
        bool success = asset->Reload();
        
        if (success) {
            Logger::Info("AssetManager", "Asset reloaded successfully: {}", handle.GetPath());
            
            // Event yayınla
            EventManager::GetInstance().PublishEvent<AssetReloadedEvent>(handle);
            
        } else {
            Logger::Error("AssetManager", "Failed to reload asset: {}", handle.GetPath());
            
            // Event yayınla
            EventManager::GetInstance().PublishEvent<AssetReloadFailedEvent>(handle);
        }
        
    } catch (const std::exception& e) {
        Logger::Error("AssetManager", "Asset reload failed for {}: {}", 
                     handle.GetPath(), e.what());
    }
}
```

## Asset Validation

### Genel Bakış

Asset validation sistemi, asset'lerin bütünlüğünü ve format doğruluğunu kontrol eder. Bozuk asset'lerin sisteme girmesini engeller.

### Validation Türleri

#### Dosya Bütünlüğü Kontrolü
```cpp
bool AssetManager::ValidateAssetFile(const std::string& filePath) const {
    try {
        // Dosya var mı?
        if (!std::filesystem::exists(filePath)) {
            Logger::Error("Validation", "File not found: {}", filePath);
            return false;
        }
        
        // Dosya boş mu?
        if (std::filesystem::file_size(filePath) == 0) {
            Logger::Error("Validation", "Empty file: {}", filePath);
            return false;
        }
        
        // Dosya okunabilir mi?
        std::ifstream testFile(filePath, std::ios::binary);
        if (!testFile.good()) {
            Logger::Error("Validation", "Cannot read file: {}", filePath);
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Validation", "File validation failed for {}: {}", 
                     filePath, e.what());
        return false;
    }
}
```

#### Format Doğrulama
```cpp
bool AssetManager::ValidateShaderFile(const std::string& filePath) const {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return false;
        
        // SPIR-V magic number kontrolü
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        
        if (magic != 0x07230203) { // SPIR-V magic number
            Logger::Error("Validation", "Invalid SPIR-V magic number in {}", filePath);
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Validation", "Shader validation failed for {}: {}", 
                     filePath, e.what());
        return false;
    }
}
```

#### Asset Tipi Özel Kontroller
```cpp
bool AssetManager::ValidateTextureFile(const std::string& filePath) const {
    try {
        // PNG kontrolü
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return false;
        
        // PNG signature kontrolü
        unsigned char signature[8];
        file.read(reinterpret_cast<char*>(signature), 8);
        
        const unsigned char pngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        if (std::memcmp(signature, pngSignature, 8) == 0) {
            return true; // Geçerli PNG
        }
        
        // JPEG kontrolü
        if (signature[0] == 0xFF && signature[1] == 0xD8) {
            return true; // Geçerli JPEG
        }
        
        Logger::Error("Validation", "Unknown texture format: {}", filePath);
        return false;
        
    } catch (const std::exception& e) {
        Logger::Error("Validation", "Texture validation failed for {}: {}", 
                     filePath, e.what());
        return false;
    }
}
```

### Validation Hataları

#### Detaylı Hata Raporlama
```cpp
std::vector<std::string> AssetManager::GetValidationErrors(const AssetHandle& handle) const {
    std::vector<std::string> errors;
    
    try {
        auto asset = GetAsset(handle);
        if (!asset) {
            errors.push_back("Asset not found in cache");
            return errors;
        }
        
        // Dosya bütünlüğü kontrolü
        if (!std::filesystem::exists(handle.GetPath())) {
            errors.push_back("Asset file not found: " + handle.GetPath());
        }
        
        // Asset tipine özel kontroller
        switch (handle.GetType()) {
            case AssetType::Texture:
                ValidateTextureErrors(handle.GetPath(), errors);
                break;
            case AssetType::Shader:
                ValidateShaderErrors(handle.GetPath(), errors);
                break;
            case AssetType::Model:
                ValidateModelErrors(handle.GetPath(), errors);
                break;
            default:
                break;
        }
        
        // Bellek kontrolü
        if (!asset->IsLoaded()) {
            errors.push_back("Asset not loaded into memory");
        }
        
    } catch (const std::exception& e) {
        errors.push_back("Validation exception: " + std::string(e.what()));
    }
    
    return errors;
}
```

## Asset Dependency Tracking

### Genel Bakış

Asset bağımlılık takip sistemi, asset'ler arasındaki ilişkileri izler. Bir asset değiştiğinde bağımlı olan asset'ler otomatik olarak güncellenir.

### Dependency Yönetimi

#### Bağımlılık Ekleme
```cpp
void AssetManager::AddAssetDependency(const AssetHandle& dependent, const AssetHandle& dependency) {
    if (!dependent.IsValid() || !dependency.IsValid()) {
        Logger::Warning("AssetManager", "Invalid dependency relationship");
        return;
    }
    
    // Dependency listesine ekle
    auto& deps = m_dependencies[dependent];
    if (std::find(deps.begin(), deps.end(), dependency) == deps.end()) {
        deps.push_back(dependency);
    }
    
    // Reverse dependency listesine ekle
    auto& revDeps = m_dependents[dependency];
    if (std::find(revDeps.begin(), revDeps.end(), dependent) == revDeps.end()) {
        revDeps.push_back(dependent);
    }
    
    Logger::Debug("AssetManager", "Dependency added: {} -> {}", 
                 dependent.GetPath(), dependency.GetPath());
}
```

#### Bağımlılık Sorgulama
```cpp
std::vector<AssetHandle> AssetManager::GetAssetDependencies(const AssetHandle& handle) const {
    auto it = m_dependencies.find(handle);
    return (it != m_dependencies.end()) ? it->second : std::vector<AssetHandle>{};
}

std::vector<AssetHandle> AssetManager::GetDependentAssets(const AssetHandle& handle) const {
    auto it = m_dependents.find(handle);
    return (it != m_dependents.end()) ? it->second : std::vector<AssetHandle>{};
}
```

### Bağımlılık Yayılımı

#### Değişiklik Yayılımı
```cpp
void AssetManager::PropagateAssetChange(const AssetHandle& changedAsset) {
    Logger::Info("AssetManager", "Propagating change from: {}", changedAsset.GetPath());
    
    // Tüm bağımlı asset'leri al
    auto dependents = GetDependentAssets(changedAsset);
    
    for (const auto& dependent : dependents) {
        Logger::Info("AssetManager", "Updating dependent asset: {}", dependent.GetPath());
        
        // Bağımlı asset'i yeniden yükle
        ReloadAsset(dependent);
        
        // Recursive olarak alt bağımlıları da güncelle
        PropagateAssetChange(dependent);
    }
}
```

#### Pratik Kullanım Örneği
```cpp
// Material oluştur ve texture'lara bağımlılık ekle
AssetHandle materialHandle = CreateMaterial("metal_material");

AssetHandle albedoHandle = assetManager->Load<Texture>("textures/metal_albedo.png");
AssetHandle normalHandle = assetManager->Load<Texture>("textures/metal_normal.png");
AssetHandle metallicHandle = assetManager->Load<Texture>("textures/metal_metallic.png");

// Bağımlılıkları tanımla
assetManager->AddAssetDependency(materialHandle, albedoHandle);
assetManager->AddAssetDependency(materialHandle, normalHandle);
assetManager->AddAssetDependency(materialHandle, metallicHandle);

// Texture'lardan biri değiştiğinde material otomatik güncellenir
```

## Thread Safety

### Concurrent Access

#### Thread-Safe Cache Erişimi
```cpp
template<typename T>
std::shared_ptr<T> AssetManager::Get(const AssetHandle& handle) const {
    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    
    auto it = m_cache.find(handle);
    if (it != m_cache.end()) {
        return std::static_pointer_cast<T>(it->second);
    }
    
    return nullptr;
}

template<typename T>
void AssetManager::CacheAsset(const AssetHandle& handle, std::shared_ptr<T> asset) {
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_cache[handle] = std::static_pointer_cast<Asset>(asset);
}
```

#### Thread-Safe Yükleme
```cpp
template<typename T>
std::future<AssetHandle> AssetManager::LoadAsync(const std::string& path) {
    return m_threadPool->Submit([this, path]() {
        try {
            // Thread-safe asset yükleme
            auto asset = LoadAsset<T>(path);
            
            // Cache'e thread-safe ekleme
            AssetHandle handle = asset->GetHandle();
            CacheAsset(handle, asset);
            
            return handle;
            
        } catch (const std::exception& e) {
            Logger::Error("AssetManager", "Async load failed for {}: {}", path, e.what());
            return AssetHandle(); // Invalid handle
        }
    });
}
```

### Race Condition Önleme

#### Double-Checked Locking
```cpp
template<typename T>
AssetHandle AssetManager::Load(const std::string& path) {
    // Önce cache'i kontrol et (read lock)
    {
        std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
        auto cached = FindCachedAsset<T>(path);
        if (cached.IsValid()) {
            return cached;
        }
    }
    
    // Cache miss - yükleme yap (write lock)
    {
        std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
        
        // Tekrar kontrol et (double-checked locking)
        auto cached = FindCachedAsset<T>(path);
        if (cached.IsValid()) {
            return cached;
        }
        
        // Asset'i yükle
        auto asset = LoadAsset<T>(path);
        AssetHandle handle = asset->GetHandle();
        m_cache[handle] = std::static_pointer_cast<Asset>(asset);
        
        return handle;
    }
}
```

## Performans Optimizasyonları

### Cache Optimizasyonları

#### LRU Cache
```cpp
class AssetCache {
    std::unordered_map<AssetHandle, CacheEntry> m_cache;
    std::list<AssetHandle> m_lruList;
    std::mutex m_mutex;
    size_t m_maxSize;
    
public:
    template<typename T>
    std::shared_ptr<T> Get(const AssetHandle& handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_cache.find(handle);
        if (it == m_cache.end()) {
            return nullptr;
        }
        
        // LRU listesini güncelle
        m_lruList.remove(handle);
        m_lruList.push_front(handle);
        
        return std::static_pointer_cast<T>(it->second.asset);
    }
    
    void SetMaxSize(size_t maxSize) { m_maxSize = maxSize; }
    
private:
    void EvictLRU() {
        while (m_cache.size() > m_maxSize && !m_lruList.empty()) {
            AssetHandle oldest = m_lruList.back();
            m_lruList.pop_back();
            m_cache.erase(oldest);
        }
    }
};
```

### Async Loading Optimizasyonu

#### Priority Queue
```cpp
class AsyncAssetLoader {
    std::priority_queue<AssetLoadRequest> m_loadQueue;
    ThreadPool m_threadPool;
    
public:
    void QueueAsset(const std::string& path, AssetLoadPriority priority) {
        m_loadQueue.emplace(path, priority);
    }
    
    void ProcessQueue() {
        while (!m_loadQueue.empty()) {
            auto request = m_loadQueue.top();
            m_loadQueue.pop();
            
            m_threadPool.Submit([request]() {
                return LoadAsset(request.path);
            });
        }
    }
};
```

### Memory Pool

#### Asset Memory Pool
```cpp
class AssetMemoryPool {
    std::vector<std::unique_ptr<char[]>> m_pools;
    std::vector<size_t> m_poolOffsets;
    std::mutex m_mutex;
    
    static constexpr size_t POOL_SIZE = 16 * 1024 * 1024; // 16MB
    
public:
    void* Allocate(size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Uygun pool'u bul
        for (size_t i = 0; i < m_pools.size(); ++i) {
            if (m_poolOffsets[i] + size <= POOL_SIZE) {
                void* ptr = m_pools[i].get() + m_poolOffsets[i];
                m_poolOffsets[i] += size;
                return ptr;
            }
        }
        
        // Yeni pool oluştur
        m_pools.emplace_back(std::make_unique<char[]>(POOL_SIZE));
        m_poolOffsets.push_back(size);
        return m_pools.back().get();
    }
};
```

## Örnek Uygulamalar

### Asset Yönetim Sistemi Kullanımı

#### Temel Asset Yükleme ve Kullanım
```cpp
class Game {
    AssetManager* m_assetManager;
    
public:
    void LoadAssets() {
        // Texture yükle
        m_diffuseTexture = m_assetManager->Load<Texture>("textures/character_diffuse.png");
        m_normalTexture = m_assetManager->Load<Texture>("textures/character_normal.png");
        m_metallicTexture = m_assetManager->Load<Texture>("textures/character_metallic.png");
        
        // Model yükle
        m_characterModel = m_assetManager->Load<Model>("models/character.fbx");
        
        // Shader yükle
        m_pbrShader = m_assetManager->Load<Shader>("shaders/pbr_lit.slang");
        
        // Material oluştur
        m_characterMaterial = CreateMaterial("character_material", m_pbrShader);
        m_characterMaterial->SetAlbedoTexture(m_diffuseTexture);
        m_characterMaterial->SetNormalTexture(m_normalTexture);
        m_characterMaterial->SetMetallicTexture(m_metallicTexture);
    }
    
    void RenderCharacter() {
        // Model'e eriş
        auto model = m_assetManager->Get<Model>(m_characterModel);
        if (!model || !model->IsLoaded()) return;
        
        // Tüm mesh'leri render et
        const auto& meshes = model->GetMeshes();
        for (const auto& mesh : meshes) {
            // Material'i ayarla
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipelineLayout, 0, 1, 
                                  &m_characterMaterial->GetDescriptorSet(), 0, nullptr);
            
            // Mesh'i render et
            VkBuffer vertexBuffers[] = { mesh.GetVertexBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
            
            vkCmdDrawIndexed(commandBuffer, mesh.GetIndexCount(), 1, 0, 0, 0);
        }
    }
    
private:
    AssetHandle m_diffuseTexture;
    AssetHandle m_normalTexture;
    AssetHandle m_metallicTexture;
    AssetHandle m_characterModel;
    AssetHandle m_pbrShader;
    std::shared_ptr<Material> m_characterMaterial;
};
```

#### Async Asset Yükleme ile Loading Screen
```cpp
class LoadingScreen {
    AssetManager* m_assetManager;
    std::vector<std::future<AssetHandle>> m_loadFutures;
    int m_loadedCount = 0;
    int m_totalCount = 0;
    
public:
    void StartLoading() {
        // Async asset yükleme başlat
        m_loadFutures.push_back(m_assetManager->LoadAsync<Texture>("textures/level1_diffuse.png"));
        m_loadFutures.push_back(m_assetManager->LoadAsync<Texture>("textures/level1_normal.png"));
        m_loadFutures.push_back(m_assetManager->LoadAsync<Model>("models/level1_static.fbx"));
        m_loadFutures.push_back(m_assetManager->LoadAsync<Model>("models/level1_dynamic.fbx"));
        
        m_totalCount = m_loadFutures.size();
        m_loadedCount = 0;
    }
    
    bool UpdateLoading() {
        // Yüklenen asset'leri kontrol et
        for (auto& future : m_loadFutures) {
            if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                AssetHandle handle = future.get();
                if (handle.IsValid()) {
                    m_loadedCount++;
                    Logger::Info("Loading", "Loaded: {}", handle.GetPath());
                }
            }
        }
        
        // Progress güncelle
        float progress = static_cast<float>(m_loadedCount) / m_totalCount;
        UpdateProgressBar(progress);
        
        // Tüm asset'ler yüklendi mi?
        return m_loadedCount >= m_totalCount;
    }
};
```

#### Hot Reload ile Development
```cpp
class DeveloperTools {
    AssetManager* m_assetManager;
    
public:
    void EnableHotReload() {
        // Hot reload'u etkinleştir
        m_assetManager->EnableHotReload(true);
        
        // Asset değişikliklerini izle
        EventManager::GetInstance().Subscribe<AssetReloadedEvent>(
            [this](AssetReloadedEvent& event) {
                Logger::Info("Dev", "Asset reloaded: {}", event.handle.GetPath());
                OnAssetReloaded(event.handle);
                return true;
            }
        );
    }
    
    void OnAssetReloaded(const AssetHandle& handle) {
        // Asset tipine göre işlem yap
        switch (handle.GetType()) {
            case AssetType::Texture:
                ReloadTextureMaterials(handle);
                break;
            case AssetType::Shader:
                ReloadShaders(handle);
                break;
            case AssetType::Model:
                ReloadModels(handle);
                break;
            default:
                break;
        }
    }
    
private:
    void ReloadTextureMaterials(const AssetHandle& textureHandle) {
        // Bu texture'a bağımlı material'leri bul
        auto dependentMaterials = m_assetManager->GetDependentAssets(textureHandle);
        
        for (const auto& materialHandle : dependentMaterials) {
            auto material = m_assetManager->Get<Material>(materialHandle);
            if (material) {
                material->UpdateUniformBuffer();
                Logger::Info("Dev", "Updated material: {}", materialHandle.GetPath());
            }
        }
    }
};
```

## Hata Yönetimi ve Debugging

### Validation Kullanımı

#### Asset Validation
```cpp
bool ValidateGameAssets() {
    auto* assetManager = engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();
    
    // Tüm asset'leri kontrol et
    auto allAssets = assetManager->GetAllAssets();
    
    for (const auto& handle : allAssets) {
        // Asset validation
        if (!assetManager->ValidateAsset(handle)) {
            auto errors = assetManager->GetValidationErrors(handle);
            for (const auto& error : errors) {
                Logger::Error("Validation", "{}: {}", handle.GetPath(), error);
            }
            return false;
        }
    }
    
    Logger::Info("Validation", "All assets validated successfully");
    return true;
}
```

### Memory Leak Detection

#### Asset Memory Tracking
```cpp
class AssetMemoryTracker {
    size_t m_initialMemory = 0;
    
public:
    void StartTracking() {
        m_initialMemory = MemoryManager::GetInstance().GetTotalAllocated();
    }
    
    void ReportMemoryUsage() {
        size_t currentMemory = MemoryManager::GetInstance().GetTotalAllocated();
        size_t assetMemory = currentMemory - m_initialMemory;
        
        Logger::Info("Memory", "Asset memory usage: {} bytes", assetMemory);
        
        // Asset başına memory kullanımı
        auto* assetManager = engine->GetSubsystem<AssetSubsystem>()->GetAssetManager();
        size_t assetCount = assetManager->GetLoadedAssetCount();
        
        if (assetCount > 0) {
            size_t avgMemoryPerAsset = assetMemory / assetCount;
            Logger::Info("Memory", "Average memory per asset: {} bytes", avgMemoryPerAsset);
        }
    }
};
```

## Performans ve En İyi Uygulamalar

### Asset Loading Best Practices

#### 1. Async Loading Kullanımı
```cpp
// Do: Async yükleme kullan
auto future = assetManager->LoadAsync<Texture>("large_texture.png");
// Ana thread'de devam et
DoOtherWork();
auto handle = future.get();

// Don't: Senkron yükleme ile ana thread'i blokla
auto handle = assetManager->Load<Texture>("large_texture.png"); // Blocking!
```

#### 2. Batch Loading
```cpp
// Do: Birden fazla asset'i aynı anda yükle
std::vector<std::future<AssetHandle>> futures;
futures.push_back(assetManager->LoadAsync<Texture>("tex1.png"));
futures.push_back(assetManager->LoadAsync<Texture>("tex2.png"));
futures.push_back(assetManager->LoadAsync<Texture>("tex3.png"));

// Hepsini aynı anda bekle
for (auto& future : futures) {
    future.wait();
}

// Don't: Tek tek yükle
auto h1 = assetManager->LoadAsync<Texture>("tex1.png"); h1.get();
auto h2 = assetManager->LoadAsync<Texture>("tex2.png"); h2.get();
auto h3 = assetManager->LoadAsync<Texture>("tex3.png"); h3.get();
```

#### 3. Cache Kullanımı
```cpp
// Do: Cache'i kontrol et önce
auto cached = assetManager->Get<Texture>(handle);
if (cached) {
    return cached; // Cache hit
}

// Don't: Her seferinde yükle
auto handle = assetManager->Load<Texture>(path); // Cache miss
```

### Memory Management

#### 1. Frame Memory Kullanımı
```cpp
// Do: Geçici veriler için frame memory kullan
void ProcessAssetData() {
    auto& memoryManager = MemoryManager::GetInstance();
    void* tempBuffer = memoryManager.AllocateFrameMemory(1024);
    // Kullan...
    // Otomatik temizlik
}

// Don't: Manuel allocate/free
void* buffer = memoryManager.Allocate(1024);
// Kullan...
memoryManager.Deallocate(buffer); // Unutma riski
```

#### 2. Shared Pointer Kullanımı
```cpp
// Do: Shared pointer ile otomatik yönetim
std::shared_ptr<Texture> texture = assetManager->Get<Texture>(handle);
// Otomatik temizlik

// Don't: Raw pointer'lar
Texture* texture = assetManager->Get<Texture>(handle).get();
// Memory leak riski
```

## Sık Karşılaşılan Sorular

**S: Asset yükleme ne kadar sürer?**
A: Dosya boyutuna ve türe bağlıdır. Küçük texture'lar: 1-10ms, Büyük modeller: 100-1000ms. Async yükleme önerilir.

**S: Kaç asset aynı anda yüklenebilir?**
A: Teknik olarak sınırsız, ancak memory için 1000-10000 arası pratik sınırdır.

**S: Hot reload production'da kullanılmalı mı?**
A: Hayır, sadece development için. Production'da devre dışı bırakın.

**S: Asset'ler ne zaman memory'den temizlenir?**
A: LRU cache politikası ile otomatik temizlik veya manuel `UnloadAsset()` çağrısı ile.

**S: Custom asset türleri nasıl eklenir?**
A: `Asset` sınıfından türetin ve `AssetManager::RegisterAssetType<T>()` ile kaydedin.

**S: Asset validation nasıl etkinleştirilir?**
A: `ValidateAsset()` veya `ValidateAssetFile()` çağrısı ile manuel kontrol yapın.

Bu API referansı, Astral Engine'in güçlü ve esnek asset yönetim sisteminin tüm özelliklerini kapsamaktadır. Hot reload, validation, dependency tracking ve thread-safe tasarım ile profesyonel oyun geliştirme için optimize edilmiştir.