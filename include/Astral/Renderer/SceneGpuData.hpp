#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <span>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Astral/Renderer/ShaderInterop.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/BrickGrid.hpp"

namespace Astral {

class VulkanContext;
class SDFChangeSet;

/// Descriptor ve izgara guncellemeleri icin sahne tamponlarinin ozet gorunumleri
struct SceneBufferViews {
    vk::DescriptorBufferInfo primitives{};
    vk::DescriptorBufferInfo previousTransforms{};
    vk::DescriptorBufferInfo lights{};
    vk::DescriptorBufferInfo grid{};
    uint32_t primitiveCount = 0;
    glm::vec4 gridParams{0.0f};
};

/// Primitifler, onceki dunya donusumleri, isiklar, BrickGrid ve Kamera UBO tamponlarinin
/// GPU yukleme ve sahiplik sorumlulugunu ustlenen bilesen.
class SceneGpuData {
public:
    static constexpr size_t MAX_LIGHTS = 64;

    explicit SceneGpuData(VulkanContext& context, bool persistentMap = true);
    ~SceneGpuData();

    SceneGpuData(const SceneGpuData&) = delete;
    SceneGpuData& operator=(const SceneGpuData&) = delete;
    SceneGpuData(SceneGpuData&&) noexcept;
    SceneGpuData& operator=(SceneGpuData&&) noexcept;

    /// Sahne primitiflerini, onceki donusum gecmisini ve BrickGrid yapisini gunceller
    void Upload(std::span<const SDFPrimitiveRecord> records, const SDFChangeSet& changeSet, bool legacyMap = false);

    /// Sahne tamponlarinin anlik descriptor bilgilerini ve izgara parametrelerini dondurur
    [[nodiscard]] SceneBufferViews GetViews() const;

    /// Isik verilerini gunceller ve GPU tamponuna aktarir
    void SetLights(std::span<const LightGPU> lights);

    /// Onceden hesaplanmis kamera UBO verisini GPU tamponuna aktarir
    void UploadCamera(const CameraUBOData& cameraData);

    /// Kamera UBO descriptor bilgisini dondurur
    [[nodiscard]] vk::DescriptorBufferInfo GetCameraDescriptor() const;

    /// Donusum gecmisi haritasini temizler
    void ResetTransformHistory() noexcept;

    /// G11: Basarili gonderim sonrasinda aday donusum gecmisini committed duruma gecirir
    void CommitSubmitted() noexcept;

    /// G11: Gonderim iptal edilirse aday donusum gecmisini temizler
    void AbortPrepared() noexcept;

    /// Kimligi (surfaceId == 0) olmayan primitifler icin indeks tabanli benzersiz anahtar uretir
    static constexpr uint32_t MakeFallbackTransformKey(uint32_t index) noexcept {
        return 0x80000000u | index;
    }

    // Geriye donuk uyumluluk getter'lari
    [[nodiscard]] Buffer* GetEditBuffer() const noexcept { return m_EditBuffer.get(); }
    [[nodiscard]] Buffer* GetPrevTransformBuffer() const noexcept { return m_PrevTransformBuffer.get(); }
    [[nodiscard]] Buffer* GetLightBuffer() const noexcept { return m_LightBuffer.get(); }
    [[nodiscard]] Buffer* GetCameraUBO() const noexcept { return m_CameraUBO.get(); }
    [[nodiscard]] BrickGrid* GetBrickGrid() const noexcept { return m_BrickGrid.get(); }
    [[nodiscard]] uint32_t GetActiveEditCount() const noexcept { return m_ActiveEditCount; }
    [[nodiscard]] const std::vector<LightGPU>& GetLights() const noexcept { return m_Lights; }
    [[nodiscard]] std::vector<LightGPU>& GetLights() noexcept { return m_Lights; }

    /// G13: Son karede GPU'ya yuklenen primitif ve donusum bayt miktarlari
    [[nodiscard]] size_t GetLastPrimitiveUploadBytes() const noexcept { return m_LastPrimitiveUploadBytes; }
    [[nodiscard]] size_t GetLastTransformUploadBytes() const noexcept { return m_LastTransformUploadBytes; }
    [[nodiscard]] size_t GetLastTotalUploadBytes() const noexcept { return m_LastPrimitiveUploadBytes + m_LastTransformUploadBytes; }

private:
    VulkanContext* m_Context = nullptr;
    std::unique_ptr<Buffer> m_EditBuffer;
    std::unique_ptr<Buffer> m_PrevTransformBuffer;
    std::unique_ptr<Buffer> m_LightBuffer;
    std::unique_ptr<Buffer> m_CameraUBO;
    std::unique_ptr<BrickGrid> m_BrickGrid;

    std::vector<LightGPU> m_Lights;
    std::unordered_map<uint32_t, glm::mat4> m_PrevWorldTransforms;
    std::unordered_map<uint32_t, glm::mat4> m_CandidateWorldTransforms;
    uint32_t m_ActiveEditCount = 0;

    // G13: Calisma tamponlari ve yuklenen kayit durum onbellegi (Kapasiteyi yeniden kullanir)
    std::vector<glm::mat4> m_PrevMatricesWorkBuffer;
    std::vector<SDFPrimitiveRecord> m_UploadedRecords;
    std::vector<glm::mat4> m_UploadedPrevMatrices;
    bool m_IsInitialized = false;

    size_t m_LastPrimitiveUploadBytes = 0;
    size_t m_LastTransformUploadBytes = 0;

    void UpdateLights();
};

} // namespace Astral
