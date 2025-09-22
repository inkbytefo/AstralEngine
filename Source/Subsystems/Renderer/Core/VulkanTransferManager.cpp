#include "VulkanTransferManager.h"
#include "Core/Logger.h"

namespace AstralEngine {

	VulkanTransferManager::VulkanTransferManager(VulkanDevice* device)
		: m_device(device) {
		AE_CORE_ASSERT(device, "VulkanDevice is null");
	}

	VulkanTransferManager::~VulkanTransferManager() {
		// Shutdown is called by the owner (GraphicsDevice) to ensure proper order
	}

	void VulkanTransferManager::Initialize() {
		if (!m_device || !m_device->GetDevice()) {
			AE_CORE_ERROR("Device not initialized. Cannot initialize VulkanTransferManager.");
			return;
		}

		VkDevice logicalDevice = m_device->GetDevice();
		const auto& queueFamilyIndices = m_device->GetQueueFamilyIndices();

		if (!queueFamilyIndices.transferFamily.has_value()) {
			AE_CORE_ERROR("Transfer queue family not found. Cannot initialize VulkanTransferManager.");
			return;
		}

		// Create transfer command pool
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value();
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &m_transferCommandPool) != VK_SUCCESS) {
			AE_CORE_CRITICAL("Failed to create transfer command pool.");
			return;
		}

		// Allocate command buffer
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_transferCommandPool;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &m_transferCommandBuffer) != VK_SUCCESS) {
			AE_CORE_CRITICAL("Failed to allocate transfer command buffer.");
			vkDestroyCommandPool(logicalDevice, m_transferCommandPool, nullptr);
			m_transferCommandPool = VK_NULL_HANDLE;
			return;
		}

		if (!RecreateSignaledFence_()) {
			AE_CORE_CRITICAL("Failed to create initial transfer fence.");
			vkDestroyCommandPool(logicalDevice, m_transferCommandPool, nullptr);
			m_transferCommandPool = VK_NULL_HANDLE;
			m_transferCommandBuffer = VK_NULL_HANDLE;
			return;
		}

		AE_CORE_INFO("VulkanTransferManager initialized successfully.");
	}

	void VulkanTransferManager::Shutdown() {
		if (!m_device || !m_device->GetDevice() || m_transferCommandPool == VK_NULL_HANDLE) {
			return;
		}
		AE_CORE_INFO("Shutting down VulkanTransferManager...");

		VkDevice logicalDevice = m_device->GetDevice();

		vkDeviceWaitIdle(logicalDevice);

		if (m_transferFence != VK_NULL_HANDLE) {
			vkDestroyFence(logicalDevice, m_transferFence, nullptr);
			m_transferFence = VK_NULL_HANDLE;
		}

		if (m_transferCommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(logicalDevice, m_transferCommandPool, nullptr);
			m_transferCommandPool = VK_NULL_HANDLE;
		}
		m_transferCommandBuffer = VK_NULL_HANDLE;


		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_transferQueue.clear();

		AE_CORE_INFO("VulkanTransferManager shutdown complete.");
	}

	void VulkanTransferManager::QueueTransfer(std::function<void(VkCommandBuffer)>&& transferFunction) {
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_transferQueue.emplace_back(std::move(transferFunction));
		AE_CORE_TRACE("Queued a new transfer operation. Queue size: {0}", m_transferQueue.size());
	}

	void VulkanTransferManager::SubmitTransfers() {
		std::lock_guard<std::mutex> lock(m_submitMutex);

		std::vector<std::function<void(VkCommandBuffer)>> localQueue;
		{
			std::lock_guard<std::mutex> queueLock(m_queueMutex);
			if (m_transferQueue.empty()) {
				return;
			}
			localQueue.swap(m_transferQueue);
		}

		size_t transferCount = localQueue.size();
		AE_CORE_TRACE("Submitting {0} queued transfers...", transferCount);

		VkDevice logicalDevice = m_device->GetDevice();

		vkWaitForFences(logicalDevice, 1, &m_transferFence, VK_TRUE, UINT64_MAX);

		if (vkResetCommandBuffer(m_transferCommandBuffer, 0) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to reset transfer command buffer.");
			return;
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(m_transferCommandBuffer, &beginInfo) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to begin recording transfer command buffer.");
			return;
		}

		for (const auto& func : localQueue) {
			func(m_transferCommandBuffer);
		}

		if (vkEndCommandBuffer(m_transferCommandBuffer) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to end recording transfer command buffer.");
			return;
		}

		if (vkResetFences(logicalDevice, 1, &m_transferFence) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to reset transfer fence.");
			return;
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_transferCommandBuffer;

		if (vkQueueSubmit(m_device->GetTransferQueue(), 1, &submitInfo, m_transferFence) != VK_SUCCESS) {
			AE_CORE_ERROR("vkQueueSubmit failed for batched transfers. Attempting recovery...");
			IdleTransferQueue_();
			if (!RecreateSignaledFence_()) {
				AE_CORE_CRITICAL("Fence recovery failed. Transfer manager is in an unrecoverable state.");
				return;
			}
			{
				std::lock_guard<std::mutex> ql(m_queueMutex);
				m_transferQueue.insert(m_transferQueue.begin(),
					std::make_move_iterator(localQueue.begin()),
					std::make_move_iterator(localQueue.end()));
			}
			AE_CORE_WARN("Recovered from submit failure. {0} transfers were re-queued.", transferCount);
			return;
		}

		vkWaitForFences(logicalDevice, 1, &m_transferFence, VK_TRUE, UINT64_MAX);

		// Execute cleanup callbacks after fence signals (GPU completion)
		if (!m_cleanupCallbacks.empty()) {
			AE_CORE_TRACE("Executing {0} cleanup callbacks after transfer completion.", m_cleanupCallbacks.size());
			for (const auto& callback : m_cleanupCallbacks) {
				callback();
			}
			m_cleanupCallbacks.clear();
		}

		AE_CORE_INFO("Successfully submitted and completed {0} transfer operations.", transferCount);
	}

	VkCommandBuffer VulkanTransferManager::GetCommandBufferForImmediateUse() {
		AE_CORE_TRACE("Acquiring command buffer for immediate use...");
		m_submitMutex.lock();

		VkDevice logicalDevice = m_device->GetDevice();

		vkWaitForFences(logicalDevice, 1, &m_transferFence, VK_TRUE, UINT64_MAX);

		if (vkResetCommandBuffer(m_transferCommandBuffer, 0) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to reset immediate command buffer.");
			m_submitMutex.unlock();
			return VK_NULL_HANDLE;
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(m_transferCommandBuffer, &beginInfo) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to begin recording immediate command buffer.");
			m_submitMutex.unlock();
			return VK_NULL_HANDLE;
		}

		AE_CORE_TRACE("Command buffer ready for immediate use. Mutex is locked.");
		return m_transferCommandBuffer;
	}

	void VulkanTransferManager::SubmitImmediateCommand(VkCommandBuffer commandBuffer) {
		AE_CORE_ASSERT(commandBuffer == m_transferCommandBuffer, "Invalid command buffer submitted for immediate execution.");
		AE_CORE_TRACE("Submitting immediate command...");

		VkDevice logicalDevice = m_device->GetDevice();

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to end recording immediate command buffer.");
			m_submitMutex.unlock();
			return;
		}

		if (vkResetFences(logicalDevice, 1, &m_transferFence) != VK_SUCCESS) {
			AE_CORE_ERROR("Failed to reset immediate fence.");
			m_submitMutex.unlock();
			return;
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		if (vkQueueSubmit(m_device->GetTransferQueue(), 1, &submitInfo, m_transferFence) != VK_SUCCESS) {
			AE_CORE_ERROR("vkQueueSubmit failed for immediate command. Attempting recovery...");
			IdleTransferQueue_();
			if (!RecreateSignaledFence_()) {
				AE_CORE_CRITICAL("Fence recovery failed. Transfer manager is in an unrecoverable state.");
			} else {
				(void)vkResetCommandBuffer(m_transferCommandBuffer, 0);
			}
			m_submitMutex.unlock();
			return;
		}

		vkWaitForFences(logicalDevice, 1, &m_transferFence, VK_TRUE, UINT64_MAX);

		AE_CORE_INFO("Immediate command submitted and completed successfully.");
		m_submitMutex.unlock();
	}

	void VulkanTransferManager::RegisterCleanupCallback(std::function<void()>&& cleanupCallback) {
		std::lock_guard<std::mutex> lock(m_submitMutex);
		m_cleanupCallbacks.emplace_back(std::move(cleanupCallback));
		AE_CORE_TRACE("Registered cleanup callback. Total callbacks: {0}", m_cleanupCallbacks.size());
	}

	void VulkanTransferManager::IdleTransferQueue_() {
		VkQueue transferQueue = m_device->GetTransferQueue();
		if (transferQueue != VK_NULL_HANDLE) {
			vkQueueWaitIdle(transferQueue);
		} else {
			vkDeviceWaitIdle(m_device->GetDevice());
		}
	}

	bool VulkanTransferManager::RecreateSignaledFence_() {
		VkDevice logicalDevice = m_device->GetDevice();
		vkDestroyFence(logicalDevice, m_transferFence, nullptr);

		VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_transferFence) != VK_SUCCESS) {
			AE_CORE_CRITICAL("Failed to recreate transfer fence after submit failure.");
			return false;
		}
		return true;
	}

} // namespace AstralEngine
