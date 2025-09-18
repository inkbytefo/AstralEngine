#include "VulkanTextureManager.h"
#include "../../Core/Logger.h"
#include "../../Asset/AssetData.h"
#include <vulkan/vulkan.h>

namespace AstralEngine {

VulkanTextureManager::VulkanTextureManager() {
    Logger::Debug("VulkanTextureManager", "VulkanTextureManager created");
}

VulkanTextureManager::~VulkanTextureManager() {
    Logger::Debug("VulkanTextureManager", "VulkanTextureManager destroyed");
}

bool VulkanTextureManager::Initialize(VulkanDevice* device, AssetSubsystem* assetSubsystem) {
    if (!device || !assetSubsystem) {
        SetError("Invalid parameters: device and assetSubsystem must not be null");
        return false;
    }

    m_device = device;
    m_assetSubsystem = assetSubsystem;
    m_initialized = true;

    Logger::Info("VulkanTextureManager", "VulkanTextureManager initialized successfully");
    return true;
}

void VulkanTextureManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("VulkanTextureManager", "Shutting down VulkanTextureManager...");

    // Önbelleği temizle
    ClearCache();

    m_device = nullptr;
    m_assetSubsystem = nullptr;
    m_initialized = false;

    Logger::Info("VulkanTextureManager", "VulkanTextureManager shutdown complete");
}

std::shared_ptr<VulkanTexture> VulkanTextureManager::GetOrCreateTexture(AssetHandle handle) {
    if (!m_initialized) {
        SetError("VulkanTextureManager not initialized");
        return nullptr;
    }

    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Önbellekte kontrol et
    auto it = m_textureCache.find(handle);
    if (it != m_textureCache.end()) {
        auto& entry = it->second;
        
        // Eğer texture hazır ise döndür
        if (entry.state == GpuResourceState::Ready && entry.texture) {
            Logger::Debug("VulkanTextureManager", "Texture found in cache and ready for handle: {}", handle.GetID());
            return entry.texture;
        }
        
        // Eğer texture hala upload ediliyorsa, bu kare için atla
        if (entry.state == GpuResourceState::Uploading) {
            Logger::Trace("VulkanTextureManager", "Texture for handle {} is still uploading. Skipping for this frame.", handle.GetID());
            return nullptr;
        }
        
        // Eğer upload başarısız olduysa, logla ve atla
        if (entry.state == GpuResourceState::Failed) {
            Logger::Warning("VulkanTextureManager", "Texture upload failed for handle: {}. Skipping.", handle.GetID());
            return nullptr;
        }
    }

    // Önbellekte yoksa, AssetSubsystem'den TextureData al
    auto assetManager = m_assetSubsystem->GetAssetManager();
    if (!assetManager) {
        SetError("AssetManager not available from AssetSubsystem");
        return nullptr;
    }

    // TextureData'yı asenkron olarak al
    auto textureData = assetManager->GetAsset<TextureData>(handle);

    // Eğer TextureData henüz hazır değilse (yani hala yükleniyor ise), bu kare için atla
    if (!textureData) {
        // Bu bir hata değil, asset streaming'in beklenen bir davranışıdır.
        // Hata loglaması yerine trace seviyesinde bir log yeterlidir.
        Logger::Trace("VulkanTextureManager", "TextureData for handle {} is not yet loaded (still loading or failed). Skipping texture creation for this frame.", handle.GetID());
        return nullptr;
    }

    // TextureData hazır ama geçersiz mi kontrol et
    if (!textureData->IsValid()) {
        // Bu durum, yükleme tamamlandı ama veri bozuk olduğu için bir hata durumudur.
        SetError("TextureData for handle " + std::to_string(handle.GetID()) + " is loaded but invalid.");
        Logger::Warning("VulkanTextureManager", "TextureData for handle {} is invalid.", handle.GetID());
        return nullptr;
    }

    // Yeni texture oluştur
    auto texture = CreateTextureFromData(textureData, handle);
    if (!texture) {
        // CreateTextureFromData zaten kendi hatasını loglar ve ayarlar.
        // Burada ek bir hata ayarlamaya gerek yok, sadece null döndürmek yeterli.
        Logger::Error("VulkanTextureManager", "Failed to create texture from TextureData for handle: {}", handle.GetID());
        return nullptr;
    }

    // Yeni cache entry oluştur ve Uploading olarak işaretle (asenkron yükleme için)
    TextureCacheEntry entry;
    entry.texture = texture;
    entry.state = GpuResourceState::Uploading;
    entry.needsCompletion = true;  // CompleteImageInitialization çağrılması gerekiyor
    
    // Önbelleğe ekle
    m_textureCache[handle] = entry;
    Logger::Info("VulkanTextureManager", "Created texture and marked as uploading for handle: {} (width: {}, height: {}, channels: {})",
                 handle.GetID(), textureData->width, textureData->height, textureData->channels);

    // Texture hala yükleniyor olduğu için nullptr döndür
    // CheckUploadCompletions() çağrıldığında hazır olacak
    return nullptr;
}

