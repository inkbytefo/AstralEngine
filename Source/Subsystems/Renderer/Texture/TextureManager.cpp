#include "TextureManager.h"
#include "../../../Core/Logger.h"
#include "../Core/VulkanDevice.h"
#include "../Buffers/VulkanBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <algorithm>
#include <chrono>
#include <sstream>

namespace AstralEngine {

// TextureData implementasyonu
TextureData::TextureData(TextureData&& other) noexcept {
    data = other.data;
    width = other.width;
    height = other.height;
    channels = other.channels;
    format = other.format;
    size = other.size;
    
    other.data = nullptr;
    other.size = 0;
}

TextureData& TextureData::operator=(TextureData&& other) noexcept {
    if (this != &other) {
        Free();
        
        data = other.data;
        width = other.width;
        height = other.height;
        channels = other.channels;
        format = other.format;
        size = other.size;
        
        other.data = nullptr;
        other.size = 0;
    }
    return *this;
}

void TextureData::Allocate(uint32_t w, uint32_t h, uint32_t c, TextureFormat fmt) {
    Free();
    
    width = w;
    height = h;
    channels = c;
    format = fmt;
    
    // Format'a göre pixel boyutunu hesapla
    uint32_t pixelSize = 0;
    switch (fmt) {
        case TextureFormat::R8_UNORM: pixelSize = 1; break;
        case TextureFormat::R8G8_UNORM: pixelSize = 2; break;
        case TextureFormat::R8G8B8_UNORM: pixelSize = 3; break;
        case TextureFormat::R8G8B8A8_UNORM:
        case TextureFormat::R8G8B8A8_SRGB: pixelSize = 4; break;
        case TextureFormat::R32_SFLOAT: pixelSize = 4; break;
        case TextureFormat::R32G32_SFLOAT: pixelSize = 8; break;
        case TextureFormat::R32G32B32_SFLOAT: pixelSize = 12; break;
        case TextureFormat::R32G32B32A32_SFLOAT: pixelSize = 16; break;
        default: pixelSize = 4; break;
    }
    
    size = static_cast<size_t>(width) * height * pixelSize;
    data = malloc(size);
    
    if (!data) {
        size = 0;
        throw std::bad_alloc();
    }
}

void TextureData::Free() {
    if (data) {
        free(data);
        data = nullptr;
    }
    size = 0;
}

// Texture sınıfı implementasyonu
Texture::Texture() 
    : m_device(nullptr)
    , m_isInitialized(false)
    , m_loaded(false) {
    
    Logger::Debug("Texture", "Texture created");
}

Texture::~Texture() {
    if (m_isInitialized) {
        Shutdown();
    }
    Logger::Debug("Texture", "Texture destroyed: {}", m_info.name);
}

bool Texture::Initialize(VulkanDevice* device, const TextureInfo& info) {
    if (m_isInitialized) {
        Logger::Warning("Texture", "Texture already initialized: {}", m_info.name);
        return true;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = device;
    m_info = info;
    
    Logger::Info("Texture", "Initializing texture: {}", m_info.name);
    
    try {
        // Vulkan texture oluştur
        m_vulkanTexture = std::make_shared<VulkanTexture>();
        if (!m_vulkanTexture->Initialize(m_device, m_info.filePath)) {
            SetError("Failed to create Vulkan texture: " + m_vulkanTexture->GetLastError());
            return false;
        }
        
        // Sampler oluştur
        if (!CreateSampler()) {
            return false;
        }
        
        m_loaded = true;
        m_isInitialized = true;
        
        Logger::Info("Texture", "Texture initialized successfully: {}", m_info.name);
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during texture initialization: ") + e.what());
        Logger::Error("Texture", "Texture initialization failed: {}", e.what());
        return false;
    }
}

bool Texture::InitializeFromData(VulkanDevice* device, const TextureData& data, const TextureInfo& info) {
    if (m_isInitialized) {
        Logger::Warning("Texture", "Texture already initialized: {}", m_info.name);
        return true;
    }
    
    if (!device || !data.IsValid()) {
        SetError("Invalid device or texture data");
        return false;
    }
    
    m_device = device;
    m_info = info;
    m_info.width = data.width;
    m_info.height = data.height;
    m_info.channels = data.channels;
    
    Logger::Info("Texture", "Initializing texture from data: {}", m_info.name);
    
    try {
        // Vulkan texture oluştur
        VkFormat vkFormat = ConvertToVkFormat(data.format);
        m_vulkanTexture = std::make_shared<VulkanTexture>();
        if (!m_vulkanTexture->InitializeFromData(m_device, data.data, data.width, data.height, vkFormat)) {
            SetError("Failed to create Vulkan texture from data: " + m_vulkanTexture->GetLastError());
            return false;
        }
        
        // Sampler oluştur
        if (!CreateSampler()) {
            return false;
        }
        
        m_loaded = true;
        m_isInitialized = true;
        
        Logger::Info("Texture", "Texture initialized from data successfully: {}", m_info.name);
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during texture initialization from data: ") + e.what());
        Logger::Error("Texture", "Texture initialization from data failed: {}", e.what());
        return false;
    }
}

void Texture::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    Logger::Info("Texture", "Shutting down texture: {}", m_info.name);
    
    // Sampler'ı temizle
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device->GetDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    
    // Vulkan texture'ı temizle
    m_vulkanTexture.reset();
    
    m_device = nullptr;
    m_loaded = false;
    m_isInitialized = false;
    
    Logger::Info("Texture", "Texture shutdown completed: {}", m_info.name);
}

bool Texture::LoadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::Info("Texture", "Loading texture from file: {}", filePath);
    
