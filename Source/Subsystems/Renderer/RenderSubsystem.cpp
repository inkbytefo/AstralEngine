#include "RenderSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Platform/PlatformSubsystem.h"
#include "../Platform/Window.h"
#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanCommandManager.h"

namespace AstralEngine {

const int MAX_FRAMES_IN_FLIGHT = 2;

RenderSubsystem::RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem created");
}

RenderSubsystem::~RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem destroyed");
}

void RenderSubsystem::OnInitialize(Engine* owner) {
    if (m_isInitialized) return;

    m_owner = owner;
    Logger::Info("RenderSubsystem", "Initializing render subsystem...");

    auto* platform = m_owner->GetSubsystem<PlatformSubsystem>();
    if (!platform) {
        Logger::Critical("RenderSubsystem", "PlatformSubsystem not found!");
        return;
    }

    m_window = platform->GetWindow();
    if (!m_window) {
        Logger::Critical("RenderSubsystem", "Window not found!");
        return;
    }

    m_graphicsDevice = std::make_unique<GraphicsDevice>();
    m_graphicsDevice->Initialize(m_window);

    CreateSyncObjects();

    m_isInitialized = true;
    Logger::Info("RenderSubsystem", "Render subsystem initialized successfully");
}

void RenderSubsystem::OnUpdate(float) {
    if (!m_isInitialized) return;

    uint32_t imageIndex = BeginFrame();

    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCommandManager()->GetCurrentCommandBuffer();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_graphicsDevice->GetRenderPass();
    renderPassInfo.framebuffer = m_graphicsDevice->GetSwapchain()->GetFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
    VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsDevice->GetPipeline()->GetPipeline());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    EndFrame(imageIndex);
}

void RenderSubsystem::OnShutdown() {
    if (!m_isInitialized) return;

    Logger::Info("RenderSubsystem", "Shutting down render subsystem...");

    VkDevice device = m_graphicsDevice->GetDevice();
    vkDeviceWaitIdle(device);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, m_inFlightFences[i], nullptr);
    }

    if (m_graphicsDevice) {
        m_graphicsDevice->Shutdown();
    }

    m_isInitialized = false;
    Logger::Info("RenderSubsystem", "Render subsystem shutdown complete");
}

void RenderSubsystem::CreateSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_graphicsDevice->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_graphicsDevice->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_graphicsDevice->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}


uint32_t RenderSubsystem::BeginFrame() {
    vkWaitForFences(m_graphicsDevice->GetDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_graphicsDevice->GetDevice(), 1, &m_inFlightFences[m_currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_graphicsDevice->GetDevice(), m_graphicsDevice->GetSwapchain()->GetSwapchain(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCommandManager()->GetCurrentCommandBuffer();
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    return imageIndex;
}

void RenderSubsystem::EndFrame(uint32_t imageIndex) {
    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCommandManager()->GetCurrentCommandBuffer();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_graphicsDevice->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_graphicsDevice->GetSwapchain()->GetSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(m_graphicsDevice->GetPresentQueue(), &presentInfo);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

}
