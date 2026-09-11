#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <memory>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"

namespace Astral {

enum class PickingState {
    Idle = 0,
    Requested,
    Dispatched,
    Completed,
    Consumed
};

struct PickRequest {
    uint64_t requestId = 0;
    uint64_t sceneInstance = 0;
    int x = -1;
    int y = -1;
};

struct CompletedPick {
    uint64_t requestId = 0;
    uint64_t sceneInstance = 0;
    uint64_t frameSerial = 0;
    SelectionDataGPU data{};
};

class PickingReadback {
public:
    PickingReadback();
    explicit PickingReadback(VulkanContext& context);
    ~PickingReadback() = default;

    PickingReadback(const PickingReadback&) = delete;
    PickingReadback& operator=(const PickingReadback&) = delete;
    PickingReadback(PickingReadback&&) noexcept = default;
    PickingReadback& operator=(PickingReadback&&) noexcept = default;

    /// Monoton artan requestId ile yeni secim istegi olusturur.
    uint64_t RequestPick(int x, int y, uint64_t sceneInstance = 0);

    /// GBuffer dispatch oncesi push constant parametrelerini hazirlar.
    /// Kamera yoksa veya koordinat sinir disindaysa false doner ve bos completed sonuc uretir.
    bool PrepareDispatch(uint32_t renderWidth, uint32_t renderHeight, bool hasCamera,
                         int& outMouseX, int& outMouseY, float& outPickingFlag);

    /// Compute Shader -> Host okuma bariyerini komut tamponuna yazar ve durumu Dispatched yapar.
    void RecordBarrier(vk::CommandBuffer cmd);

    /// GPU komut tamponu submit edildiginde cagirilir; karenin seri numarasini baglar.
    void OnFrameSubmitted(uint64_t frameSerial);

    /// G11: Kaydedilip submit edilmeyen karede aday secim durumunu geri alir
    void AbortPrepared() noexcept;

    /// Fence tamamlandiginda cagirilir; host okumasini yapar, CompletedPick hazirlar ve durumu Completed yapar.
    void OnFrameCompleted(uint64_t completedSerial);

    /// Tamamlanmis secim sonucunu tek seferlik tuketir.
    /// Sonuc yoksa veya zaten tuketilmisse std::nullopt dondurur.
    std::optional<CompletedPick> ConsumeCompleted();

    /// Tamamlanmis sonucu tuketmeden sadece okur (varsa).
    [[nodiscard]] std::optional<CompletedPick> PeekCompleted() const;

    /// Sahne degistiginde bekleyen veya tamamlanan eski sonuclari gecersiz kilar.
    void InvalidateScene(uint64_t currentSceneInstance);

    /// Tamponu -1 ile sifirlar ve durumu Idle yapar.
    void Clear();

    [[nodiscard]] PickingState GetState() const noexcept { return m_State; }
    [[nodiscard]] bool HasPendingRead() const noexcept {
        return m_State == PickingState::Dispatched || m_State == PickingState::Completed;
    }
    [[nodiscard]] bool HasCompletedResult() const noexcept { return m_State == PickingState::Completed; }
    [[nodiscard]] const std::optional<PickRequest>& GetActiveRequest() const noexcept { return m_ActiveRequest; }
    [[nodiscard]] uint64_t GetLastRequestId() const noexcept { return m_RequestCounter; }

    [[nodiscard]] vk::DescriptorBufferInfo GetDescriptorInfo() const;
    [[nodiscard]] vk::Buffer GetBuffer() const;
    [[nodiscard]] Buffer* GetSelectionBuffer() const noexcept { return m_SelectionBuffer.get(); }

    /// Test amaciyla CPU simule veri yazma (GPU olmadan testler icin)
    void SetSimulatedDataForTesting(const SelectionDataGPU& data);

private:
    std::unique_ptr<Buffer> m_SelectionBuffer;
    SelectionDataGPU m_CpuFallbackData{};

    PickingState m_State = PickingState::Idle;
    uint64_t m_RequestCounter = 0;

    std::optional<PickRequest> m_ActiveRequest;
    uint64_t m_DispatchedSerial = 0;
    std::optional<CompletedPick> m_CompletedPick;

    SelectionDataGPU* GetMappedDataPtr();
    const SelectionDataGPU* GetMappedDataPtr() const;
};

} // namespace Astral