    try {
        if (!LoadWithSTB(filePath)) {
            return false;
        }
        
        m_info.filePath = filePath;
        m_loaded = true;
        
        UpdateAccessTime();
        Logger::Info("Texture", "Texture loaded successfully: {}", filePath);
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception loading texture: ") + e.what());
        Logger::Error("Texture", "Failed to load texture {}: {}", filePath, e.what());
        return false;
    }
}

bool Texture::LoadFromMemory(const void* data, uint32_t width, uint32_t height, TextureFormat format) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!data || width == 0 || height == 0) {
        SetError("Invalid parameters for LoadFromMemory");
        return false;
    }
    
    Logger::Info("Texture", "Loading texture from memory: {}x{}", width, height);
    
    try {
        if (!SaveToMemory(data, width, height, format)) {
            return false;
        }
        
        m_loaded = true;
        
        UpdateAccessTime();
        Logger::Info("Texture", "Texture loaded from memory successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception loading texture from memory: ") + e.what());
        Logger::Error("Texture", "Failed to load texture from memory: {}", e.what());
        return false;
    }
}

bool Texture::LoadAsCubemap(const std::vector<std::string>& facePaths) {
    if (facePaths.size() != 6) {
        SetError("Cubemap requires exactly 6 face paths");
        return false;
    }
    
    Logger::Info("Texture", "Loading cubemap with 6 faces");
    
    // TODO: Cubemap implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Cubemap loading not yet implemented");
    return false;
}

bool Texture::CreateCheckerboard(uint32_t width, uint32_t height, uint32_t squareSize, 
                                const glm::vec3& color1, const glm::vec3& color2) {
    Logger::Info("Texture", "Creating checkerboard texture: {}x{}", width, height);
    
    try {
        TextureData data;
        data.Allocate(width, height, 4, TextureFormat::R8G8B8A8_UNORM);
        
        uint8_t* pixels = static_cast<uint8_t*>(data.data);
        
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                bool evenX = (x / squareSize) % 2 == 0;
                bool evenY = (y / squareSize) % 2 == 0;
                bool useColor1 = (evenX && evenY) || (!evenX && !evenY);
                
                const glm::vec3& color = useColor1 ? color1 : color2;
                
                uint32_t index = (y * width + x) * 4;
                pixels[index] = static_cast<uint8_t>(color.r * 255.0f);
                pixels[index + 1] = static_cast<uint8_t>(color.g * 255.0f);
                pixels[index + 2] = static_cast<uint8_t>(color.b * 255.0f);
                pixels[index + 3] = 255; // Alpha
            }
        }
        
        if (!InitializeFromData(m_device, data, m_info)) {
            data.Free();
            return false;
        }
        
        data.Free();
        m_loaded = true;
        
        Logger::Info("Texture", "Checkerboard texture created successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception creating checkerboard texture: ") + e.what());
        Logger::Error("Texture", "Failed to create checkerboard texture: {}", e.what());
        return false;
    }
}