std::shared_ptr<VulkanTexture> VulkanTextureManager::GetTexture(AssetHandle handle) {
    if (!m_initialized) {
        SetError("VulkanTextureManager not initialized");
        return nullptr;
    }

    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Önbellekte kontrol et
    auto it = m_textureCache.find(handle);
    if (it != m_textureCache.end()) {
        auto& entry = it->second;
        
        // Eğer texture hazır ise döndür
        if (entry.state == GpuResourceState::Ready && entry.texture) {
            Logger::Debug("VulkanTextureManager", "Texture found in cache and ready for handle: {}", handle.GetID());
            return entry.texture;
        }
        
        // Eğer texture hazır değilse, nullptr döndür
        Logger::Trace("VulkanTextureManager", "Texture for handle {} is not ready (state: {}). Returning nullptr.",
                     handle.GetID(), static_cast<int>(entry.state));
        return nullptr;
    }

    // Önbellekte yoksa, nullptr döndür
    Logger::Trace("VulkanTextureManager", "Texture for handle {} not found in cache. Returning nullptr.", handle.GetID());
    return nullptr;
}

void VulkanTextureManager::RemoveTexture(AssetHandle handle) {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_textureCache.find(handle);
    if (it != m_textureCache.end()) {
        Logger::Debug("VulkanTextureManager", "Removing texture from cache for handle: {}", handle.GetID());
        m_textureCache.erase(it);
    }
}

void VulkanTextureManager::ClearCache() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    Logger::Info("VulkanTextureManager", "Clearing texture cache ({} textures)", m_textureCache.size());
    
    // Tüm texture'ları ve staging buffer'ları temizle
    for (auto& pair : m_textureCache) {
        auto& entry = pair.second;
        
        // Texture'ı shutdown et
        if (entry.texture) {
            entry.texture->Shutdown();
        }
        
        // Staging buffer'ları temizle
        if (entry.stagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->GetDevice(), entry.stagingBuffer, nullptr);
        }
        if (entry.stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), entry.stagingMemory, nullptr);
        }
        
        // Fence'i temizle
        if (entry.uploadFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device->GetDevice(), entry.uploadFence, nullptr);
        }
    }

    m_textureCache.clear();
}

bool VulkanTextureManager::HasTexture(AssetHandle handle) const {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_textureCache.find(handle) != m_textureCache.end();
}

