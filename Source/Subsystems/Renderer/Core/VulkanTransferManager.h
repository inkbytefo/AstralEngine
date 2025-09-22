#pragma once
#include "VulkanDevice.h"
#include <functional>
#include <vector>
#include <mutex>

namespace AstralEngine {

	/**
	 * @class VulkanTransferManager
	 * @brief Manages asynchronous data transfers to the GPU.
	 *
	 * This class provides an efficient way to transfer data (e.g., buffers, textures)
	 * to the GPU using a dedicated transfer queue. It supports batching multiple
	 * transfer operations into a single submission to improve performance. It also
	 * provides a mechanism for immediate, blocking transfers when necessary.
	 *
	 * The manager is thread-safe and designed to be used from multiple threads.
	 */
	class VulkanTransferManager {
	public:
		/**
		 * @brief Constructs a VulkanTransferManager.
		 * @param device A pointer to the VulkanDevice.
		 */
		VulkanTransferManager(VulkanDevice* device);
		~VulkanTransferManager();

		// Non-copyable
		VulkanTransferManager(const VulkanTransferManager&) = delete;
		VulkanTransferManager& operator=(const VulkanTransferManager&) = delete;

		/**
		 * @brief Initializes the transfer manager, creating command pools and fences.
		 */
		void Initialize();

		/**
		 * @brief Shuts down the transfer manager, releasing all Vulkan resources.
		 */
		void Shutdown();

		/**
		 * @brief Queues a generic transfer operation.
		 *
		 * This method allows queueing any sequence of Vulkan commands that should be
		 * executed on the transfer queue. The provided function will be called with a
		 * valid VkCommandBuffer during SubmitTransfers.
		 *
		 * @param transferFunction A function that takes a VkCommandBuffer and records transfer commands.
		 *
		 * @code
		 * transferManager.QueueTransfer([=](VkCommandBuffer cmd) {
		 *     VkBufferCopy copyRegion = {};
		 *     // ... setup copyRegion ...
		 *     vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
		 * });
		 * @endcode
		 */
		void QueueTransfer(std::function<void(VkCommandBuffer)>&& transferFunction);

		/**
		 * @brief Submits all queued transfer operations to the GPU.
		 *
		 * This method records all queued transfer functions into a command buffer
		 * and submits it to the transfer queue. It waits for the previous batch
		 * of transfers to complete before submitting the new one. This is a
		 * non-blocking call from the perspective of the GPU, but it will block
		 * the calling thread until the submission is complete.
		 */
		void SubmitTransfers();

		/**
		 * @brief Acquires a command buffer for immediate, one-time use.
		 *
		 * This method provides a command buffer that can be used for immediate,
		 * blocking transfers. It waits for any ongoing transfers to complete
		 * before providing the command buffer. The caller is responsible for
		 * recording commands and then calling SubmitImmediateCommand.
		 *
		 * @return A valid VkCommandBuffer ready for recording, or VK_NULL_HANDLE on error.
		 */
		VkCommandBuffer GetCommandBufferForImmediateUse();

		/**
		 * @brief Submits a command buffer for immediate execution.
		 *
		 * This method ends recording on the provided command buffer and submits it
		 * to the transfer queue. It then blocks until the transfer operation is
		 * complete.
		 *
		 * @param commandBuffer The command buffer acquired from GetCommandBufferForImmediateUse.
		 */
		void SubmitImmediateCommand(VkCommandBuffer commandBuffer);

		/**
		 * @brief Registers a cleanup callback to be executed after the current batch of transfers completes.
		 *
		 * This method allows registering cleanup operations (such as freeing staging buffers)
		 * that should only be executed after the GPU has completed the transfer operations.
		 * The callback will be executed after the transfer fence signals completion.
		 *
		 * @param cleanupCallback A function to be called after transfer completion.
		 *
		 * @code
		 * transferManager.RegisterCleanupCallback([=]() {
		 *     vkDestroyBuffer(device, stagingBuffer, nullptr);
		 *     vkFreeMemory(device, stagingMemory, nullptr);
		 * });
		 * @endcode
		 */
		void RegisterCleanupCallback(std::function<void()>&& cleanupCallback);

	private:
		void IdleTransferQueue_();
		bool RecreateSignaledFence_();

		VulkanDevice* m_device;
		VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;
		VkCommandBuffer m_transferCommandBuffer = VK_NULL_HANDLE;
		VkFence m_transferFence = VK_NULL_HANDLE;

		std::mutex m_queueMutex;
		std::vector<std::function<void(VkCommandBuffer)>> m_transferQueue;

		std::mutex m_submitMutex;
		std::vector<std::function<void()>> m_cleanupCallbacks;
	};

} // namespace AstralEngine