bool Texture::CreateGradient(uint32_t width, uint32_t height, 
                            const glm::vec3& startColor, const glm::vec3& endColor, 
                            bool horizontal) {
    Logger::Info("Texture", "Creating gradient texture: {}x{}", width, height);
    
    try {
        TextureData data;
        data.Allocate(width, height, 4, TextureFormat::R8G8B8A8_UNORM);
        
        uint8_t* pixels = static_cast<uint8_t*>(data.data);
        
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                float t = horizontal ? (float)x / (float)(width - 1) : (float)y / (float)(height - 1);
                glm::vec3 color = glm::mix(startColor, endColor, t);
                
                uint32_t index = (y * width + x) * 4;
                pixels[index] = static_cast<uint8_t>(color.r * 255.0f);
                pixels[index + 1] = static_cast<uint8_t>(color.g * 255.0f);
                pixels[index + 2] = static_cast<uint8_t>(color.b * 255.0f);
                pixels[index + 3] = 255; // Alpha
            }
        }
        
        if (!InitializeFromData(m_device, data, m_info)) {
            data.Free();
            return false;
        }
        
        data.Free();
        m_loaded = true;
        
        Logger::Info("Texture", "Gradient texture created successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception creating gradient texture: ") + e.what());
        Logger::Error("Texture", "Failed to create gradient texture: {}", e.what());
        return false;
    }
}

bool Texture::CreateNoise(uint32_t width, uint32_t height, float scale, float persistence) {
    Logger::Info("Texture", "Creating noise texture: {}x{}", width, height);
    
    try {
        TextureData data;
        data.Allocate(width, height, 4, TextureFormat::R8G8B8A8_UNORM);
        
        uint8_t* pixels = static_cast<uint8_t*>(data.data);
        
        // Simple noise generation
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                float noise = 0.0f;
                float amplitude = 1.0f;
                float frequency = scale;
                
                for (int i = 0; i < 4; ++i) {
                    float sampleX = x * frequency;
                    float sampleY = y * frequency;
                    
                    // Simple pseudo-random noise
                    float value = sin(sampleX * 0.1f) * cos(sampleY * 0.1f) + 
                                 sin(sampleX * 0.05f) * cos(sampleY * 0.05f);
                    
                    noise += value * amplitude;
                    amplitude *= persistence;
                    frequency *= 2.0f;
                }
                
                // Normalize to 0-1 range
                noise = (noise + 2.0f) / 4.0f;
                noise = glm::clamp(noise, 0.0f, 1.0f);
                
                uint32_t index = (y * width + x) * 4;
                uint8_t value = static_cast<uint8_t>(noise * 255.0f);
                pixels[index] = value;     // R
                pixels[index + 1] = value; // G
                pixels[index + 2] = value; // B
                pixels[index + 3] = 255;   // A
            }
        }
        
        if (!InitializeFromData(m_device, data, m_info)) {
            data.Free();
            return false;
        }
        
        data.Free();
        m_loaded = true;
        
        Logger::Info("Texture", "Noise texture created successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception creating noise texture: ") + e.what());
        Logger::Error("Texture", "Failed to create noise texture: {}", e.what());
        return false;
    }
}

bool Texture::CreateNormalMapFromHeightmap(const Texture* heightmap, float strength) {
    if (!heightmap || !heightmap->IsLoaded()) {
        SetError("Invalid heightmap texture");
        return false;
    }
    
    Logger::Info("Texture", "Creating normal map from heightmap");
    
    // TODO: Normal map generation implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Normal map generation not yet implemented");
    return false;
}

bool Texture::GenerateMipmaps() {
    if (!m_isInitialized || !m_vulkanTexture) {
        SetError("Texture not initialized");
        return false;
    }
    
    Logger::Info("Texture", "Generating mipmaps for texture: {}", m_info.name);
    
    // TODO: Mipmap generation implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Mipmap generation not yet implemented");
    return false;
}

