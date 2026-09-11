#include "Astral/Renderer/PickingReadback.hpp"
#include <cstring>

namespace Astral {

PickingReadback::PickingReadback() {
    m_CpuFallbackData.hitIndex = -1;
    m_CpuFallbackData.hitPoint = glm::vec4(0.0f);
}

PickingReadback::PickingReadback(VulkanContext& context)
    : PickingReadback() {
    m_SelectionBuffer = std::make_unique<Buffer>(
        context.GetAllocator(),
        context.GetDevice(),
        context.GetPhysicalDevice(),
        sizeof(SelectionDataGPU),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );
    Clear();
}

SelectionDataGPU* PickingReadback::GetMappedDataPtr() {
    if (m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        return static_cast<SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
    }
    return &m_CpuFallbackData;
}

const SelectionDataGPU* PickingReadback::GetMappedDataPtr() const {
    if (m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        return static_cast<const SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
    }
    return &m_CpuFallbackData;
}

uint64_t PickingReadback::RequestPick(int x, int y, uint64_t sceneInstance) {
    m_RequestCounter++;
    m_ActiveRequest = PickRequest{
        .requestId = m_RequestCounter,
        .sceneInstance = sceneInstance,
        .x = x,
        .y = y
    };
    m_CompletedPick = std::nullopt;
    m_State = PickingState::Requested;
    return m_RequestCounter;
}

bool PickingReadback::PrepareDispatch(uint32_t renderWidth, uint32_t renderHeight, bool hasCamera,
                                     int& outMouseX, int& outMouseY, float& outPickingFlag) {
    if (m_State != PickingState::Requested || !m_ActiveRequest.has_value()) {
        outMouseX = -1;
        outMouseY = -1;
        outPickingFlag = 0.0f;
        return false;
    }

    const auto& req = *m_ActiveRequest;

    // Kamera yoksa veya piksel sinir disindaysa
    if (!hasCamera || req.x < 0 || req.y < 0 ||
        (renderWidth > 0 && static_cast<uint32_t>(req.x) >= renderWidth) ||
        (renderHeight > 0 && static_cast<uint32_t>(req.y) >= renderHeight)) {
        
        CompletedPick cp{};
        cp.requestId = req.requestId;
        cp.sceneInstance = req.sceneInstance;
        cp.frameSerial = 0;
        cp.data.hitIndex = -1;
        cp.data.hitPoint = glm::vec4(0.0f);

        if (auto* ptr = GetMappedDataPtr()) {
            ptr->hitIndex = -1;
            ptr->hitPoint = glm::vec4(0.0f);
        }

        m_CompletedPick = cp;
        m_State = PickingState::Completed;
        m_ActiveRequest = std::nullopt;

        outMouseX = -1;
        outMouseY = -1;
        outPickingFlag = 0.0f;
        return false;
    }

    // Gecerli istek: tamponu hazirla (-1 yaz)
    if (auto* ptr = GetMappedDataPtr()) {
        ptr->hitIndex = -1;
        ptr->hitPoint = glm::vec4(0.0f);
    }

    outMouseX = req.x;
    outMouseY = req.y;
    outPickingFlag = 1.0f;
    return true;
}

void PickingReadback::RecordBarrier(vk::CommandBuffer cmd) {
    if (m_State != PickingState::Requested || !m_ActiveRequest.has_value()) {
        return;
    }

    if (m_SelectionBuffer && cmd) {
        vk::BufferMemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = m_SelectionBuffer->GetBuffer();
        barrier.offset = 0;
        barrier.size = sizeof(SelectionDataGPU);

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eHost,
            {},
            0, nullptr,
            1, &barrier,
            0, nullptr
        );
    }

    m_State = PickingState::Dispatched;
}

void PickingReadback::OnFrameSubmitted(uint64_t frameSerial) {
    if (m_State == PickingState::Dispatched) {
        m_DispatchedSerial = frameSerial;
    }
}

void PickingReadback::AbortPrepared() noexcept {
    if (m_State == PickingState::Dispatched) {
        m_State = m_ActiveRequest.has_value() ? PickingState::Requested : PickingState::Idle;
    }
}

void PickingReadback::OnFrameCompleted(uint64_t completedSerial) {
    if (m_State == PickingState::Dispatched && m_ActiveRequest.has_value()) {
        const auto* ptr = GetMappedDataPtr();
        SelectionDataGPU data = ptr ? *ptr : SelectionDataGPU{};

        CompletedPick cp{};
        cp.requestId = m_ActiveRequest->requestId;
        cp.sceneInstance = m_ActiveRequest->sceneInstance;
        cp.frameSerial = completedSerial;
        cp.data = data;

        m_CompletedPick = cp;
        m_State = PickingState::Completed;
        m_ActiveRequest = std::nullopt;
    }
}

std::optional<CompletedPick> PickingReadback::ConsumeCompleted() {
    // Senkron yolda henuz OnFrameCompleted cagrilmadiysa otomatik tamamla
    if (m_State == PickingState::Dispatched && m_ActiveRequest.has_value()) {
        OnFrameCompleted(m_DispatchedSerial);
    }

    if (m_State != PickingState::Completed || !m_CompletedPick.has_value()) {
        return std::nullopt;
    }

    CompletedPick result = *m_CompletedPick;
    m_CompletedPick = std::nullopt;
    m_State = PickingState::Consumed;

    // Tamponu bir sonraki okumada -1 kalacak sekilde temizle
    if (auto* ptr = GetMappedDataPtr()) {
        ptr->hitIndex = -1;
    }

    return result;
}

std::optional<CompletedPick> PickingReadback::PeekCompleted() const {
    if (m_State == PickingState::Completed && m_CompletedPick.has_value()) {
        return m_CompletedPick;
    }
    return std::nullopt;
}

void PickingReadback::InvalidateScene(uint64_t currentSceneInstance) {
    if (m_ActiveRequest.has_value() && m_ActiveRequest->sceneInstance != currentSceneInstance) {
        m_ActiveRequest = std::nullopt;
        m_State = PickingState::Idle;
    }
    if (m_CompletedPick.has_value() && m_CompletedPick->sceneInstance != currentSceneInstance) {
        m_CompletedPick = std::nullopt;
        m_State = PickingState::Idle;
    }
}

void PickingReadback::Clear() {
    m_State = PickingState::Idle;
    m_ActiveRequest = std::nullopt;
    m_CompletedPick = std::nullopt;
    if (auto* ptr = GetMappedDataPtr()) {
        ptr->hitIndex = -1;
        ptr->hitPoint = glm::vec4(0.0f);
    }
}

vk::DescriptorBufferInfo PickingReadback::GetDescriptorInfo() const {
    if (m_SelectionBuffer) {
        return m_SelectionBuffer->GetDescriptorInfo();
    }
    return vk::DescriptorBufferInfo{};
}

vk::Buffer PickingReadback::GetBuffer() const {
    if (m_SelectionBuffer) {
        return m_SelectionBuffer->GetBuffer();
    }
    return vk::Buffer{};
}

void PickingReadback::SetSimulatedDataForTesting(const SelectionDataGPU& data) {
    if (auto* ptr = GetMappedDataPtr()) {
        *ptr = data;
    }
}

} // namespace Astral