std::vector<AssetHandle> VulkanTextureManager::GetCachedHandles() const {
    std::vector<AssetHandle> handles;
    
    if (!m_initialized) {
        return handles;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    handles.reserve(m_textureCache.size());
    for (const auto& pair : m_textureCache) {
        handles.push_back(pair.first);
    }
    
    return handles;
}

std::shared_ptr<VulkanTexture> VulkanTextureManager::CreateTextureFromData(const std::shared_ptr<TextureData>& textureData, AssetHandle handle) {
    if (!textureData || !textureData->IsValid()) {
        SetError("Invalid TextureData provided");
        return nullptr;
    }

    // Yeni VulkanTexture oluştur
    auto texture = std::make_shared<VulkanTexture>();
    if (!texture) {
        SetError("Failed to allocate VulkanTexture");
        return nullptr;
    }

    // Vulkan formatını belirle
    VkFormat format = DetermineVkFormat(textureData);

    // Texture'ı veriden başlat
    if (!texture->InitializeFromData(m_device, textureData->data, textureData->width, textureData->height, format)) {
        SetError("Failed to initialize VulkanTexture from data: " + texture->GetLastError());
        return nullptr;
    }

    Logger::Debug("VulkanTextureManager", "Successfully created texture from TextureData for handle: {}", handle.GetID());
    return texture;
}

VkFormat VulkanTextureManager::DetermineVkFormat(const std::shared_ptr<TextureData>& textureData) const {
    if (!textureData || !textureData->IsValid()) {
        return VK_FORMAT_R8G8B8A8_UNORM; // Default format
    }

    // Kanal sayısına göre format belirle
    switch (textureData->channels) {
        case 1:
            return VK_FORMAT_R8_UNORM;
        case 2:
            return VK_FORMAT_R8G8_UNORM;
        case 3:
            return VK_FORMAT_R8G8B8_UNORM;
        case 4:
        default:
            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

void VulkanTextureManager::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanTextureManager", "{}", error);
}

void VulkanTextureManager::CheckUploadCompletions() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    // Tüm "Uploading" durumundaki texture'ları iterate et
    for (auto& pair : m_textureCache) {
        auto& entry = pair.second;
        
        if (entry.state == GpuResourceState::Uploading && entry.texture) {
            // Texture'ın hazır olup olmadığını kontrol et
            if (entry.texture->IsReady()) {
                // Hazır olan texture'lar için CompleteImageInitialization çağır
                if (entry.needsCompletion) {
                    entry.texture->CompleteImageInitialization();
                    entry.needsCompletion = false;
                    
                    // CompleteImageInitialization çağrıldıktan sonra texture durumunu kontrol et
                    if (entry.texture->GetState() == GpuResourceState::Ready) {
                        entry.state = GpuResourceState::Ready;
                        Logger::Info("VulkanTextureManager", "Texture upload completed successfully for handle: {}", pair.first.GetID());
                    } else if (entry.texture->GetState() == GpuResourceState::Failed) {
                        entry.state = GpuResourceState::Failed;
                        Logger::Error("VulkanTextureManager", "Texture upload failed for handle: {}", pair.first.GetID());
                    }
                    // Eğer hala Uploading durumundaysa, bir sonraki framede tekrar kontrol edilecek
                } else {
                    // needsCompletion false ise, doğrudan Ready olarak işaretle
                    entry.state = GpuResourceState::Ready;
                    Logger::Info("VulkanTextureManager", "Texture marked as ready for handle: {}", pair.first.GetID());
                }
            } else {
                // Texture'ın kendi durumunu kontrol et
                GpuResourceState textureState = entry.texture->GetState();
                if (textureState == GpuResourceState::Failed) {
                    entry.state = GpuResourceState::Failed;
                    entry.needsCompletion = false;
                    Logger::Error("VulkanTextureManager", "Texture upload failed for handle: {}", pair.first.GetID());
                }
                // Hala Uploading durumundaysa, bir sonraki framede tekrar kontrol edilecek
            }
        }
    }
}

GpuResourceState VulkanTextureManager::GetTextureState(AssetHandle handle) const {
    if (!m_initialized) {
        return GpuResourceState::Unloaded;
    }

    if (!handle.IsValid()) {
        return GpuResourceState::Unloaded;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_textureCache.find(handle);
    if (it != m_textureCache.end()) {
        return it->second.state;
    }

    return GpuResourceState::Unloaded;
}

bool VulkanTextureManager::IsTextureReady(AssetHandle handle) const {
    return GetTextureState(handle) == GpuResourceState::Ready;
}

void VulkanTextureManager::CleanupTextureResources(AssetHandle handle) {
    if (!m_initialized) {
        return;
    }

    if (!handle.IsValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_textureCache.find(handle);
    if (it != m_textureCache.end()) {
        auto& entry = it->second;
        
        Logger::Debug("VulkanTextureManager", "Cleaning up texture resources for handle: {}", handle.GetID());
        
        // Texture'ı shutdown et
        if (entry.texture) {
            entry.texture->Shutdown();
            entry.texture.reset();
        }
        
        // Staging buffer'ları temizle
        if (entry.stagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->GetDevice(), entry.stagingBuffer, nullptr);
            entry.stagingBuffer = VK_NULL_HANDLE;
        }
        if (entry.stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), entry.stagingMemory, nullptr);
            entry.stagingMemory = VK_NULL_HANDLE;
        }
        
        // Fence'i temizle
        if (entry.uploadFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device->GetDevice(), entry.uploadFence, nullptr);
            entry.uploadFence = VK_NULL_HANDLE;
        }
        
        // Durumu güncelle
        entry.state = GpuResourceState::Unloaded;
        entry.needsCompletion = false;
        
        Logger::Info("VulkanTextureManager", "Texture resources cleaned up for handle: {}", handle.GetID());
    }
}

} // namespace AstralEngine