bool Texture::ConvertFormat(TextureFormat newFormat) {
    if (!m_isInitialized) {
        SetError("Texture not initialized");
        return false;
    }
    
    Logger::Info("Texture", "Converting texture format: {} -> {}", 
                static_cast<int>(m_info.format), static_cast<int>(newFormat));
    
    // TODO: Format conversion implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Format conversion not yet implemented");
    return false;
}

bool Texture::Resize(uint32_t newWidth, uint32_t newHeight) {
    if (!m_isInitialized) {
        SetError("Texture not initialized");
        return false;
    }
    
    Logger::Info("Texture", "Resizing texture: {}x{} -> {}x{}", 
                m_info.width, m_info.height, newWidth, newHeight);
    
    // TODO: Resize implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Texture resize not yet implemented");
    return false;
}

bool Texture::FlipHorizontal() {
    Logger::Info("Texture", "Flipping texture horizontally");
    
    // TODO: Horizontal flip implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Horizontal flip not yet implemented");
    return false;
}

bool Texture::FlipVertical() {
    Logger::Info("Texture", "Flipping texture vertically");
    
    // TODO: Vertical flip implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Vertical flip not yet implemented");
    return false;
}

bool Texture::Rotate90(bool clockwise) {
    Logger::Info("Texture", "Rotating texture 90 degrees {}", clockwise ? "clockwise" : "counter-clockwise");
    
    // TODO: Rotation implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Texture rotation not yet implemented");
    return false;
}

bool Texture::AddToAtlas(Texture* otherTexture, uint32_t x, uint32_t y) {
    if (!otherTexture || !otherTexture->IsLoaded()) {
        SetError("Invalid other texture");
        return false;
    }
    
    Logger::Info("Texture", "Adding texture to atlas at position ({}, {})", x, y);
    
    // TODO: Atlas operations implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Atlas operations not yet implemented");
    return false;
}

bool Texture::ExtractRegion(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Texture* outTexture) {
    if (!outTexture) {
        SetError("Invalid output texture");
        return false;
    }
    
    Logger::Info("Texture", "Extracting region ({}, {}, {}, {}) from texture", x, y, width, height);
    
    // TODO: Region extraction implementasyonu
    // Bu kısım daha sonra implement edilecek
    Logger::Warning("Texture", "Region extraction not yet implemented");
    return false;
}

VkImageView Texture::GetImageView() const {
    if (m_vulkanTexture) {
        return m_vulkanTexture->GetImageView();
    }
    return VK_NULL_HANDLE;
}

VkSampler Texture::GetSampler() const {
    return m_sampler;
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_info.minFilter = minFilter;
    m_info.magFilter = magFilter;
    
    if (m_isInitialized) {
        UpdateSampler();
    }
}

void Texture::SetWrap(TextureWrap wrapU, TextureWrap wrapV, TextureWrap wrapW) {
    m_info.wrapU = wrapU;
    m_info.wrapV = wrapV;
    m_info.wrapW = wrapW;
    
    if (m_isInitialized) {
        UpdateSampler();
    }
}

void Texture::SetAnisotropy(float anisotropy) {
    m_info.anisotropy = glm::clamp(anisotropy, 1.0f, 16.0f);
    
    if (m_isInitialized) {
        UpdateSampler();
    }
}

void Texture::SetBorderColor(const glm::vec4& color) {
    m_info.borderColor = color;
    
    if (m_isInitialized) {
        UpdateSampler();
    }
}

VkFormat Texture::ConvertToVkFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case TextureFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::BC1_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case TextureFormat::BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TextureFormat::BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

VkFilter Texture::ConvertToVkFilter(TextureFilter filter) const {
    switch (filter) {
        case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        case TextureFilter::Linear:
        case TextureFilter::Bilinear:
        case TextureFilter::Trilinear: return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode Texture::ConvertToVkSamplerAddressMode(TextureWrap wrap) const {
    switch (wrap) {
        case TextureWrap::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case TextureWrap::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

bool Texture::CreateSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = ConvertToVkFilter(m_info.magFilter);
    samplerInfo.minFilter = ConvertToVkFilter(m_info.minFilter);
    samplerInfo.addressModeU = ConvertToVkSamplerAddressMode(m_info.wrapU);
    samplerInfo.addressModeV = ConvertToVkSamplerAddressMode(m_info.wrapV);
    samplerInfo.addressModeW = ConvertToVkSamplerAddressMode(m_info.wrapW);
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = m_info.anisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_info.mipLevels);
    
    VkResult result = vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler);
    if (result != VK_SUCCESS) {
        SetError("Failed to create sampler: " + std::to_string(result));
        return false;
    }
    
    Logger::Debug("Texture", "Sampler created for texture: {}", m_info.name);
    return true;
}

bool Texture::UpdateSampler() {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device->GetDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    
    return CreateSampler();
}

void Texture::UpdateAccessTime() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_info.lastAccessTime = static_cast<uint64_t>(now);
}

void Texture::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("Texture", "Error in texture {}: {}", m_info.name, error);
}

bool Texture::LoadWithSTB(const std::string& filePath) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        SetError("Failed to load texture with STB: " + filePath);
        return false;
    }
    
    m_info.width = static_cast<uint32_t>(width);
    m_info.height = static_cast<uint32_t>(height);
    m_info.channels = 4; // Her zaman RGBA
    
    VkFormat vkFormat = m_info.sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    
    // Vulkan texture oluştur
    m_vulkanTexture = std::make_shared<VulkanTexture>();
    if (!m_vulkanTexture->InitializeFromData(m_device, pixels, m_info.width, m_info.height, vkFormat)) {
        stbi_image_free(pixels);
        SetError("Failed to create Vulkan texture: " + m_vulkanTexture->GetLastError());
        return false;
    }
    
    stbi_image_free(pixels);
    
    // Sampler oluştur
    if (!CreateSampler()) {
        return false;
    }
    
    return true;
}

bool Texture::SaveToMemory(const void* data, uint32_t width, uint32_t height, TextureFormat format) {
    m_info.width = width;
    m_info.height = height;
    m_info.format = format;
    
    VkFormat vkFormat = ConvertToVkFormat(format);
    
    // Vulkan texture oluştur
    m_vulkanTexture = std::make_shared<VulkanTexture>();
    if (!m_vulkanTexture->InitializeFromData(m_device, data, width, height, vkFormat)) {
        SetError("Failed to create Vulkan texture from memory: " + m_vulkanTexture->GetLastError());
        return false;
    }
    
    // Sampler oluştur
    if (!CreateSampler()) {
        return false;
    }
    
    return true;
}

// TextureManager implementasyonu
TextureManager::TextureManager() 
    : m_device(nullptr)
    , m_assetManager(nullptr)
    , m_totalMemoryUsage(0)
    , m_lastCleanupTime(0)
    , m_initialized(false) {
    
    Logger::Debug("TextureManager", "TextureManager created");
}

TextureManager::~TextureManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("TextureManager", "TextureManager destroyed");
}

bool TextureManager::Initialize(VulkanDevice* device, AssetManager* assetManager) {
    if (m_initialized) {
        Logger::Warning("TextureManager", "TextureManager already initialized");
        return true;
    }
    
    if (!device || !assetManager) {
        Logger::Error("TextureManager", "Invalid device or asset manager pointer");
        return false;
    }
    
    m_device = device;
    m_assetManager = assetManager;
    
    Logger::Info("TextureManager", "Initializing TextureManager");
    
    try {
        // Varsayılan texture'ları oluştur
        if (!CreateDefaultTextures()) {
            Logger::Error("TextureManager", "Failed to create default textures");
            return false;
        }
        
        m_initialized = true;
        Logger::Info("TextureManager", "TextureManager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "TextureManager initialization failed: {}", e.what());
        return false;
    }
}

void TextureManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("TextureManager", "Shutting down TextureManager");
    
    // Tüm texture'ları temizle
    m_textures.clear();
    m_defaultWhiteTexture.reset();
    m_defaultBlackTexture.reset();
    m_defaultNormalTexture.reset();
    m_defaultCheckerboardTexture.reset();
    
    m_device = nullptr;
    m_assetManager = nullptr;
    m_totalMemoryUsage = 0;
    m_initialized = false;
    
    Logger::Info("TextureManager", "TextureManager shutdown completed");
}

void TextureManager::Update() {
    if (!m_initialized) {
        return;
    }
    
    // Periyodik temizlik
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (now - m_lastCleanupTime > CLEANUP_INTERVAL) {
        CleanupUnusedTextures();
        m_lastCleanupTime = static_cast<uint64_t>(now);
    }
}

std::shared_ptr<Texture> TextureManager::CreateTexture(const TextureInfo& info) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        auto texture = std::make_shared<Texture>();
        if (!texture->Initialize(m_device, info)) {
            Logger::Error("TextureManager", "Failed to create texture: {}", info.name);
            return nullptr;
        }
        
        // Texture'ı kaydet
        m_textures[info.name] = texture;
        
        // Bellek kullanımını güncelle
        m_totalMemoryUsage += info.memorySize;
        
        Logger::Info("TextureManager", "Texture created: {}", info.name);
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "Exception creating texture: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& texturePath, TextureUsageType usage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Önce cache'te ara
    std::string textureName = texturePath;
    auto it = m_textures.find(textureName);
    if (it != m_textures.end()) {
        return it->second;
    }
    
    try {
        // Texture info oluştur
        TextureInfo info;
        info.name = textureName;
        info.filePath = texturePath;
        info.usage = usage;
        
        // Usage tipine göre varsayılan ayarlar
        switch (usage) {
            case TextureUsageType::Normal:
                info.format = TextureFormat::R8G8B8A8_UNORM;
                info.sRGB = false;
                break;
            case TextureUsageType::Metallic:
            case TextureUsageType::Roughness:
            case TextureUsageType::AO:
            case TextureUsageType::Height:
                info.format = TextureFormat::R8_UNORM;
                info.sRGB = false;
                break;
            default:
                info.format = TextureFormat::R8G8B8A8_SRGB;
                info.sRGB = true;
                break;
        }
        
        // Texture oluştur
        auto texture = std::make_shared<Texture>();
        if (!texture->Initialize(m_device, info)) {
            Logger::Error("TextureManager", "Failed to load texture: {}", texturePath);
            return nullptr;
        }
        
        // Texture'ı kaydet
        m_textures[textureName] = texture;
        
        // Bellek kullanımını güncelle
        UpdateMemoryUsage();
        
        Logger::Info("TextureManager", "Texture loaded: {}", texturePath);
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "Exception loading texture: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Texture> TextureManager::LoadCubemap(const std::vector<std::string>& facePaths) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (facePaths.size() != 6) {
        Logger::Error("TextureManager", "Cubemap requires exactly 6 face paths");
        return nullptr;
    }
    
    // TODO: Cubemap loading implementasyonu
    Logger::Warning("TextureManager", "Cubemap loading not yet implemented");
    return nullptr;
}

std::shared_ptr<Texture> TextureManager::GetTexture(const std::string& textureName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(textureName);
    if (it != m_textures.end()) {
        return it->second;
    }
    
    Logger::Warning("TextureManager", "Texture not found: {}", textureName);
    return nullptr;
}

std::shared_ptr<Texture> TextureManager::CreateCheckerboard(const std::string& name, uint32_t width, uint32_t height,
                                                           uint32_t squareSize, const glm::vec3& color1, const glm::vec3& color2) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        TextureInfo info;
        info.name = name;
        info.width = width;
        info.height = height;
        info.format = TextureFormat::R8G8B8A8_UNORM;
        info.sRGB = false;
        
        auto texture = std::make_shared<Texture>();
        if (!texture->Initialize(m_device, info)) {
            Logger::Error("TextureManager", "Failed to initialize checkerboard texture: {}", name);
            return nullptr;
        }
        
        if (!texture->CreateCheckerboard(width, height, squareSize, color1, color2)) {
            Logger::Error("TextureManager", "Failed to create checkerboard texture: {}", name);
            return nullptr;
        }
        
        m_textures[name] = texture;
        UpdateMemoryUsage();
        
        Logger::Info("TextureManager", "Checkerboard texture created: {}", name);
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "Exception creating checkerboard texture: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Texture> TextureManager::CreateGradient(const std::string& name, uint32_t width, uint32_t height,
                                                       const glm::vec3& startColor, const glm::vec3& endColor, bool horizontal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        TextureInfo info;
        info.name = name;
        info.width = width;
        info.height = height;
        info.format = TextureFormat::R8G8B8A8_UNORM;
        info.sRGB = false;
        
        auto texture = std::make_shared<Texture>();
        if (!texture->Initialize(m_device, info)) {
            Logger::Error("TextureManager", "Failed to initialize gradient texture: {}", name);
            return nullptr;
        }
        
        if (!texture->CreateGradient(width, height, startColor, endColor, horizontal)) {
            Logger::Error("TextureManager", "Failed to create gradient texture: {}", name);
            return nullptr;
        }
        
        m_textures[name] = texture;
        UpdateMemoryUsage();
        
        Logger::Info("TextureManager", "Gradient texture created: {}", name);
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "Exception creating gradient texture: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Texture> TextureManager::CreateNoise(const std::string& name, uint32_t width, uint32_t height,
                                                    float scale, float persistence) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        TextureInfo info;
        info.name = name;
        info.width = width;
        info.height = height;
        info.format = TextureFormat::R8G8B8A8_UNORM;
        info.sRGB = false;
        
        auto texture = std::make_shared<Texture>();
        if (!texture->Initialize(m_device, info)) {
            Logger::Error("TextureManager", "Failed to initialize noise texture: {}", name);
            return nullptr;
        }
        
        if (!texture->CreateNoise(width, height, scale, persistence)) {
            Logger::Error("TextureManager", "Failed to create noise texture: {}", name);
            return nullptr;
        }
        
        m_textures[name] = texture;
        UpdateMemoryUsage();
        
        Logger::Info("TextureManager", "Noise texture created: {}", name);
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("TextureManager", "Exception creating noise texture: {}", e.what());
        return nullptr;
    }
}

void TextureManager::RegisterTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (texture) {
        m_textures[name] = texture;
        UpdateMemoryUsage();
        Logger::Info("TextureManager", "Texture registered: {}", name);
    }
}

void TextureManager::UnregisterTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        m_textures.erase(it);
        UpdateMemoryUsage();
        Logger::Info("TextureManager", "Texture unregistered: {}", name);
    }
}

bool TextureManager::HasTexture(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.find(name) != m_textures.end();
}

void TextureManager::ReloadTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        auto& texture = it->second;
        if (!texture->GetFilePath().empty()) {
            Logger::Info("TextureManager", "Reloading texture: {}", name);
            texture->LoadFromFile(texture->GetFilePath());
        }
    }
}

std::shared_ptr<Texture> TextureManager::CreateTextureAtlas(const std::string& name, 
                                                           const std::vector<std::shared_ptr<Texture>>& textures,
                                                           uint32_t padding) {
    // TODO: Texture atlas implementasyonu
    Logger::Warning("TextureManager", "Texture atlas creation not yet implemented");
    return nullptr;
}

bool TextureManager::ExtractFromAtlas(const std::string& atlasName, const std::string& textureName,
                                     uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    // TODO: Atlas extraction implementasyonu
    Logger::Warning("TextureManager", "Atlas extraction not yet implemented");
    return false;
}

void TextureManager::PreloadTextures(const std::vector<std::string>& texturePaths) {
    Logger::Info("TextureManager", "Preloading {} textures", texturePaths.size());
    
    for (const auto& path : texturePaths) {
        LoadTexture(path);
    }
}

void TextureManager::UnloadUnusedTextures() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto it = m_textures.begin();
    while (it != m_textures.end()) {
        // Varsayılan texture'ları silme
        if (it->second == m_defaultWhiteTexture || 
            it->second == m_defaultBlackTexture || 
            it->second == m_defaultNormalTexture || 
            it->second == m_defaultCheckerboardTexture) {
            ++it;
            continue;
        }
        
        // Sadece manager'da referans olan texture'ları sil
        if (it->second.use_count() == 1 && IsTextureExpired(it->second, static_cast<uint64_t>(now))) {
            Logger::Debug("TextureManager", "Removing unused texture: {}", it->first);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
    
    UpdateMemoryUsage();
}

void TextureManager::OptimizeTextures() {
    Logger::Info("TextureManager", "Optimizing textures");
    
    // TODO: Texture optimization implementasyonu
    // - Texture compression
    // - Mipmap generation
    // - Memory defragmentation
    Logger::Warning("TextureManager", "Texture optimization not yet implemented");
}

size_t TextureManager::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalMemoryUsage;
}

std::vector<std::string> TextureManager::GetLoadedTextureNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_textures.size());
    
    for (const auto& pair : m_textures) {
        names.push_back(pair.first);
    }
    
    return names;
}

void TextureManager::PrintMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::Info("TextureManager", "=== Texture Memory Usage ===");
    Logger::Info("TextureManager", "Total textures: {}", m_textures.size());
    Logger::Info("TextureManager", "Total memory usage: {} MB", m_totalMemoryUsage / (1024 * 1024));
    
    for (const auto& pair : m_textures) {
        const auto& texture = pair.second;
        const auto& info = texture->GetInfo();
        Logger::Info("TextureManager", "  {}: {}x{} ({} KB)", 
                    info.name, info.width, info.height, info.memorySize / 1024);
    }
}

bool TextureManager::CreateDefaultTextures() {
    // Default white texture
    m_defaultWhiteTexture = CreateCheckerboard("DefaultWhite", 64, 64, 1, 
                                              glm::vec3(1.0f), glm::vec3(1.0f));
    if (!m_defaultWhiteTexture) {
        return false;
    }
    
    // Default black texture
    m_defaultBlackTexture = CreateCheckerboard("DefaultBlack", 64, 64, 1, 
                                              glm::vec3(0.0f), glm::vec3(0.0f));
    if (!m_defaultBlackTexture) {
        return false;
    }
    
    // Default normal texture (flat normal)
    m_defaultNormalTexture = CreateGradient("DefaultNormal", 64, 64, 
                                           glm::vec3(0.5f, 0.5f, 1.0f), 
                                           glm::vec3(0.5f, 0.5f, 1.0f));
    if (!m_defaultNormalTexture) {
        return false;
    }
    
    // Default checkerboard texture
    m_defaultCheckerboardTexture = CreateCheckerboard("DefaultCheckerboard", 64, 64, 8, 
                                                     glm::vec3(1.0f), glm::vec3(0.0f));
    if (!m_defaultCheckerboardTexture) {
        return false;
    }
    
    Logger::Info("TextureManager", "Default textures created successfully");
    return true;
}

void TextureManager::CleanupUnusedTextures() {
    UnloadUnusedTextures();
}

void TextureManager::UpdateMemoryUsage() {
    m_totalMemoryUsage = 0;
    
    for (const auto& pair : m_textures) {
        const auto& texture = pair.second;
        const auto& info = texture->GetInfo();
        m_totalMemoryUsage += info.memorySize;
    }
}

std::string TextureManager::GenerateUniqueName(const std::string& baseName) const {
    std::string uniqueName = baseName;
    int counter = 1;
    
    while (m_textures.find(uniqueName) != m_textures.end()) {
        std::ostringstream oss;
        oss << baseName << "_" << counter;
        uniqueName = oss.str();
        counter++;
    }
    
    return uniqueName;
}

bool TextureManager::IsTextureExpired(const std::shared_ptr<Texture>& texture, uint64_t currentTime) const {
    const auto& info = texture->GetInfo();
    
    // 5 dakikadan uzun süredir erişilmemiş texture'ları sil
    const uint64_t EXPIRE_TIME = 5 * 60 * 1000; // 5 dakika
    
    return (currentTime - info.lastAccessTime) > EXPIRE_TIME;
}

// Global texture manager
namespace {
    std::unique_ptr<TextureManager> g_textureManager;
}

TextureManager* GetTextureManager() {
    if (!g_textureManager) {
        g_textureManager = std::make_unique<TextureManager>();
    }
    return g_textureManager.get();
}

} // namespace AstralEngine
